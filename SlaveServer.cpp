/*HEADER INCLUDES*/
#include <iostream>
#include <fstream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "include/rapidjson/document.h"
#include <unordered_map>
#include <unistd.h>
#include <vector>
/*HEADER INCLUDES*/

/*MACRO DECLARATIONS*/
#define DOMAIN AF_INET
#define TYPE_TCP SOCK_STREAM
#define TYPE_UDP SOCK_DGRAM     //Using UDP for HeartBeat Msgs
#define PROTOCOL 0
#define BUFF_SIZE 512*1024
#define LEVEL SOL_SOCKET
#define SET_OPTIONS SO_REUSEADDR
#define HEART_BEAT_DURATION 2 //Duration is converted to time
/*MACRO DECLARATIONS*/

/*NAMESPACES DECLARATIONS*/
using namespace std;
using namespace rapidjson;
/*NAMESPACES DECLARATIONS*/

/*GLOBAL DATA STRUCTURES*/
unordered_map<string,string> data_client;
unordered_map<string,string> data_secondary;
/*THREAD MANAGEMENT*/
int migration_count=0;
int thread_count=0;
pthread_mutex_t mutex_sync;
/*THREAD MANAGEMENT*/
/*GLOBAL DATA STRUCTURES*/

string create_json_string(vector<pair<string,string>> &data)
{
    string json_string="{";
    int i=0;
    for(i=0;i<data.size()-1;i++)
    {
        json_string+="\n\t";
        json_string+="\""+data[i].first+"\":\""+data[i].second+"\",";
    }
    json_string+="\n\t";
    json_string+="\""+data[i].first+"\":\""+data[i].second+"\"";
    json_string+="\n}";
    return json_string;
}

int connection_establish(string ip,int port)
{
    int option_set=1;
    struct sockaddr_in  server_addr;
    int sock_fd=socket(DOMAIN,TYPE_TCP,PROTOCOL);
    if(sock_fd<0)
    {
        cout<<"Cannot get FD for socket"<<endl;
        exit(0);
    }
    //Setting to reuse socket again
    //cout<<"Sock FD="<<sock_fd<<endl;
    server_addr.sin_family=DOMAIN;
    server_addr.sin_port=htons(port);
    if(inet_pton(DOMAIN, ip.c_str(), &server_addr.sin_addr)<0)
    {
        cout<<"Problem in converting IP"<<endl;
    }
    int connect_stat=connect(sock_fd,(struct sockaddr *)&server_addr, sizeof(server_addr));
    //cout<<"Connection Status = "<<connect_stat<<endl;
    if(connect_stat<0)
    {
        return -1;
    }
    return sock_fd;
}

int send_sync(int sock_fd,string port_listen)  //return 1 on successfully sending syn packet
{
    string data="{\n\t\"type\":\"slave\",\n\t\"port\":\""+port_listen+"\"\n}";
    const void * data_send = data.c_str();
    int send_stat=send(sock_fd,data_send,data.length(),0);
    if(send_stat<0)
    {
        cout<<"Sending Error"<<endl;
    }
    cout<<"send_stat"<<send_stat<<endl;
    char buff[BUFF_SIZE]={0};
    //cout<<"Reading Chunck Size"<<endl;
    int valread = recv( sock_fd , buff, BUFF_SIZE,0);
    if(valread<0)
    {
        cout<<"Error in Reading SYN ACK"<<endl;
    }
    string response_recv=buff;
    Document document;
    document.Parse(response_recv.c_str());
    data = document["status"].GetString();
    if(strcmp(data.c_str(),"connected")==0)
    {
        send_stat=1;
    }
    else
        send_stat=0;
    //cout<<"Connection Status Received is:"<<response_recv<<endl;
    return send_stat;
}

void* heart_beat_thr(void *sock)
{
    int *sock_par = (int *)(sock);
    int sock_fd = *sock_par;
    clock_t startTime = clock();
    clock_t timePassed;
    //string heart_string = "{\n\t\"purpose\":\"heartbeat\"\n}";
    string heart_string = "1";
    const void * data_send = heart_string.c_str();
    while(true)
    {
        timePassed = clock() - startTime;
        timePassed = (timePassed / (double)CLOCKS_PER_SEC)*1000;
        //cout<<"Duration Passed "<<timePassed<<endl;
        if(timePassed>=HEART_BEAT_DURATION)     //Duration is converted to time
        {
            /*Sending HeartBeat*/
            int send_stat=send(sock_fd,data_send,heart_string.length(),0);
            if(send_stat<0)
            {
                cout<<"HeartBeat Sending Error"<<endl;
            }
            startTime = clock();
            /*Sending HeartBeat*/
        }
    }
}

int set_socket(struct sockaddr_in self_track,int option_set,int port)
{
    int sock_fd=socket(DOMAIN,TYPE_TCP,PROTOCOL);
    if(sock_fd<0)
    {
        cout<<"Cannot get FD for socket"<<endl;
        exit(0);
    }
    //Setting to reuse socket again
    //cout<<"Sock FD="<<sock_fd<<endl;
    if(setsockopt(sock_fd,LEVEL,SET_OPTIONS,(void *)&option_set,sizeof(int))<0)
    {
        cout<<"SETTING OPTION : "<<errno<<endl;
        cout<<"Error in setting option"<<endl;
        exit(0);
    }
    self_track.sin_family=DOMAIN;
    self_track.sin_port=htons(port);
    self_track.sin_addr.s_addr = INADDR_ANY;
    if(bind(sock_fd,(struct sockaddr *)&self_track,sizeof(self_track))<0)
    {
        cout<<"Binding Failed "<<errno<<endl;
    }
    return sock_fd;
}

int getData(int sock_fd , string key)
{
    unordered_map<string,string>::iterator itr = data_client.find(key);
    string response;
    if(itr == data_client.end())
    {
        response = "{\n\t\"value\":\"failure\"\n}";
    }
    else
    {
        response = "{\n\t\"value\":\""+data_client[key]+"\"\n}";
    }
    int send_stat=send(sock_fd,response.c_str(),response.length(),0);
    if(send_stat<0)
    {
        cout<<"Error in sending GET Response"<<endl;
    }
    return 1;
}

int putData(int sock_fd, string key,string value,string addAs)
{
    int adding = stoi(addAs);
    string response;
    unordered_map<string,string>::iterator itr;
    if(adding==1)   //add to primary map
    {
        itr = data_client.find(key);
        if(itr==data_client.end())
        {
            //currently does not exist so can insert
            data_client[key]=value;
            response = "{\n\t\"purpose\":\"put\",\n\t\"value\":\"success\"\n}";
            cout<<"Put Success"<<endl;
        }
        else
        {
            response = "{\n\t\"purpose\":\"put\",\n\t\"value\":\"exists\"\n}";
            cout<<"Already Exists"<<endl;
        }
    }
    else if(adding==0) //add to previous map
    {
        itr = data_secondary.find(key);
        if(itr==data_secondary.end())
        {
            //currently does not exist so can insert
            data_secondary[key]=value;
            response = "{\n\t\"purpose\":\"put\",\n\t\"value\":\"success\"\n}";
            cout<<"Put Success"<<endl;
        }
        else
        {
            response = "{\n\t\"purpose\":\"put\",\n\t\"value\":\"exists\"\n}";
            cout<<"Already Exists"<<endl;
        }
    }
    else
    {
        //Error in Adding No Table Indicated
        response = "{\n\t\"purpose\":\"put\",\n\t\"value\":\"no_proper_map_specified\"\n}";
    }
    int send_stat=send(sock_fd,response.c_str(),response.length(),0);
    if(send_stat<0)
    {
        cout<<"Error in sending PUT Response"<<endl;
    }
    cout<<"Put Response "<<response<<endl;
    return 1;
}

int updateData(int sock_fd,string key,string value,string addAs)
{
    int adding = stoi(addAs);
    unordered_map<string,string>::iterator itr;
    vector<pair<string,string>> response_create;
    response_create.push_back(make_pair("purpose","update"));
    string response;
    if(adding==1)
    {
        itr = data_client.find(key);
        if(itr == data_client.end())
        {
            //data does not exist
            data_client[key] = value;
            response_create.push_back(make_pair("value","added"));
        }
        else
        {
            data_client[key] = value;
            response_create.push_back(make_pair("value","exists"));
        }
    }
    else if(adding==0)
    {
        itr = data_secondary.find(key);
        if(itr == data_secondary.end())
        {
            //data does not exist
            data_secondary[key] = value;
            response_create.push_back(make_pair("value","added"));
        }
        else
        {
            data_secondary[key] = value;
            response_create.push_back(make_pair("value","exists"));
        }
    }
    else
    {
        response_create.push_back(make_pair("value","no_proper_map_specified"));
    }
    response = create_json_string(response_create);
    int send_stat=send(sock_fd,response.c_str(),response.length(),0);
    if(send_stat<0)
    {
        cout<<"Error in sending UPDATE Response"<<endl;
    }
    return 1;
}

int deleteData(int sock_fd, string key,string addAs)
{
    int adding = stoi(addAs);
    unordered_map<string,string>::iterator itr;
    string response;
    vector<pair<string,string>> response_create;
    response_create.push_back(make_pair("purpose","delete"));
    if(adding==1)
    {
        itr = data_client.find(key);
        if(itr == data_client.end())
        {
            //data does not exist
            response_create.push_back(make_pair("value","nexists"));
        }
        else
        {
            data_client.erase(itr);
            response_create.push_back(make_pair("value","success"));
        }
    }
    else if(adding==0)
    {
        itr = data_secondary.find(key);
        if(itr == data_secondary.end())
        {
            //data does not exist
            response_create.push_back(make_pair("value","nexists"));
        }
        else
        {
            data_secondary.erase(itr);
            response_create.push_back(make_pair("value","success"));
        }
    }
    else
    {
        response_create.push_back(make_pair("value","no_proper_map_specified"));
    }
    response = create_json_string(response_create);
    int send_stat=send(sock_fd,response.c_str(),response.length(),0);
    if(send_stat<0)
    {
        cout<<"Error in sending DELETE Response"<<endl;
    }
    return 1;
}

int handle_migration_thread(string command,int sock_fd)
{
    cout<<"Migration Thread Hit"<<endl;
    pthread_mutex_lock(&mutex_sync);
        migration_count++;
        while(thread_count>0);  //Wait for current thread to exit
        /*Normal Exec*/
        
        /*Normal Exec*/
        migration_count--;
    pthread_mutex_unlock(&mutex_sync);
}

int normal_thread(string command,int sock_fd)
{
    Document document;
    document.Parse(command.c_str());
    if(migration_count>0)
    {
        sleep(300);
        cout<<"New Thread and Some Migration in Process"<<endl;
        //Some Migration Thread is Running
        pthread_mutex_lock(&mutex_sync);
        thread_count++;
        /*Normal Execution*/
            if(strcmp(document["purpose"].GetString(),"termination")==0)
            {
                //termination of connection
                close(sock_fd);
            }
            else if(strcmp(document["purpose"].GetString(),"get")==0)
            {
                while(getData(sock_fd,document["key"].GetString())!=1);
            }
            else if(strcmp(document["purpose"].GetString(),"put")==0)
            {
                while(putData(sock_fd,document["key"].GetString(),document["value"].GetString(),document["addAs"].GetString())!=1);
            }
            else if(strcmp(document["purpose"].GetString(),"update")==0)
            {
                while(updateData(sock_fd,document["key"].GetString(),document["value"].GetString(),document["addAs"].GetString())!=1);
            }
            else if(strcmp(document["purpose"].GetString(),"delete")==0)
            {
                while(deleteData(sock_fd,document["key"].GetString(),document["addAs"].GetString())!=1);
            }
        /*Normal Execution*/
        thread_count--;
        pthread_mutex_unlock(&mutex_sync);
    }
    else
    {
        sleep(300);
        cout<<"New Thread and NO Migration in Process"<<endl;
        //No other Migration Thread Running
        thread_count++;
        /*Normal Execution*/
            if(strcmp(document["purpose"].GetString(),"termination")==0)
            {
                //termination of connection
                close(sock_fd);
            }
            else if(strcmp(document["purpose"].GetString(),"get")==0)
            {
                while(getData(sock_fd,document["key"].GetString())!=1);
            }
            else if(strcmp(document["purpose"].GetString(),"put")==0)
            {
                while(putData(sock_fd,document["key"].GetString(),document["value"].GetString(),document["addAs"].GetString())!=1);
            }
            else if(strcmp(document["purpose"].GetString(),"update")==0)
            {
                while(updateData(sock_fd,document["key"].GetString(),document["value"].GetString(),document["addAs"].GetString())!=1);
            }
            else if(strcmp(document["purpose"].GetString(),"delete")==0)
            {
                while(deleteData(sock_fd,document["key"].GetString(),document["addAs"].GetString())!=1);
            }
        /*Normal Execution*/
        thread_count--;
    }
}

void* request_process(void* accept_stat)
{
    int* accept_sta = (int *) accept_stat;
    int sock_fd = (*accept_sta);
    char buff[BUFF_SIZE]={0};
    int valread = recv( sock_fd , buff, BUFF_SIZE,0);
    if(valread<0)
    {
        cout<<"Error in Reading SYN ACK"<<endl;
    }
    string response_recv=buff;
    Document document;
    document.Parse(response_recv.c_str());
    /*THREAD TYPE AND PROCESS ACC.*/
    if(strcmp(document["purpose"].GetString(),"migration")==0)
    {
        //Migration Thread
        handle_migration_thread(response_recv,sock_fd);
    }
    else
    {
        //Normal Thread
        normal_thread(response_recv,sock_fd);
    }
    /*THREAD TYPE AND PROCESS ACC.*/
    /*
    if(strcmp(document["purpose"].GetString(),"termination")==0)
    {
        //termination of connection
        close(sock_fd);
    }
    else if(strcmp(document["purpose"].GetString(),"get")==0)
    {
        while(getData(sock_fd,document["key"].GetString())!=1);
    }
    else if(strcmp(document["purpose"].GetString(),"put")==0)
    {
        while(putData(sock_fd,document["key"].GetString(),document["value"].GetString(),document["addAs"].GetString())!=1);
    }
    else if(strcmp(document["purpose"].GetString(),"update")==0)
    {
        while(updateData(sock_fd,document["key"].GetString(),document["value"].GetString(),document["addAs"].GetString())!=1);
    }
    else if(strcmp(document["purpose"].GetString(),"delete")==0)
    {
        while(deleteData(sock_fd,document["key"].GetString(),document["addAs"].GetString())!=1);
    }
    */
}

int main()
{
    mutex_sync = PTHREAD_MUTEX_INITIALIZER;
    string path,ip,port_listen;
    int port,sock_fd,sock_fd_listen,heart_thread,int_port_listen,option_set=1;
    struct sockaddr_in self_track;
    struct sockaddr_in new_connection;
    socklen_t new_connection_size;
    cout<<"Enter Path Of Co-Ordination Server Data"<<endl;
    cin>>path;
    fstream co_ord_data(path);
    co_ord_data>>ip;
    port_listen = (ip);
    co_ord_data>>ip;
    port = stoi(ip);
    co_ord_data>>ip;
    /*OPEN CONNECTION*/
    sock_fd = connection_establish(ip,port);
    /*OPEN CONNECTION*/
    /*SYNC MSG*/
    while(send_sync(sock_fd,port_listen)!=1);
    cout<<"Connection Established!!"<<endl;
    /*SYNC MSG*/
    /*Creating Different Thread for HeartBeat*/
    pthread_t heart_beat;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
    heart_thread = pthread_create(&heart_beat,&attr,heart_beat_thr,(void *)&sock_fd);
    /*Creating Different Thread for HeartBeat*/
    /*Listening Co-Ordination Server for Requests*/

    int_port_listen = stoi(port_listen);
    sock_fd_listen = set_socket(self_track,option_set,int_port_listen); 
    if(listen(sock_fd_listen,SOMAXCONN)<0) //SOMAXCONN is max connections to queue
    {
        cout<<"Listening Error"<<endl;
    }
    while(true)
    {
        int accept_stat=accept(sock_fd_listen, (struct sockaddr*)&new_connection,&new_connection_size);
        pthread_t service_request;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
        service_request = pthread_create(&service_request,&attr,request_process,(void *)&accept_stat);
    }
    /*Listening Co-Ordination Server for Requests*/
    return 0;
}
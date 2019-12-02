#include<iostream>
#include<fstream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>
#include <vector>
#include "/home/manish/Piazza/include/rapidjson/document.h"

#define DOMAIN AF_INET
#define TYPE SOCK_STREAM
#define PROTOCOL 0
#define BUFF_SIZE 512*1024
#define LEVEL SOL_SOCKET
#define SET_OPTIONS SO_REUSEADDR

using namespace std;
using namespace rapidjson;

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
    int sock_fd=socket(DOMAIN,TYPE,PROTOCOL);
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

int recv_response(int sock_fd,string &response)
{
    char buff[BUFF_SIZE]={0};
    int valread = recv( sock_fd , buff, BUFF_SIZE,0);
    if(valread<0)
    {
        cout<<"Error in Reading SYN ACK"<<endl;
    }
    response=buff;
    return valread;
}

int send_command(int sock_fd,string command)    //return 1 on successfully sending command
{
    const void * data_send = command.c_str();
    int send_stat=send(sock_fd,data_send,command.length(),0);
    if(send_stat<0)
    {
        cout<<"Sending Error"<<endl;
    }
    return send_stat;
}

int logout(string username,int sock_fd) //On Successful Logout Return 1
{

}

int login(string key,string value,int sock_fd)
{
    string send_data;
    int stat=0;
    vector<pair<string,string>> command;
    command.push_back(make_pair("purpose","login"));
    command.push_back(make_pair("username",key));
    command.push_back(make_pair("password",value));
    send_data = create_json_string(command);
    while(send_command(sock_fd,send_data)<0);
    while(recv_response(sock_fd,send_data)<0);
    Document document;
    document.Parse(send_data.c_str());
    send_data = document["status"].GetString();
    if(strcmp(send_data.c_str(),"success")==0)
    {
        cout<<"Login Sucess!!"<<endl;
        stat=1;
    }
    else if(strcmp(send_data.c_str(),"invalid")==0)
    {
        cout<<"Enter Proper Password"<<endl;
        stat=1;
    }
    else if(strcmp(send_data.c_str(),"failure")==0)
    {
        cout<<"Login Failed!!\nNo Such User"<<endl;
        stat=1;
    }
    return stat;
}

int send_syn(int sock_fd)  //return 1 on successfully sending syn packet
{
    string data="{\n\t\"type\":\"client\"\n}";
    const void * data_send = data.c_str();
    int send_stat=send(sock_fd,data_send,data.length(),0);
    if(send_stat<0)
    {
        cout<<"Sending Error"<<endl;
    }
    //cout<<"send_stat"<<send_stat<<endl;
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

int get_data(string key,int sock_fd,string &value)
{
    vector<pair<string,string>> command;
    command.push_back(make_pair("purpose","get"));
    command.push_back(make_pair("key",key));
    key = create_json_string(command);
    while(send_command(sock_fd,key)<0);
    while(recv_response(sock_fd,key)<0);
    Document document;
    document.Parse(key.c_str());
    value = document["value"].GetString();
    if(strcmp(value.c_str(),"failure")==0)
    {
        cout<<"Get Failure!!"<<endl;
    }
    else
    {
        cout<<"Value is:"<<value<<endl;
    }
    return 1;
}

int create_user(string key,string value,int sock_fd)
{
    vector<pair<string,string>> command;
    command.push_back(make_pair("purpose","create_user"));
    command.push_back(make_pair("username",key));
    command.push_back(make_pair("password",value));
    key = create_json_string(command);
    while(send_command(sock_fd,key)<0);
    while(recv_response(sock_fd,key)<0);
    Document document;
    document.Parse(key.c_str());
    value = document["value"].GetString();
    if(strcmp(value.c_str(),"exists")==0)
    {
        cout<<"User Already Exist!!"<<endl;
    }
    else if(strcmp(value.c_str(),"success")==0)
    {
        cout<<"User Created Successfully!!"<<endl;
    }
    return 1;
}

int delete_data(string key,int sock_fd)
{
    vector<pair<string,string>> command;
    command.push_back(make_pair("purpose","delete"));
    command.push_back(make_pair("key",key));
    command.push_back(make_pair("addAs","-1"));
    key = create_json_string(command);
    while(send_command(sock_fd,key)<0);
    while(recv_response(sock_fd,key)<0);
    Document document;
    document.Parse(key.c_str());
    key = document["value"].GetString();
    if(strcmp(key.c_str(),"success")==0)
    {
        cout<<"SuccessFully Deleted!!"<<endl;
    }
    else if(strcmp(key.c_str(),"failure")==0)
    {
        cout<<"Delete Failed!!"<<endl;
    }
    else if(strcmp(key.c_str(),"nexists")==0)
    {
        cout<<"Key Does Not Exists!!"<<endl;
    }
    return 1;
}

int put_data(int sock_fd,string key,string value)
{
    vector<pair<string,string>> command;
    command.push_back(make_pair("purpose","put"));
    command.push_back(make_pair("key",key));
    command.push_back(make_pair("value",value));
    command.push_back(make_pair("addAs","-1"));
    key = create_json_string(command);
    while(send_command(sock_fd,key)<0);
    while(recv_response(sock_fd,key)<0);
    Document document;
    document.Parse(key.c_str());
    key = document["value"].GetString();
    if(strcmp(key.c_str(),"success")==0)
    {
        cout<<"Tuple Added!!"<<endl;
        return 1;
    }
    else if(strcmp(key.c_str(),"exists")==0)
    {
        cout<<"Tuple with given key already exists!!"<<endl;
        return 1;
    }
    else
    {
        cout<<"Error occured while inserting"<<endl;
        return 1;
    }
}

int update_data(int sock_fd,string key,string value)
{
    vector<pair<string,string>> command;
    command.push_back(make_pair("purpose","update"));
    command.push_back(make_pair("key",key));
    command.push_back(make_pair("value",value));
    command.push_back(make_pair("addAs","-1"));
    key = create_json_string(command);
    while(send_command(sock_fd,key)<0);
    while(recv_response(sock_fd,key)<0);
    Document document;
    document.Parse(key.c_str());
    key = document["value"].GetString();
    if(strcmp(key.c_str(),"exists")==0)
    {
        cout<<"Tuple Updated!!"<<endl;
        return 1;
    }
    else if(strcmp(key.c_str(),"added")==0)
    {
        cout<<"Tuple Added!!"<<endl;
        return 1;
    }
    else
    {
        cout<<"Error occured while updating!!"<<endl;
        return 1;
    }

}

int main()
{
    string path,ip,command,username,key,value;
    int port,sock_fd;
    cout<<"Enter Config File Path:"<<endl;
    cin>>path;
    fstream details(path);
    details>>ip;
    port = stoi(ip);
    details>>ip;
    //cout<<"IP is:"<<ip<<" Port is:"<<port<<endl;
    sock_fd=connection_establish(ip,port);
    //cout<<"Sock_fd"<<sock_fd<<endl;
    if(sock_fd<0)
    {
        //Connection Establish Failed
        cout<<"Problem in Connection Establishment"<<endl;
        return 1;
    }
    /*Send SYN Packet*/
    while(send_syn(sock_fd)<0);
    /*Send SYN Packet*/
    //Connection Properly Established
    cout<<"Enter Command"<<endl;
    getline(cin,command);
    while(strcmp("logout",command.c_str())!=0)
    {
        /*Command Processing*/
        if(command.compare(0,5,"login")==0)  //login username password
        {
            /*Record UserName*/
            stringstream get_user(command);
            getline(get_user, username, ' ');
            getline(get_user, username, ' ');
            key=username;
            getline(get_user, value, ' ');
            //cout<<"UserName is:"<<username<<endl;
            /*Record UserName*/
            while(login(key,value,sock_fd)!=1);
        }
        else if(command.compare(0,11,"create_user")==0) //create_user username password
        {
            stringstream get_user(command);
            getline(get_user, key, ' ');
            getline(get_user, key, ' ');
            getline(get_user, value, ' ');
            while(create_user(key,value,sock_fd)!=1);
        }
        else if(command.compare(0,3,"get")==0)  //get key
        {
            stringstream get_user(command);
            getline(get_user, key, ' ');
            getline(get_user, key, ' ');
            while(get_data(key,sock_fd,value)!=1);
        }
        else if(command.compare(0,3,"put")==0)  //put key value
        {
            stringstream get_user(command);
            getline(get_user, key, ' ');
            getline(get_user, key, ' ');
            getline(get_user, value, ' ');
            while(put_data(sock_fd,key,value)!=1);
        }
        else if(command.compare(0,6,"update")==0)   //update key new_value
        {
            stringstream get_user(command);
            getline(get_user,key,' ');
            getline(get_user,key,' ');
            getline(get_user,value,' ');
            while(update_data(sock_fd,key,value)!=1);
        }
        else if(command.compare(0,6,"delete")==0)   //delete key
        {
            stringstream get_user(command);
            getline(get_user, key, ' ');
            getline(get_user, key, ' ');
            while(delete_data(key,sock_fd)!=1);
        }
        /*Command Processing*/
        cout<<"Enter Command"<<endl;
        getline(cin,command);
    }
    /*Logout Function*/
    while(logout(username,sock_fd)!=1);
    /*Logout Function*/
    return 0;
}


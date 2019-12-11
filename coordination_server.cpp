#include "coordination_server.hpp"

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int CCoord_server::alwaysListen()
{
    ifstream fin("coord_info.txt");
    if(!fin)
    {
        cout<<"Error openeing coord_info.txt";
        return -1;
    }
    // read coord. server's ip and port num from file
    while(!fin.eof())
    {
        string ip, port;
        fin>>ip>>port;
        if(fin.eof())
            break;
        coord_info.IPaddr=ip;
        coord_info.portnum=port;
    }

    int sockfd;  // listen on sock_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;    
    hints.ai_socktype = SOCK_STREAM;    // stream protocol, TCP
    hints.ai_flags = AI_PASSIVE; // use machine's IP

    // get all the addrinfo structures, each of which contains an Internet address that can be specified in a call to bind() or connect()
    if ((rv = getaddrinfo(NULL, coord_info.portnum.c_str(), &hints, &servinfo)) != 0) 
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) 
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int)) == -1) 
        {
            perror("setsockopt");
            return -1;
            //exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) 
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        //cout<<((struct sockaddr_in*)p->ai_addr)->sin_port<<endl;
        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  
    {
        fprintf(stderr, "server: failed to bind\n");
        return -1;
        //exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) 
    {
        perror("listen");
        return -1;
        //exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) 
    {
        perror("sigaction");
        return -1;
        //exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) 
    {  // main accept() loop
        sin_size = sizeof their_addr;
        int *new_fd = new int;      // new connection on new_fd
        *new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (*new_fd == -1) 
        {
            perror("accept");
            //continue;
        }

        inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr),s, sizeof s);
        printf("server: got connection from %s\n", s);
        //string connectionIP (s);
        string senderPort = to_string(ntohs(((struct sockaddr_in *)&their_addr)->sin_port));
        char buf[BUFFERSIZE];
        int numbytes;
        // First receive
        memset(buf,0,BUFFERSIZE);
        if ((numbytes = recv(*new_fd, buf, BUFFERSIZE, 0)) == -1) 
        {
            perror("recv");
            return -1;
            //exit(1);
        }
        // JSON parsing
        cout<<buf<<endl;
        Document document;
        document.Parse(buf);
        string type=document["type"].GetString();
        
        int ty;
        if(type=="client")
        {
            //cout<<"client: "<<*new_fd<<endl;
            string sendstr="{\n\t\"status\":\"connected\"\n}";
            if(send(*new_fd, sendstr.c_str(), sendstr.length(), 0) == -1)
                perror("send");
            thread typethread(&CCoord_server::clientHandle, this, *new_fd);
            typethread.detach();
        }
        else if(type=="slave")
        {
            string slaveIP(s);
            string slavePort = document["port"].GetString();;
            cout<<slaveIP<<" "<<slavePort<<endl;
            thread typethread(&CCoord_server::slaveHandle, this, slaveIP, slavePort, *new_fd);
            typethread.detach();
        }
        
    }
}

int CCoord_server::clientHandle(int fd)
{
    int numbytes;
    // First receive
    Document document;
    while(1)
    {
        char buf[BUFFERSIZE];
        memset(buf,0,BUFFERSIZE);
        if ((numbytes = recv(fd, buf, BUFFERSIZE, 0)) == -1) 
        {
            perror("recv");
            return -1;
            //exit(1);
        }
        string tmp(buf);
        cout<<tmp<<endl<<endl<<endl;
        document.Parse(tmp.c_str());
        assert(document.HasMember("purpose"));
        assert(document["purpose"].IsString());
        string purpose=document["purpose"].GetString();
        if(purpose=="create_user")
        {
            assert(document.HasMember("username"));
            assert(document["username"].IsString());
            string username = document["username"].GetString();
            assert(document.HasMember("password"));
            assert(document["password"].IsString());
            string password = document["password"].GetString();
            create_user(username, password, fd);
        }
        else if(purpose=="login")
        {
            assert(document.HasMember("username"));
            assert(document["username"].IsString());
            string username = document["username"].GetString();
            assert(document.HasMember("password"));
            assert(document["password"].IsString());
            string password = document["password"].GetString();
            login(username, password, fd);
        }
        else if(purpose=="get")
        {
            string bufstr(buf);
            getData(bufstr, fd);    //passing file desc and command string
        }
        else if(purpose=="put")
        {
            string bufstr(buf);
            putData(bufstr, fd);
        }
        else if(purpose=="delete")
        {
            string bufstr(buf);
            deleteData(bufstr, fd);
        }
        else if(purpose=="update")
        {
            string bufstr(buf);
            updateData(bufstr, fd);
        }
    }
}
int CCoord_server::slaveHandle(string slaveIP, string slavePort, int fd)
{
    // Receive port num
    char buf[BUFFERSIZE];
    memset(buf,0,BUFFERSIZE);
    int numbytes;
    //if ((numbytes = recv(fd, buf, BUFFERSIZE, 0)) == -1) 
    //{
    //    perror("recv");
    //    return -1;
    //    //exit(1);
    //}
    //string slaveport(buf,numbytes);
    //memset(buf,0,BUFFERSIZE);

    slaveData* newSlave = new slaveData;
    newSlave->IPaddr = slaveIP;
    newSlave->portnum = slavePort;
    newSlave->hashvalue = hashSlave(newSlave);
    newSlave->isActive = true;
    cout<<slaveIP<<": "<<newSlave->hashvalue<<endl;
    insertBST(&treeRoot,newSlave);
    inorder(treeRoot);
    cout<<endl;
    std::pair<std::map<string,slaveData>::iterator,bool> ret;
    ret =  slavemap.insert(make_pair(newSlave->IPaddr, *newSlave));
    
    if (ret.second==false) 
    {
        // If element already exists
        //----Initiate migration-----//
        //migrationInitUp(newSlave);
        string sendstr="{\n\t\"status\":\"connected\"\n}";
        if(send(fd, sendstr.c_str(), sendstr.length(), 0) == -1)
            perror("send");
        return 0;
    }
    else
    {
        string sendstr="{\n\t\"status\":\"connected\"\n}";
        if(send(fd, sendstr.c_str(), sendstr.length(), 0) == -1)
            perror("send");
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    clock_t startTime = clock(); //Start timer
    double secondsPassed;
    double secondsToDelay = SECTODELAY;
    while(1)
    {
        memset(buf,0,BUFFERSIZE);
        //sleep(2);
        if ((numbytes = recv(fd, buf, BUFFERSIZE, 0)) == -1) 
        {
            perror("recv");
            //return -1;
            //exit(1);
        }
        if(numbytes>0)
        {
            //puts(buf);
            startTime=clock();
        }
        else
            cout<<"No receive from: "<<newSlave->IPaddr<<endl;
        secondsPassed = ((double)(clock() - startTime) / CLOCKS_PER_SEC)*1000;
        //cout<<clock()<<" "<<startTime<<endl;
        if(secondsPassed >= secondsToDelay)
        {
            cout<<"Slave down. Socket addr: "<<newSlave->IPaddr<<":"<<newSlave->portnum<<endl<<"Initiating migration..."<<endl;
            migrationInitDown(newSlave);
            close(fd);
            break;
        }
        sleep(2);
        //listenHeartbeat()
    }
}

int CCoord_server::create_user(string username, string password, int sock_fd)
{
    clientData *newClient = new clientData;
    newClient->username = username;
    newClient->password = password;
    //newClient->IPaddr = IPaddr;
    //newClient->portnum = portnum;
    newClient->isActive = false;

    std::pair<std::map<string,clientData>::iterator,bool> ret;
    ret =  clientmap.insert(make_pair(username, *newClient));
    if (ret.second==false) 
    {
        // If element already exists
        vector<pair<string,string>> sendjson;
        sendjson.push_back(make_pair("value","exists"));
        string tosend = create_json_string(sendjson);
        if(send(sock_fd, tosend.c_str(), tosend.length(), 0) == -1)
            perror("send");
        return 0;
    }
    else
    {
        vector<pair<string,string>> sendjson;
        sendjson.push_back(make_pair("value","success"));
        string tosend = create_json_string(sendjson);
        if(send(sock_fd, tosend.c_str(), tosend.length(), 0) == -1)
            perror("send");
        return 1;
    }
}

int CCoord_server::login(string username, string password, int sock_fd)
{
    map<string, clientData>:: iterator itr;
    if((itr = clientmap.find(username)) != clientmap.end())
    {
        if(itr->second.password==password)
        {
            //itr->second.IPaddr=IPaddr;
            //itr->second.password=password;
            itr->second.isActive=true;
            vector<pair<string,string>> sendjson;
            sendjson.push_back(make_pair("status","success"));
            string tosend = create_json_string(sendjson);
            if(send(sock_fd, tosend.c_str(), tosend.length(), 0) == -1)
                perror("send");
            return true;    // User present, might not be active;
        }
        else
        {
            vector<pair<string,string>> sendjson;
            sendjson.push_back(make_pair("status","invalid"));
            string tosend = create_json_string(sendjson);
            if(send(sock_fd, tosend.c_str(), tosend.length(), 0) == -1)
                perror("send");
            return false;
        }
    } 
    else
    {
        vector<pair<string,string>> sendjson;
        sendjson.push_back(make_pair("status","failure"));
        string tosend = create_json_string(sendjson);
        if(send(sock_fd, tosend.c_str(), tosend.length(), 0) == -1)
            perror("send");
        return false;   // user not registered with tracker
    }
}

int CCoord_server::getData(string bufstr, int client_fd)
{
    int slave_fd, numbytes;
    char buf[BUFFERSIZE];
    Document document;
    document.Parse(bufstr.c_str());
    assert(document.HasMember("key"));
    assert(document["key"].IsString());
    string key = document["key"].GetString();
    char* str = new char[key.length()+1];
    strcpy(str, key.c_str());
    Fnv32_t hash_val = fnv_32_str(str, FNV1_32_INIT);
    bstNode* targetNode = bst_upperBound(treeRoot, hash_val);

    if(targetNode == NULL)      // If hash value of key is greater than the last slave server, search slave with smallest hash value
    {
        targetNode = findMinimum(treeRoot);
    }
#if CACHE_ENABLE  
    string cachestr = targetNode->data.cache.getValue(key);

    if(cachestr != "none")
    {
        string response = "{\n\t\"value\":\""+cachestr+"\"\n}";
        if(send(client_fd, response.c_str(), response.length(), 0) == -1)
            perror("send");
    }
    else
#endif
    {
        if(targetNode->data.isActive)
        {
            slave_fd = connect_to_slave(&targetNode->data);
            if(send(slave_fd, bufstr.c_str(), bufstr.length(), 0) == -1)
                perror("send");
            memset(buf,0,BUFFERSIZE);
            if ((numbytes = recv(slave_fd, buf, BUFFERSIZE, 0)) == -1) 
            {
                perror("recv");
                return -1;
                //exit(1);
            }
        }
        else
        {
            cout<<"Target slave down"<<endl;
        }
        if(send(client_fd, buf, numbytes, 0) == -1)
            perror("send");
    }
}   

int CCoord_server::putData(string bufstr, int client_fd)
{
    bstNode* primary = put_update_delete_handle(bufstr, client_fd);
#if CACHE_ENABLE
    if(primary != NULL)
    {
        Document document_p;
        document_p.Parse(bufstr.c_str());
        assert(document_p.HasMember("key"));
        assert(document_p["key"].IsString());
        string key = document_p["key"].GetString();

        assert(document_p.HasMember("value"));
        assert(document_p["value"].IsString());
        string value = document_p["value"].GetString();
        primary->data.cache.putInSet(key, value);
    }
    else
    {
        cout<<"Primary node NULL for putting data"<<endl;
    }
#endif
}

int CCoord_server::deleteData(string bufstr, int client_fd)
{
    bstNode* primary = put_update_delete_handle(bufstr, client_fd);
#if CACHE_ENABLE
    if(primary != NULL)
    {
        Document document_p;
        document_p.Parse(bufstr.c_str());
        assert(document_p.HasMember("key"));
        assert(document_p["key"].IsString());
        string key = document_p["key"].GetString();
        primary->data.cache.deleteKey(key);
    }
    else
    {
        cout<<"Primary node NULL for deleting data"<<endl;
    }
#endif
}

int CCoord_server::updateData(string bufstr, int client_fd)
{
    bstNode* primary = put_update_delete_handle(bufstr, client_fd);
#if CACHE_ENABLE
    if(primary !=NULL)
    {
        Document document_p;
        document_p.Parse(bufstr.c_str());
        assert(document_p.HasMember("key"));
        assert(document_p["key"].IsString());
        string key = document_p["key"].GetString();
        primary->data.cache.deleteKey(key);

        assert(document_p.HasMember("value"));
        assert(document_p["value"].IsString());
        string value = document_p["value"].GetString();
        primary->data.cache.putInSet(key, value);
    }
    else
    {
        cout<<"Primary node NULL for updating data"<<endl;
    }
#endif
}   

int CCoord_server::connect_to_slave(slaveData* slave)
{
    int sockfd, numbytes;  
    char buf[BUFFERSIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char* server_addr = new char[slave->IPaddr.size()+1];
    strcpy(server_addr,slave->IPaddr.c_str());
    char* server_port = new char[slave->portnum.size()+1];
    strcpy(server_port, slave->portnum.c_str());
    if ((rv = getaddrinfo(server_addr, server_port, &hints, &servinfo)) != 0) 
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) 
        {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) 
        {
            close(sockfd);
            perror("client: connect");
            cout<<"Slave IP: "<<slave->IPaddr<<endl;
            continue;
        }

        break;
    }

    if (p == NULL) 
    {
        fprintf(stderr, "client: failed to connect\n");
        return -2;
    }
    //string senderPort = to_string(ntohs(((struct sockaddr_in *)p->ai_addr)->sin_port));
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),s, sizeof s);
    printf("client: connecting to %s:%s\n", s, server_port);

    freeaddrinfo(servinfo); // all done with this structure
    
    return sockfd;
}

bstNode* CCoord_server::put_update_delete_handle(string bufstr, int client_fd)
{
    int primary_fd, secondary_fd,numbytes1, numbytes2;
    char primaryResp[BUFFERSIZE], secondaryResp[BUFFERSIZE];
    Document document_p, document_s;
    document_p.Parse(bufstr.c_str());
    assert(document_p.HasMember("key"));
    assert(document_p["key"].IsString());
    string key = document_p["key"].GetString();
    char* str = new char[key.length()+1];
    strcpy(str, key.c_str());
    Fnv32_t hash_val = fnv_32_str(str, FNV1_32_INIT);

    bstNode* primaryNode = bst_upperBound(treeRoot, hash_val);
    if(primaryNode == NULL)      // If hash value of key is greater than the last slave server, search slave with smallest hash value
    {
        primaryNode = findMinimum(treeRoot);
    }
    assert(document_p.HasMember("addAs"));
    assert(document_p["addAs"].IsString());
    document_p["addAs"] = "1";
    //cout<<document_p["addAs"].GetString()<<endl;
    bstNode* secondaryNode;
    findSuccessor(treeRoot, secondaryNode, primaryNode->data.hashvalue);
    if(secondaryNode == NULL)      // If hash value of key is greater than the last slave server, search slave with smallest hash value
    {
        secondaryNode = findMinimum(treeRoot);
    }
    document_s.Parse(bufstr.c_str());
    assert(document_s.HasMember("addAs"));
    assert(document_s["addAs"].IsString());
    document_s["addAs"] = "0";
    //cout<<document_s["addAs"].GetString()<<endl;

    
 
    StringBuffer p_buffer;
    Writer<StringBuffer> p_writer(p_buffer);
    document_p.Accept(p_writer);
    const char* p_output = p_buffer.GetString();
    string p_Sendbuf(p_output);

    StringBuffer s_buffer;
    Writer<StringBuffer> s_writer(s_buffer);
    document_s.Accept(s_writer);
    const char* s_output = s_buffer.GetString();
    string s_Sendbuf(s_output);

    if(primaryNode->data.isActive && secondaryNode->data.isActive)
    {
        memset(primaryResp,0,BUFFERSIZE);
        memset(secondaryResp,0,BUFFERSIZE);
        primary_fd = connect_to_slave(&(primaryNode->data));
        secondary_fd = connect_to_slave(&(secondaryNode->data));
        //cout<<"prim: "<<primary_fd<<"\tsec: "<<secondary_fd<<"\tclient_fd: "<<endl;
        thread primarythread(&CCoord_server::data_modify_ThreadFn, this, p_Sendbuf, primaryResp, &numbytes1, primary_fd);
        thread secondarythread(&CCoord_server::data_modify_ThreadFn, this, s_Sendbuf, secondaryResp, &numbytes2, secondary_fd);
        primarythread.join();
        secondarythread.join();
    }
    else
    {
        if(!primaryNode->data.isActive)
            cout<<"Primary slave down"<<endl;
        if(!secondaryNode->data.isActive)
            cout<<"Secondary slave down"<<endl;
        return NULL;
    }
    if(send(client_fd, primaryResp, numbytes1, 0) == -1)
        perror("send");
    
    return primaryNode;
}
int CCoord_server::data_modify_ThreadFn(string bufstr, char* response, int* numbytes, int fd)
{
    if(send(fd, bufstr.c_str(), bufstr.length(), 0) == -1)
        perror("send");
    
    memset(response,0,BUFFERSIZE);
    if ((*numbytes = recv(fd, response, BUFFERSIZE, 0)) == -1) 
    {
        perror("recv");
        return -1;
        //exit(1);
    }
    return 1;
}

int CCoord_server::migrationInitDown(slaveData* slave_down)
{

    bstNode* pre=NULL, *succ=NULL, *succsucc=NULL;
    char buf[BUFFERSIZE];
    int numbytes;
    //findPreSuc(treeRoot, pre, succ, slave_down->hashvalue);
    findPredecessor(treeRoot, pre, slave_down->hashvalue);
    if(pre==NULL)
        pre = findMaximum(treeRoot);
    treeRoot = deleteBST(&treeRoot, slave_down->hashvalue);
    succ = bst_upperBound(treeRoot, slave_down->hashvalue);
    if(succ==NULL)
        succ = findMinimum(treeRoot);
    findSuccessor(treeRoot, succsucc, succ->data.hashvalue);
    if(succsucc==NULL)
        succsucc = findMinimum(treeRoot);

    cout<<"pred: "<<pre->data.IPaddr;
    cout<<" succ : "<<succ->data.IPaddr;
    cout<<" succsucc: "<<succsucc->data.IPaddr<<endl;
    int pred_fd, succ_fd, succsucc_fd;


    // Step 1: Connect to succsucc and ask it to GET data from succ to update it's own PREV
    succsucc_fd = connect_to_slave(&(succsucc->data));
    vector<pair<string,string>> sendjson;
    sendjson.push_back(make_pair("purpose","migration"));
    sendjson.push_back(make_pair("task","get"));
    sendjson.push_back(make_pair("ip",succ->data.IPaddr));
    sendjson.push_back(make_pair("port",succ->data.portnum));
    sendjson.push_back(make_pair("what","prev"));
    sendjson.push_back(make_pair("addTo","prev"));
    string tosend = create_json_string(sendjson);
    if(send(succsucc_fd, tosend.c_str(), tosend.length(), 0) == -1)
        perror("send");
    //memset(buf,0,BUFFERSIZE);
    //if ((numbytes = recv(succsucc_fd, buf, BUFFERSIZE, 0)) == -1) 
    //{
    //    perror("recv");
    //    return -1;
    //    //exit(1);
    //}
    sleep(4);
    cout<<"Migration: Step-1 complete"<<endl;
    close(succsucc_fd);

    // Step 2: Ask succ to merge its PREV table with its CURR
    // Step 3: Ask succ to GET pred's CURR to update its PREV
    succ_fd = connect_to_slave(&(succ->data));
    sendjson.clear();
    sendjson.push_back(make_pair("purpose","migration"));
    sendjson.push_back(make_pair("task","merge"));
    tosend = create_json_string(sendjson);
    if(send(succ_fd, tosend.c_str(), tosend.length(), 0) == -1)
        perror("send");
    //memset(buf,0,BUFFERSIZE);
    //if ((numbytes = recv(succ_fd, buf, BUFFERSIZE, 0)) == -1) 
    //{
    //    perror("recv");
    //    return -1;
    //    //exit(1);
    //}
    sleep(4);
    cout<<"Migration: Step-2 complete"<<endl;
    close(succ_fd);

    succ_fd = connect_to_slave(&(succ->data));
    sendjson.clear();
    sendjson.push_back(make_pair("purpose","migration"));
    sendjson.push_back(make_pair("task","get"));
    sendjson.push_back(make_pair("ip",pre->data.IPaddr));
    sendjson.push_back(make_pair("port",pre->data.portnum));
    sendjson.push_back(make_pair("what","curr"));
    sendjson.push_back(make_pair("addTo","prev"));
    tosend = create_json_string(sendjson);
    if(send(succ_fd, tosend.c_str(), tosend.length(), 0) == -1)
        perror("send");
    //memset(buf,0,BUFFERSIZE);
    //if ((numbytes = recv(succsucc_fd, buf, BUFFERSIZE, 0)) == -1) 
    //{
    //    perror("recv");
    //    return -1;
    //    //exit(1);
    //}
    sleep(4);
    cout<<"Migration: Step-3 complete"<<endl;
    close(succ_fd);
    
}

int CCoord_server::migrationInitUp(slaveData* slave_up)
{
    bstNode* pre=NULL, *succ=NULL, *succsucc=NULL;
    char buf[BUFFERSIZE];
    int numbytes;

    findPreSuc(treeRoot, pre, succ, slave_up->hashvalue);
    if(pre==NULL)
        pre = findMaximum(treeRoot);
    if(succ==NULL)
        succ = findMinimum(treeRoot);
    findSuccessor(treeRoot, succsucc, succ->data.hashvalue);
    if(succsucc==NULL)
        succsucc = findMinimum(treeRoot);

    int pred_fd, succ_fd, succsucc_fd;

    succ_fd = connect_to_slave(&(succ->data));
    vector<pair<string,string>> sendjson;
    sendjson.push_back(make_pair("purpose","migration"));
    sendjson.push_back(make_pair("task","send"));
    sendjson.push_back(make_pair("ip",slave_up->IPaddr));
    sendjson.push_back(make_pair("port",slave_up->portnum));
    sendjson.push_back(make_pair("what","prev"));
    sendjson.push_back(make_pair("addTo","prev"));
    string tosend = create_json_string(sendjson);
    if(send(succ_fd, tosend.c_str(), tosend.length(), 0) == -1)
        perror("send");
    
    sendjson.clear();
    sendjson.push_back(make_pair("purpose","migration"));
    sendjson.push_back(make_pair("task","split"));
    tosend = create_json_string(sendjson);
    if(send(succ_fd, tosend.c_str(), tosend.length(), 0) == -1)
        perror("send");

    sendjson.clear();
    sendjson.push_back(make_pair("purpose","migration"));
    sendjson.push_back(make_pair("task","send"));
    sendjson.push_back(make_pair("ip",slave_up->IPaddr));
    sendjson.push_back(make_pair("port",slave_up->portnum));
    sendjson.push_back(make_pair("what","prev"));
    sendjson.push_back(make_pair("addTo","curr"));
    tosend = create_json_string(sendjson);
    if(send(succ_fd, tosend.c_str(), tosend.length(), 0) == -1)
        perror("send");

    sendjson.clear();
    sendjson.push_back(make_pair("purpose","migration"));
    sendjson.push_back(make_pair("task","send"));
    sendjson.push_back(make_pair("ip",succsucc->data.IPaddr));
    sendjson.push_back(make_pair("port",succsucc->data.portnum));
    sendjson.push_back(make_pair("what","curr"));
    sendjson.push_back(make_pair("addTo","prev"));
    tosend = create_json_string(sendjson);
    if(send(succ_fd, tosend.c_str(), tosend.length(), 0) == -1)
        perror("send");

    close(succ_fd);
}
string CCoord_server::create_json_string(vector<pair<string,string>> &data)
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

Fnv32_t CCoord_server::hashSlave(slaveData* slave)
{
    string ip_port = slave->IPaddr+slave->portnum;
    char* str = new char[ip_port.length()+1];
    strcpy(str, ip_port.c_str());
    //char* str = new char[slave->IPaddr.length()+1];
    //strcpy(str, slave->IPaddr.c_str());
    Fnv32_t hash_val = fnv_32_str(str, FNV1_32_INIT);
    return hash_val;
}

void CCoord_server::insertBST(bstNode** root,slaveData* slave)
{
    if(*root==NULL)
    {
        bstNode* newnode = new bstNode;
        newnode->data = *slave;
        newnode->leftchild = NULL;
        newnode->rightchild = NULL;
        *root = newnode;
        nodecount++;
        return;
    }
    if(slave->hashvalue < (*root)->data.hashvalue)
        insertBST(&((*root)->leftchild), slave);
    else
        insertBST(&((*root)->rightchild), slave);

}


struct bstNode * minValueNode(struct bstNode* node) 
{ 
    struct bstNode* current = node; 
  
    while (current && current->leftchild != NULL) 
        current = current->leftchild; 
  
    return current; 
} 

bstNode* CCoord_server::deleteBST(bstNode** root,Fnv32_t key)
{

 // base case 
    if (*root == NULL) return *root; 
  
    // If the key to be deleted is smaller than the root's key, 
    // then it lies in left subtree 
    if (key < (*root)->data.hashvalue) 
        (*root)->leftchild = deleteBST(&((*root)->leftchild), key);
  
    // If the key to be deleted is greater than the root's key, 
    // then it lies in right subtree 
    else if (key > (*root)->data.hashvalue) 
        (*root)->rightchild = deleteBST(&((*root)->rightchild), key); 
  
    // if key is same as root's key, then This is the node 
    // to be deleted 
    else
    { 
        // node with only one child or no child 
        if ((*root)->leftchild == NULL) 
        { 
            struct bstNode *temp = (*root)->rightchild; 
            free(*root);
            return temp;
        } 
        else if ((*root)->rightchild == NULL) 
        { 
            struct bstNode *temp = (*root)->leftchild; 
            free(*root);
            return temp; 
        } 
  
        // node with two children: Get the inorder successor (smallest 
        // in the right subtree) 
        struct bstNode* temp = minValueNode((*root)->rightchild); 
  
        // Copy the inorder successor's content to this node 
        (*root)->data = temp->data; 
  
        // Delete the inorder successor 
        (*root)->rightchild = deleteBST(&((*root)->rightchild), temp->data.hashvalue); 
    } 
    return *root; 

}

bstNode* CCoord_server::bst_upperBound(bstNode* root,Fnv32_t val)
{
    if(root==NULL)
        return NULL;
    if (root->leftchild == NULL && root->rightchild == NULL && root->data.hashvalue < val) 
        return NULL; 
    if ((root->data.hashvalue >= val && root->leftchild == NULL) || (root->data.hashvalue >= val && root->leftchild->data.hashvalue < val)) 
        return root; 
    if (root->data.hashvalue <= val) 
        return bst_upperBound(root->rightchild, val); 
    else
        return bst_upperBound(root->leftchild, val); 
}

void CCoord_server::findPreSuc(bstNode* root, bstNode*& pre, bstNode*& suc, Fnv32_t key) 
{ 
    // Base case 
    if (root == NULL)  return ; 
  
    // If key is present at root 
    if (root->data.hashvalue == key) 
    { 
        // the maximum value in left subtree is predecessor 
        if (root->leftchild != NULL) 
        { 
            bstNode* tmp = root->leftchild; 
            while (tmp->rightchild) 
                tmp = tmp->rightchild; 
            pre = tmp ; 
        } 
  
        // the minimum value in right subtree is successor 
        if (root->rightchild != NULL) 
        { 
            bstNode* tmp = root->rightchild ; 
            while (tmp->leftchild) 
                tmp = tmp->leftchild ; 
            suc = tmp ; 
        } 
        return ; 
    } 
  
    // If key is smaller than root's key, go to left subtree 
    if (root->data.hashvalue > key) 
    { 
        suc = root ; 
        findPreSuc(root->leftchild, pre, suc, key) ; 
    } 
    else // go to right subtree 
    { 
        pre = root ; 
        findPreSuc(root->rightchild, pre, suc, key) ; 
    } 
} 

bstNode* CCoord_server::findMinimum(bstNode* root)
{
	while (root->leftchild)
		root = root->leftchild;
	return root;
}

bstNode* CCoord_server::findMaximum(bstNode* root)
{
	while (root->rightchild)
		root = root->rightchild;
	return root;
}

void CCoord_server::findPredecessor(bstNode* root, bstNode*& pre, Fnv32_t key)
{
    // base case
    if (root == NULL) {
        pre = NULL;
        return;
    }
 
    // if node with key's value is found, the predecessor is maximum value
    // node in its left subtree (if any)
    if (root->data.hashvalue == key)
    {
        if (root->leftchild)
            pre = findMaximum(root->leftchild);
    }
 
    // if given key is less than the root node, recur for left subtree
    else if (key < root->data.hashvalue)
    {
        findPredecessor(root->leftchild, pre, key);
    }
 
    // if given key is more than the root node, recur for right subtree
    else
    {
        // update predecessor to current node before recursing 
        // in right subtree
        pre = root;
        findPredecessor(root->rightchild, pre, key);
    }
}

void CCoord_server::findSuccessor(bstNode* root, bstNode*& succ, Fnv32_t key)
{
	// base case
	if (root == NULL) 
    {
		succ = NULL;
		return;
	}

	if (root->data.hashvalue == key)
	{
		if (root->rightchild)
			succ = findMinimum(root->rightchild);
	}
	else if (key < root->data.hashvalue)
	{
		succ = root;
		findSuccessor(root->leftchild, succ, key);
	}
	else
		findSuccessor(root->rightchild, succ, key);
}
void CCoord_server::inorder(bstNode *root)
{
    if(root==NULL){
        
        return;
    }
    else{

        inorder(root->leftchild);
        cout << root->data.hashvalue << "/" << root->data.IPaddr << ", ";
        inorder(root->rightchild);
    }
}
int main()
{
    CCoord_server obj;
    obj.alwaysListen();
    return 0;
}
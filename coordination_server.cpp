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
    char buf[BUFFERSIZE];
    int numbytes;
    // First receive
    Document document;
    while(1)
    {
        memset(buf,0,BUFFERSIZE);
        if ((numbytes = recv(fd, buf, BUFFERSIZE, 0)) == -1) 
        {
            perror("recv");
            return -1;
            //exit(1);
        }
        string tmp(buf);
        cout<<tmp<<endl;
        document.Parse(tmp.c_str());
        assert(document["purpose"].IsString());
        string purpose=document["purpose"].GetString();
        if(purpose=="create_user")
        {
            create_user(document["username"].GetString(),document["password"].GetString(), fd);
        }
        else if(purpose=="login")
        {
            login(document["username"].GetString(),document["password"].GetString(), fd);
        }
        else if(purpose=="put")
        {
            string bufstr(buf);
            putData(bufstr, fd);
        }
        else if(purpose=="get")
        {
            string bufstr(buf);
            getData(bufstr, fd);    //passing file desc and command string
        }
        else if(purpose=="delete")
        {
            deleteData();
        }
        else if(purpose=="update")
        {
            updateData();
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
    insertBST(&treeRoot,newSlave);
    std::pair<std::map<string,slaveData>::iterator,bool> ret;
    ret =  slavemap.insert(make_pair(newSlave->IPaddr, *newSlave));
    
    if (ret.second==false) 
    {
        // If element already exists
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
            cout<<"No receive"<<endl;
        secondsPassed = ((double)(clock() - startTime) / CLOCKS_PER_SEC)*1000;
        //cout<<clock()<<" "<<startTime<<endl;
        if(secondsPassed >= secondsToDelay)
            cout<<"Slave down"<<endl;
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

int CCoord_server::putData(string bufstr, int client_fd)
{
    int primary_fd, secondary_fd,numbytes1, numbytes2;
    char primaryResp[BUFFERSIZE], secondaryResp[BUFFERSIZE];
    Document document;
    document.Parse(bufstr.c_str());
    string key = document["key"].GetString();
    char* str = new char[key.length()+1];
    strcpy(str, key.c_str());
    Fnv32_t hash_val = fnv_32_str(str, FNV1_32_INIT);

    bstNode* primaryNode = bst_upperBound(treeRoot, hash_val);
    if(primaryNode == NULL)      // If hash value of key is greater than the last slave server, search slave with smallest hash value
    {
        primaryNode = bst_upperBound(treeRoot, 0);
    }
    bstNode* secondaryNode;
    findSuccessor(treeRoot, secondaryNode, primaryNode->data.hashvalue);
    if(secondaryNode == NULL)      // If hash value of key is greater than the last slave server, search slave with smallest hash value
    {
        secondaryNode = bst_upperBound(treeRoot, 0);
    }
    if(primaryNode->data.isActive && secondaryNode->data.isActive)
    {
        memset(primaryResp,0,BUFFERSIZE);
        memset(secondaryResp,0,BUFFERSIZE);
        primary_fd = connect_to_slave(&(primaryNode->data));
        secondary_fd = connect_to_slave(&(secondaryNode->data));
        thread primarythread(&CCoord_server::putDataThreadFn, this, bufstr, &primaryResp, &numbytes1, primary_fd);
        thread secondarythread(&CCoord_server::putDataThreadFn, this, bufstr, &secondaryResp, &numbytes2, secondary_fd);
        primarythread.join();
        secondarythread.join();
    }
    else
    {
        if(!primaryNode->data.isActive)
            cout<<"Primary slave down"<<endl;
        if(!secondaryNode->data.isActive)
            cout<<"Secondary slave down"<<endl;
        return;
    }
    if(send(client_fd, primaryResp, numbytes1, 0) == -1)
        perror("send");
}

int CCoord_server::getData(string bufstr, int client_fd)
{
    put_update_delete_handle(bufstr, client_fd);
}

int CCoord_server::deleteData(string bufstr, int client_fd)
{
    put_update_delete_handle(bufstr, client_fd);
}

int CCoord_server::updateData(string bufstr, int client_fd)
{
    put_update_delete_handle(bufstr, client_fd);
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
            continue;
        }

        break;
    }

    if (p == NULL) 
    {
        fprintf(stderr, "client: failed to connect\n");
        return -2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure
    
    return sockfd;
}

void CCoord_server::put_update_delete_handle(string bufstr, int client_fd)
{
    int primary_fd, secondary_fd,numbytes1, numbytes2;
    char primaryResp[BUFFERSIZE], secondaryResp[BUFFERSIZE];
    Document document;
    document.Parse(bufstr.c_str());
    string key = document["key"].GetString();
    char* str = new char[key.length()+1];
    strcpy(str, key.c_str());
    Fnv32_t hash_val = fnv_32_str(str, FNV1_32_INIT);

    bstNode* primaryNode = bst_upperBound(treeRoot, hash_val);
    if(primaryNode == NULL)      // If hash value of key is greater than the last slave server, search slave with smallest hash value
    {
        primaryNode = bst_upperBound(treeRoot, 0);
    }
    bstNode* secondaryNode;
    findSuccessor(treeRoot, secondaryNode, primaryNode->data.hashvalue);
    if(secondaryNode == NULL)      // If hash value of key is greater than the last slave server, search slave with smallest hash value
    {
        secondaryNode = bst_upperBound(treeRoot, 0);
    }
    if(primaryNode->data.isActive && secondaryNode->data.isActive)
    {
        memset(primaryResp,0,BUFFERSIZE);
        memset(secondaryResp,0,BUFFERSIZE);
        primary_fd = connect_to_slave(&(primaryNode->data));
        secondary_fd = connect_to_slave(&(secondaryNode->data));
        thread primarythread(&CCoord_server::data_modify_ThreadFn, this, bufstr, &primaryResp, &numbytes1, primary_fd);
        thread secondarythread(&CCoord_server::data_modify_ThreadFn, this, bufstr, &secondaryResp, &numbytes2, secondary_fd);
        primarythread.join();
        secondarythread.join();
    }
    else
    {
        if(!primaryNode->data.isActive)
            cout<<"Primary slave down"<<endl;
        if(!secondaryNode->data.isActive)
            cout<<"Secondary slave down"<<endl;
        return;
    }
    if(send(client_fd, primaryResp, numbytes1, 0) == -1)
        perror("send");
}
void CCoord_server::data_modify_ThreadFn(string bufstr, char* response, int* numbytes, int fd)
{
    if(send(fd, bufstr.c_str(), bufstr.length(), 0) == -1)
        perror("send");
    if ((*numbytes = recv(fd, response, BUFFERSIZE, 0)) == -1) 
    {
        perror("recv");
        return -1;
        //exit(1);
    }
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
    char* str = new char[slave->IPaddr.length()+1];
    strcpy(str, slave->IPaddr.c_str());
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
        return;
    }
    if(slave->hashvalue < (*root)->data.hashvalue)
        insertBST(&((*root)->leftchild), slave);
    else
        insertBST(&((*root)->rightchild), slave);

}

bstNode* CCoord_server::bst_upperBound(bstNode* root,Fnv32_t val)
{
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


void CCoord_server::findSuccessor(bstNode* root, bstNode*& succ, Fnv32_t key)
{
	// base case
	if (root == nullptr) {
		succ = nullptr;
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

int main()
{
    CCoord_server obj;
    obj.alwaysListen();
    return 0;
}
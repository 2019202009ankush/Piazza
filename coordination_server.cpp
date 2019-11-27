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
        string connectionIP (s);
        string connectionPort = to_string(ntohs(((struct sockaddr_in *)&their_addr)->sin_port));
        char buf[BUFFERSIZE];
        int numbytes;
        // First receive
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
            //thread typethread(slaveHandle,*new_fd);
            //typethread.detach();
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
            putData();
        }
        else if(purpose=="get")
        {
            getData();
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
int CCoord_server::slaveHandle(int fd)
{
    
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
        if(send(sock_fd, "exists", strlen("exists"), 0) == -1)
            perror("send");
        return 0;
    }
    else
    {
        if(send(sock_fd, "success", strlen("success"), 0) == -1)
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
            if(send(sock_fd, "success", strlen("success"), 0) == -1)
                perror("send");
            return true;    // User present, might not be active;
        }
        else
        {
            if(send(sock_fd, "invalid", strlen("invalid"), 0) == -1)
                perror("send");
            return false;
        }
    } 
    else
    {
        if(send(sock_fd, "failure", strlen("failure"), 0) == -1)
            perror("send");
        return false;   // user not registered with tracker
    }
}

int CCoord_server::putData()
{

}

int CCoord_server::getData()
{

}

int CCoord_server::deleteData()
{

}

int CCoord_server::updateData()
{

}

int main()
{
    CCoord_server obj;
    obj.alwaysListen();
    return 0;
}
#include<iostream>
#include<fstream>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DOMAIN AF_INET
#define TYPE SOCK_STREAM
#define PROTOCOL 0
#define LEVEL SOL_SOCKET
#define SET_OPTIONS SO_REUSEADDR

using namespace std;

int main()
{
    string path,ip;
    int port;
    int option_set=1;
    struct sockaddr_in  server_addr;
    cout<<"Enter Config File Path:"<<endl;
    cin>>path;
    fstream details(path);
    details>>ip;
    port = stoi(ip);
    details>>ip;
    cout<<"IP is:"<<ip<<" Port is:"<<port<<endl;
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
    cout<<"Connection Status = "<<connect_stat<<endl;
    return 0;
}

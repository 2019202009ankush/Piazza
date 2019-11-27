/* -----Includes----- */
#include<iostream>
#include<string>
#include<vector>
#include<map>
#include<fstream>
#include<sstream>
#include<cstring>
#include "include/rapidjson/document.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include<thread>
#include <arpa/inet.h>

/* -----Defines----- */
#define BACKLOG 10
#define BUFFERSIZE 512
using namespace std;
using namespace rapidjson;

/*
    variables:
    username, password, IP-address, Portnumber-Client's p-num to communicate to
*/
struct clientData
{
    string username;
    string password;
    string IPaddr;
    string portnum;
    clientData()
    {
        username='\0';
        password='\0';
        IPaddr='\0';
        portnum='\0';
    }
};

struct coordination_serv
{
    string IPaddr;
    string portnum;
    coordination_serv()
    {
        IPaddr='\0';
        portnum='\0';
    }
};
/*

*/
class CCoord_server
{
private:
        map<string, clientData> clientmap;
        coordination_serv coord_info;
public:
        int alwaysListen();

};

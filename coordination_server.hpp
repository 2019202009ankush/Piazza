/* -----Includes----- */
#include<iostream>
#include<string>
#include<vector>
#include<map>
#include<fstream>
#include<sstream>
#include<cstring>
#include "include/rapidjson/document.h"
#include "include/rapidjson/rapidjson.h"
#include "fnv-hash/fnv.h"
#include "fnv-hash/hash_32.c"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
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
#define SECTODELAY 2
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
    bool isActive;
    clientData()
    {
        username='\0';
        password='\0';
        IPaddr='\0';
        portnum='\0';
        isActive=false;
    }
};

struct slaveData
{
    string IPaddr;
    string portnum;
    Fnv32_t hashvalue;
    bool isActive;
    slaveData()
    {
        IPaddr='\0';
        portnum='\0';
        isActive=false;
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

struct bstNode
{
    slaveData data;
    bstNode* leftchild;
    bstNode* rightchild;
};

/*
    Private members:
    clientmap: <username, struct clientData>, username as key
    servermap: <IPaddr, struct serverData>, hash value as key **DOubt: what if 2 servers hash to same value?
    coord_info: Coordiantion server instance information
*/
class CCoord_server
{
private:
        map<string, clientData> clientmap;
        map<string,slaveData> slavemap;
        coordination_serv coord_info;
        bstNode* treeRoot;
public:
        CCoord_server()
        {
            treeRoot=NULL;
        }
        int alwaysListen();
        int clientHandle(int fd);
        int slaveHandle(string slaveIP, string slavePort, int fd);
        int create_user(string username, string password, int sock_fd);
        int login(string username, string password, int sock_fd);
        int putData();
        int getData();
        int deleteData();
        int updateData();
        string create_json_string(vector<pair<string,string>> &data);
        Fnv32_t hashSlave(slaveData* newSlave);
        void insertBST(bstNode* root,slaveData* newSlave);
        bstNode* bst_upperBound(bstNode* root,Fnv32_t val);
        void findPreSuc(bstNode* root, bstNode*& pre, bstNode*& suc, Fnv32_t key);

};

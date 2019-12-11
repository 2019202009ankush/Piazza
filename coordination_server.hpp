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
#include "include/rapidjson/stringbuffer.h"
#include "include/rapidjson/writer.h"
#include "lru_cache.hpp"

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
#define SECTODELAY 1
#define CACHE_ENABLE 1
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
    string portnum; //This is Listening Port of SlaveServer
    Fnv32_t hashvalue;
    bool isActive;
    lruCache cache;
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
        int nodecount;
public:
        CCoord_server()
        {
            treeRoot=NULL;
            nodecount=0;
        }
        int alwaysListen();
        int clientHandle(int fd);
        int slaveHandle(string slaveIP, string slavePort, int fd);
        int create_user(string username, string password, int sock_fd);
        int login(string username, string password, int sock_fd);
        int putData(string bufstr, int client_fd);
        int getData(string key, int client_fd);
        int deleteData(string bufstr, int client_fd);
        int updateData(string bufstr, int client_fd);
        int connect_to_slave(slaveData* slave);
        bstNode* put_update_delete_handle(string bufstr, int client_fd);
        int data_modify_ThreadFn(string bufstr, char* response, int* numbytes1, int fd);
        int migrationInitDown(slaveData* slave_down);
        int migrationInitUp(slaveData* slave_up);
        string create_json_string(vector<pair<string,string>> &data);
        Fnv32_t hashSlave(slaveData* newSlave);
        void insertBST(bstNode** root,slaveData* newSlave);
        bstNode* bst_upperBound(bstNode* root,Fnv32_t val);
        void findPreSuc(bstNode* root, bstNode*& pre, bstNode*& suc, Fnv32_t key);
        void findPredecessor(bstNode* root, bstNode*& pre, Fnv32_t key);
        void findSuccessor(bstNode* root, bstNode*& succ, Fnv32_t key);
        bstNode* findMinimum(bstNode* root);
        bstNode* findMaximum(bstNode* root);
        bstNode* deleteBST(bstNode** root,Fnv32_t key);
        void inorder(bstNode *root);

};

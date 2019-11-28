#include"piazzaHeader.h"
#include"replication_group.cpp"
int main()
{
   Replication_Group s1("127.0.0.1","8000","127.0.0.1","8002");
   thread serverthread(&Replication_Group::serverpart,&s1);
   
   
   string request;
   s1.sendmessage();
   serverthread.join();

      
   
}
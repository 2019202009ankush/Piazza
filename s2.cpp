#include"piazzaHeader.h"
#include"replication_group.cpp"
int main()
{
   Replication_Group s2("10.2.131.30","8002","127.0.0.1","8000");
   
   
   thread serverthread(&Replication_Group::serverpart,&s2);
   serverthread.join();
 	
   
}
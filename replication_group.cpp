#include "piazzaHeader.h"
class Replication_Group{
public:
string TR1ip;
string TR1port;
string severip;
string serverport;
vector<thread> threadVector;
int threadCount;
sem_t m;
unordered_map<string,set<int> >AvailableChunkInfoPerFileBasis;
unordered_map<string,string>FileIdandFilepathMap;
unordered_map<string,int>download_status;
Replication_Group(string TR1ip,string TR1port,string severip,string serverport)
{
  TR1ip=TR1ip;
  TR1port=TR1port;
  severip=severip;
  serverport=serverport;
}
vector<string>ArrayOfString(string s,char del)
{
  stringstream ss(s);
  vector<string>a;
  string temp;
  while(getline(ss,temp,del))
  {
    a.push_back(temp);
  }
  return a;
}
void get_the_particular_packet(int newsocketdes,string FileId,string packetNos)
{
   // cout<<"in line 361 get_the_particular_packet"<<endl;
   string Filepath=FileIdandFilepathMap[FileId];
   // cout<<"in line 363 Filepath="<<Filepath<<endl;
   FILE *fp=fopen(Filepath.c_str(),"rb");
   // cout<<"in line 365"<<endl;
   int val;
   char buffer[BUFF];
   bzero(buffer,BUFF);
   vector<string>ArrayofPacket=ArrayOfString(packetNos,';');
   vector<int>ArrayofPacket_int;
   for(int i=0;i<ArrayofPacket.size();i++)
   {
      ArrayofPacket_int.push_back(stoi(ArrayofPacket[i]));
   }
   for(int i=0;i<ArrayofPacket_int.size();i++)
   {
      fseek(fp,ArrayofPacket_int[i]*BUFF_SIZE,SEEK_SET);
      int size=BUFF_SIZE;
      while((val=fread(buffer,sizeof(char),BUFF,fp))>0&&size>0)
      {
         // cout<<"in line 442 in get get_the_particular_packet val= size= "<<val<<" "<<size<<endl;
         send(newsocketdes,buffer,val,0);
         //char bu[1]='\0';
         //recv(newsocketdes,bu,1,0);
         memset ( buffer , '\0', sizeof(buffer));
         size=size-val;
      }
      bzero(buffer,BUFF);
      
   }
   fclose(fp);
   close(newsocketdes);
   goto l2;
   l2:
     cout<<"";
}
void send_the_packet_vector(int newsocketdes,string FileId)
{
   // cout<<"in line 326 send the packet vector"<<endl;
   string chunkdetails="";
      set<int>:: iterator it;
   for(it=AvailableChunkInfoPerFileBasis[FileId].begin();it!=AvailableChunkInfoPerFileBasis[FileId].end();it++)
   {
     if(it==AvailableChunkInfoPerFileBasis[FileId].begin())
     {
       chunkdetails+=to_string(*it);
     }
     else
     {
      chunkdetails+=";";
      chunkdetails+=to_string(*it);
     }
   }
   // cout<<"in line 341 chunkdetails= "<<chunkdetails<<endl;
   send(newsocketdes,chunkdetails.c_str(),strlen(chunkdetails.c_str()),0);
   close(newsocketdes);
  goto l2;
   l2:
     cout<<"";
}
void serverequest(int newsocketdes,string ip,int port)
{
   l2:
   // cout<<"in line 598"<<endl;

   char buffer[BUFF];
   bzero(buffer,BUFF);
   read(newsocketdes,buffer,sizeof(buffer));
   // cout<<"in line 603 buffer="<<buffer<<endl;
   string r=buffer;
   // cout<<"in line 605 r="<<r<<endl;

   vector<string> requestarray=ArrayOfString(r,';');
   string request=requestarray[0];
   if(request=="send_the_packet_vector")
   {
      string FileId=requestarray[1];
       send_the_packet_vector(newsocketdes,FileId);
   }
   else if(request=="get_the_particular_packet")
   {
      string FileId=requestarray[1];
      string packetNos=requestarray[2];
      get_the_particular_packet(newsocketdes,FileId,packetNos);
   }
   else if(request=="get_single_tuple")
   {
      string FileId=requestarray[1];
      string primarykey=requestarray[2];
      get_single_tuple(newsocketdes,FileId,primarykey);
   }
   else if(request=="get_single_tuple_value")
   {
    string FileId=requestarray[1];
    stirng primarykey=requestarray[2];
    string column=requestarray[3];
    get_single_tuple_value(newsocketdes,FileId,primarykey,column);
   }
   else if(request=="create_table")
   {
    string FileId=requestarray[1];
    int no_of_col=stoi(requestarray[2]);
    stirng column_name=requestarray[3];
    create_table(newsocketdes,FileId,primarykey,column_name);
   }
   else if(request=="put_value")
   {
    string Filepath=requestarray[1];
    stirng primarykey=requestarray[2];
    string columnpair=requestarray[3];
    put_value(newsocketdes,FileId,primarykey,columnpair);
   }
   else if(request=="del")
   {
    string FileId=requestarray[1];
    del(newsocketdes,FileId);
    
   }
   else
   {
      cout<<"Some randome data is coming"<<endl;
      cout<<"please send it again"<<endl;
      goto l2;
   }
}
void serverpart()
{
   int socketdes;
   int newsocketdes;
   int val;
   socklen_t size;
   struct sockaddr_in myaddr;
   struct sockaddr_in otheraddr;
   if((socketdes=socket(AF_INET,SOCK_STREAM,0))<0)
   {
      perror("failed to obtained the socket descriptor");
       exit(1);
   }
  myaddr.sin_family=AF_INET;
  myaddr.sin_port=htons(stoi(serverport));
  inet_pton(AF_INET,severip.c_str() , &myaddr.sin_addr); 
  //myaddr.sin_addr.s_addr=TR1ip;

  bzero(&(myaddr.sin_zero),8);

  if(bind(socketdes,(struct sockaddr *)&myaddr,sizeof(struct sockaddr))==-1)
  {
   perror("failed to obtained the bind");
   exit(1);
  }
  if(listen(socketdes,BACK)==-1)
  {
     perror("error backlog overflow");
     exit(1);

  }
  size=sizeof(struct sockaddr);
  while((newsocketdes=accept(socketdes,(struct sockaddr *)&otheraddr,&size))!=-1)
  {
   // cout<<"Got a connection from another peer "<<endl;
   string ip=inet_ntoa(otheraddr.sin_addr);
   int port=(ntohs(otheraddr.sin_port));
   // cout<<"ip="<<ip<<"port"<<port<<endl;
   threadVector.push_back(thread(serverequest,newsocketdes,ip,port));
   size=sizeof(struct sockaddr);
  }
  vector<thread>:: iterator it;
  for(it=threadVector.begin();it!=threadVector.end();it++)

   {
      if(it->joinable()) 
         it->join();
   }
   cout<<"Returning from server "<<endl;
   goto l2;
   l2:
     cout<<"";

}
};

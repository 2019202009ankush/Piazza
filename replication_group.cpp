#include "piazzaHeader.h"
class Replication_Group{
public:
static string TR1ip;
static string TR1port;
static string severip;
static string serverport;
static vector<thread> threadVector;
static int threadCount;
static sem_t m;
static unordered_map<string,set<int> >AvailableChunkInfoPerFileBasis;
static unordered_map<string,string>FileIdandFilepathMap;
unordered_map<string,int>download_status;
Replication_Group(string TR1ip,string TR1port,string severip,string serverport)
{
  TR1ip=TR1ip;
  TR1port=TR1port;
  severip=severip;
  serverport=serverport;
}
static vector<string>ArrayOfString(string s,char del)
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
static void get_the_particular_packet(int newsocketdes,string FileId,string packetNos)
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
static void create_table(int newsocketdes,string FileId,int no_of_col,string column)
{
   vector<string>cols=ArrayOfString(column,':');
   fstream out;
   out.open("Metadata.txt",ios::out|ios::in|ios::app);
   out.write(FileId.c_str(),FileId.size());
   for(int i=0;i<cols.size();i++)
   {
     out.write(cols[i].c_str(),cols[i].size());
   }
   out.write("\n",sizeof(char)*2);
   out.close();
}
static void put_value(int newsocketdes,string FileId,string columnpair)
{
  vector<string>cols=ArrayOfString(columnpair,':');
   fstream out;
   out.open(FileId.c_str(),ios::out|ios::in|ios::app);
   out.write(FileId.c_str(),FileId.size());
   for(int i=0;i<cols.size();i++)
   {
     out.write(cols[i].c_str(),cols[i].size());
   }
   out.write("\n",sizeof(char)*2);
   out.close();  
}
static string get_single_tuple(int newsocketdes,string FileId,string primarykey)
{
  ifstream in(FileId.c_str());
  string temp;
  while(getline(in,temp,':'))
  {
    if(temp.find(primarykey)!=std::string::npos)
      break;
  }
  return temp;
}
static string get_single_tuple_value(int newsocketdes,string FileId,string primarykey,string column)
{
  cout<<"Not implemented"<<endl;
}
static void del(int newsocketdes,string FileId)
{
  cout<<"Not Implemented"<<endl;
}
static void send_the_packet_vector(int newsocketdes,string FileId)
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
static void server_request(int newsocketdes)
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
    string primarykey=requestarray[2];
    string column=requestarray[3];
    get_single_tuple_value(newsocketdes,FileId,primarykey,column);
   }
   else if(request=="create_table")
   {
    string FileId=requestarray[1];
    int no_of_col=stoi(requestarray[2]);
    string column_name=requestarray[3];
    create_table(newsocketdes,FileId,no_of_col,column_name);
   }
   else if(request=="put_value")
   {
    string FileId=requestarray[1];
    string columnpair=requestarray[2];
    put_value(newsocketdes,FileId,columnpair);
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
static void serverpart()
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
   string ip=string(inet_ntoa(otheraddr.sin_addr));
   int port=(ntohs(otheraddr.sin_port));
   // cout<<"ip="<<ip<<"port"<<port<<endl;
   threadVector.push_back(thread(server_request,newsocketdes));
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


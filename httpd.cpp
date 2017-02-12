#include <iostream>
#include "httpd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <linux/limits.h>
#include <cstring>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <fstream>

#include <bitset>
using namespace std;



void start_httpd(unsigned short port, string doc_root, int pool_mode, int pool_size)
{
	cerr << "Starting server (port: " << port <<
		", doc_root: " << doc_root << ")" << endl;


  //create socket
  int servSock;
  if ((servSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    DieWithSystemMessage("socket() failed");

  struct sockaddr_in servAddr;   
  memset(&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family = AF_INET; 
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port = htons(port);

  // Bind to the local address
  if (bind(servSock, (struct sockaddr*) &servAddr, sizeof(servAddr)) < 0)
    DieWithSystemMessage("bind() failed");

 

    
  if (pool_mode == 1){
    if (listen(servSock, pool_size+1) < 0)
      DieWithSystemMessage("listen() failed");

    pthread_t * client_thread  = (pthread_t *) malloc (pool_size * sizeof(pthread_t));
    //pthread_t  client_thread[100];

    int i;
    for (i=0; i<pool_size; i++){
      struct poolThread_arg * thread_arg = (struct poolThread_arg *) malloc(sizeof(struct poolThread_arg));
      thread_arg->servSock = servSock;
      thread_arg->doc_root = doc_root;
      int thread_status = pthread_create(&client_thread[i], NULL, PoolThread, thread_arg );
      if (thread_status < 0)
        cerr << "spawn thread failed" << endl;
    }


    for (i=0; i<pool_size; i++){
      pthread_join(client_thread[i], NULL);

    }
    free(client_thread);

    //for (;;){


    //}

    
  } else {


  // Mark the socket so it will listen for incoming connections
  if (listen(servSock, MAXPENDING) < 0)
    DieWithSystemMessage("listen() failed");


  


  for (;;) { // Run forever
    struct sockaddr_in clntAddr; // Client address
    // Set length of client address structure (in-out parameter)
    socklen_t clntAddrLen = sizeof(clntAddr);

    // Wait for a client to connect
    int clntSock = accept(servSock, (struct sockaddr *) &clntAddr, &clntAddrLen);
    unsigned long client_address = clntAddr.sin_addr.s_addr;
    if (clntSock < 0)
      printf("accept() failed");


    // clntSock is connected to a client!

    char clntName[INET_ADDRSTRLEN]; // String to contain client address
    if (inet_ntop(AF_INET, &clntAddr.sin_addr.s_addr, clntName,
        sizeof(clntName)) != NULL)
      printf("Handling client %s/%d\n", clntName, ntohs(clntAddr.sin_port));
    else
      puts("Unable to get client address");


    

    //spawning new thread for each incoming client

    struct ThreadArgs * thread_arg = (struct ThreadArgs *) malloc(sizeof(struct ThreadArgs));
    if (thread_arg == NULL)
      cerr << "malloc failed"<<endl;
    thread_arg->clnt_socket = clntSock;
    thread_arg->doc_root    = doc_root;
    thread_arg->clnt_addr   = client_address;


    pthread_t thread;
    int thread_status = pthread_create( &thread, NULL, ThreadMain, thread_arg);
    if (thread_status<0)
      cerr<<"pthread create failed"<<endl;
  }
  }


}


void * PoolThread(void * thread_arg){

  struct sockaddr_in clntAddr;
  socklen_t clntAddrLen = sizeof(clntAddr);

  int servSock    = ((struct poolThread_arg *) thread_arg)->servSock;
  string doc_root = ((struct poolThread_arg *) thread_arg)->doc_root;
  free(thread_arg);
  for (;;){
    
    int clntSock    = accept(servSock, (struct sockaddr *) &clntAddr, &clntAddrLen);
    unsigned long client_address = clntAddr.sin_addr.s_addr;
    if (clntSock < 0)
      printf("accept() failed");
    char clntName[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &clntAddr.sin_addr.s_addr, clntName,
        sizeof(clntName)) != NULL)
      printf("Handling client %s/%d\n", clntName, ntohs(clntAddr.sin_port));
    else
      puts("Unable to get client address"); 
    HandleTCPClient( clntSock , doc_root,client_address);
  }
  return NULL;
}




void * ThreadMain(void * thread_arg){

  pthread_detach (pthread_self());
  int client_socket            = ((struct ThreadArgs *) thread_arg)->clnt_socket;
  string doc_root              = ((struct ThreadArgs *) thread_arg)->doc_root;
  unsigned long client_address = ((struct ThreadArgs *) thread_arg)->clnt_addr;
  free(thread_arg);
  HandleTCPClient( client_socket, doc_root, client_address);
  return thread_arg;

}


void send_error(int status, int clntSocket){
  printf("sending error with status %d", status);
   
  string response;
  if (status == 400){
    response = "HTTP/1.1 400 Client Error\r\nServer: TritonSever\r\n\r\n";
  } else if (status == 403) {
    response = "HTTP/1.1 403 Forbidden\r\nServer: TritonSever\r\n\r\n";
  } else if (status == 404) {
    response = "HTTP/1.1 404 Not Found\r\nServer: TritonSever\r\n\r\n";
  } else {
    printf("this should never happen in send_error\n");
    return;
  }
  
  const char * buffer = response.c_str();
  ssize_t numBytesSent = send(clntSocket, buffer, strlen(buffer), 0);
  if (numBytesSent != (ssize_t)strlen(buffer))
      printf("send() failed\n");
  if (status == 400) { 
    close(clntSocket);
  }
}



int check_permission(string check_line, unsigned long clnt_addr){

  printf("inside check_permission\n");
  char * line  = (char *)check_line.c_str();
  string slash = "/";
  char * delim = (char *)slash.c_str();
  char * token = strsep(&line, delim);
  struct sockaddr_in check_addr;
  int ip = -1;
  unsigned long long_address =0; 
  if (token!= NULL){
    ip = inet_pton(AF_INET, token, &check_addr.sin_addr);
    if (ip != 1) { printf("inet_pton failed \n"); }

  }
  long_address = check_addr.sin_addr.s_addr;
  token = strsep(&line, delim);

  int bit = 32 - atoi(token);

//  std::bitset<32> x(long_address);
//  std::bitset<32> y(clnt_addr);

//  std::cout << "long address is " << x <<endl; 
//  std::cout << "clnt address is " << y <<endl;
 
  if ( uint32_t ( long_address >> bit)  == uint32_t (clnt_addr >> bit)){
     return -1;
  }
  return 0;
}

int dns_lookup(string check_line, unsigned long clnt_addr ){
   struct addrinfo host;
   struct addrinfo *hostinfo, *current;
   memset(&host, 0, sizeof(host));
   host.ai_family   = AF_INET;
   host.ai_socktype = SOCK_STREAM;
   if ( getaddrinfo( check_line.c_str() , "http" , &host , &hostinfo)!= 0){
          printf("get addrinfo failed\n");     
   }
   struct sockaddr_in *address; 
   for(current = hostinfo; current != NULL; current = current->ai_next) {
     address = (struct sockaddr_in *) current->ai_addr;
     if (address->sin_addr.s_addr == ((uint32_t) clnt_addr)){
       //no permission
       freeaddrinfo(hostinfo);
       return -1;
     }
   }
   freeaddrinfo(hostinfo);
   return 0;

}


int check_htaccess (string htaccess, unsigned long clnt_addr){
  printf("in check_htaccess\n");
 
  std::ifstream fs (htaccess.c_str());
  string line;
  while (getline(fs, line)){
    string deny = "deny";
    string allow = "allow";
    if ( strncmp ( deny.c_str(), line.c_str(), strlen(deny.c_str()))==0){
      //if it's a deny line
      string check_line = line.substr(10);
      size_t found = check_line.find("/");  
      if (found != string::npos){
        // a slash is found then it's normal ip address
        if (check_permission(check_line,clnt_addr)==-1){
          //no permission
          cout << "line 275\n";
          //send_error(403, clntSocket);
          return -1;
        }
      } else {
        //need to do DNS lookup
        int dns = dns_lookup(check_line, clnt_addr);
        //if found match for deny
        if (dns == -1){
          cout << "line 284\n";
          //send_error(403, clntSocket);
          return -1;
        }
      }
    } else {
      //check allow
      string check_line = line.substr(11);
      size_t found = check_line.find("/");
      if (found != string::npos){
      // a slash is found then it's normal ip address
        if (check_permission(check_line,clnt_addr)==-1){
          //yes permission
          return 0;
        }
      } else {
        //dns look up
        int dns = dns_lookup(check_line, clnt_addr);
        //found match for allow
        if (dns == -1)
          return 0;        
      }

    }

  }
  return 0;
}



void send_response(int clntSocket, string doc_root, void * request, unsigned long clnt_addr){

  HttpRequest * reqptr = (HttpRequest *) request;

  int fd   = 0;
  int size = 0;
  FILE * req_file = NULL;

  string response; 

  if (!strcmp(reqptr->path, "/") ){
    reqptr->path = (char *)"/index.html";
  }
  
  string reqptr_str (reqptr->path); 
  reqptr_str = doc_root+reqptr_str;
  const char * path_to_file =  reqptr_str.c_str();
  char file_path [PATH_MAX];
  char file_path1 [PATH_MAX];

  char * abs_path = realpath(path_to_file, file_path);
  char * abs_doc  = realpath(doc_root.c_str(), file_path1);
  printf("abs_path is %s\n", abs_path);
    
  if (abs_path!=NULL && abs_doc!=NULL){

    int permission = strncmp ( abs_doc, abs_path, strlen(abs_doc));
    if (permission != 0) {
      printf("is it here\n");
      send_error(403, clntSocket);
                //close(clntSocket);
      return;
    } 
  }

 
  //checking htaccess files
  struct stat attr;
  stat(path_to_file, &attr);
  string htaccess = string (path_to_file)+"/.htaccess";
  //check if it's a directory and htaccess file exist
  if ( S_ISDIR(attr.st_mode) != 0 && access(htaccess.c_str(), F_OK)!=-1 ){
    
    if ( check_htaccess(htaccess, clnt_addr)==-1){
      send_error(403, clntSocket);
      return ;
    }

  } else {
    // not a directory
    string path_string (path_to_file);
    int last_slash = path_string.find_last_of("/");
    string ht_str  = path_string.substr(0, last_slash) + "/.htaccess";
    if (access(ht_str.c_str(), F_OK)!=-1){
      if (check_htaccess(ht_str, clnt_addr)==-1){
        send_error(403, clntSocket);
        return; 
      }
    }
 
  }

  //check file permission mode
  //
  printf( "what is size %d\n", (int)attr.st_size);
  if (!(attr.st_mode & S_IROTH)){

//  if ((access(path_to_file, F_OK)!= -1) && (!(attr.st_mode & S_IROTH))){
    printf("line 371\n");
    send_error(403, clntSocket);
    return;
  }

  
    //assume have permission
    //Getting size of file
    //
    //check permission, if file is there or not, if path is legal or not
  req_file = fopen(path_to_file, "r");
  if (req_file == NULL){   
    send_error(404, clntSocket);
    return ;   
  } else {  
      //fd = fileno(req_file);
      fd = open (path_to_file, O_RDONLY);
      //printf("fd is %d\n", fd);
      fseek(req_file, 0, SEEK_END);
      size = ftell(req_file);
      fseek(req_file, 0, SEEK_SET);

      std::ostringstream ss;
      ss << size;
      string st_size = ss.str();
      
      //Get last modified
      char time_buffer [200];
      struct tm * time;
      
      //stat(path_to_file, &attr);
      time = gmtime(&(attr.st_mtime));
      strftime(time_buffer, sizeof (time_buffer), "%a, %d %b %Y %H:%M:%S %Z", time);
     // printf ("last modified time is %s\n", time_buffer);
      string st_time(time_buffer);
   
     //check extension
     const char * dot0 =  strrchr (path_to_file, '.');
     char * dot = strdup(dot0);
     dot = dot + 1;


    //different string depending on extension
     if (!strcmp(dot, "jpg")){
       response = "HTTP/1.1 200 OK\r\nServer: TritonSever\r\nLast-Modified: "
         +st_time
         +"\r\nContent-Type: image/jpeg\r\nContent-Length: "
         +st_size
         +"\r\n\r\n";
      // fread(data_buf, 1, size, req_file);
       
     } else if (!strcmp(dot, "png")) {
       response = "HTTP/1.1 200 OK\r\nServer: TritonSever\r\nLast-Modified: "
         +st_time
         +"\r\nContent-Type: image/png\r\nContent-Length: "
         +st_size
         +"\r\n\r\n";
     } else if (!strcmp(dot, "html")){
       response = "HTTP/1.1 200 OK\r\nServer: TritonSever\r\nLast-Modified: "
         +st_time
         +"\r\nContent-Type: text/html\r\nContent-Length: "
         +st_size
         +"\r\n\r\n";
     } else {
       //malformed either null or extension not accepted 
       send_error(400, clntSocket);
       return;
       //response = "HTTP/1.1 400 Client Error\r\nServer: TritonSever\r\n\r\n";
     }
        
  }
       

  //use sendfile to send image 
  const char * buffer = response.c_str();

  cout << "Line 274 HttpResponse: " << buffer <<"\n"<< endl;
  ssize_t numBytesSent = send(clntSocket, buffer, strlen(buffer), 0);
  if (numBytesSent != (ssize_t)strlen(buffer))
      printf("send() failed");

 
  numBytesSent = 0;
  while (numBytesSent < size) {
    numBytesSent = sendfile(clntSocket, fd, NULL, size);
    numBytesSent += numBytesSent;
  }
}

int FrameRequest(void* raw_buf, void* message){


  printf("inside framing\n"); 
  int i;  
  struct HttpRawBuffer * rawbufptr  = (struct HttpRawBuffer *) raw_buf ;
  struct HttpMessage   * messageptr = (struct HttpMessage   *) message ;

 // cout << rawbufptr->buffer << "thor" << endl;
  for ( i=0; i< MAX_REQUEST-3; i++ ){
    char crlf1 =  rawbufptr -> buffer[i];
    char crlf2 =  rawbufptr -> buffer[i+1];
    char crlf3 =  rawbufptr -> buffer[i+2];
    char crlf4 =  rawbufptr -> buffer[i+3];
    if (crlf1 == '\r' && crlf2 == '\n' && crlf3 == '\r' && crlf4 == '\n'){
      memcpy(messageptr->buffer, rawbufptr->buffer, i+4 );
      return i+3;
    }
  }
  return -1;
}


int Parse_startline_header ( void* message, void* request, int clntSocket, string doc_root, unsigned long clnt_addr){


  printf("does it go in parse\n");

  const char delim0 [] = "\r\n";
 // const char delim1 [] = "\r\n\r\n";
  const char delim2 [] = " "; 
  const char delim3 [] = ": "; 
//  int check = -1;

  struct HttpRequest * reqptr =  ((struct HttpRequest *) request);
  char * copy  = strdup ( ((struct HttpMessage *) message )-> buffer);
  printf("Entire buffer is %s\n", copy);
  char * point = strsep( &copy, delim2);
  if (point == NULL){

    //send_error(400, clntSocket);
    return -1;   
  } 
  string get = "GET";
  if (strcmp(point, get.c_str())!=0){ 
      //send_error(400, clntSocket);
      return -1;
  }

  reqptr -> method = point;
  point = strsep (&copy, delim2);
  if (point == NULL){
    //send_error(400, clntSocket);
    return -1;   
  }
  reqptr -> path = point;

  point = strsep (&copy, delim0);
  if (point == NULL){
    //send_error(400, clntSocket);
    return -1;   
  }
  reqptr -> version = point;

  string http_v = "HTTP/1.1";
  int isit=strcmp(point, http_v.c_str());
  if (isit != 0){
    //send_error(400, clntSocket);
    return -1;
  }

  /*while (point!= NULL){
    point = strsep (&copy, delim3);
    printf("key is = %s\n", point+1);
    if (strcmp(point+1, "Host")==0){
      check += 1;
      point = strsep(&copy, delim0);
      printf("value is = %s\n", point+1);
      reqptr->host = point;

    } else if (strcmp(point+1, "Connection")==0){
      check += 1;
      point = strsep(&copy, delim0);
      printf("value is = %s\n", point+1);
      reqptr->connection = point;
    } else { 
      point = strsep(&copy, delim0);
    //  printf("token point = %s\n", point+1);
    }
  }*/

  //printf("before while loop copy is %s\n", copy);
  point = strsep(&copy, delim0);
  point = strsep(&copy, delim0);
  while (strlen(point)!=0 ){

    //point = strsep(&copy, delim0);
    char * key = strsep( &point, delim3);

    if (strlen(key) == 0 || strlen(point) ==0) { 
      //send_error(400, clntSocket);  
      return-1;
    }
    if (strcmp(key, "Host")==0){
      reqptr->host = point;
    } else if (strcmp(key, "Connection")==0){
      reqptr->connection = point;
    }

    if(strcmp(copy, "\r\n")==0){ break;}
    point = strsep(&copy, delim0);
    point = strsep(&copy, delim0);

    

  }





  printf("struct for request: %s %s %s \n", reqptr->method, reqptr->path, reqptr->version);
 // if (check!=1) { printf("didn't get all header\n");};
  printf("what is connection %s\n", reqptr->connection);

 
  send_response(clntSocket, doc_root, request, clnt_addr);
  string close = " close";
  //if connction is close 
  if ( (reqptr->connection != NULL)  && !strcmp(reqptr->connection, close.c_str())){
    return 0;
  }
 


  //change back to 1 keep alive;
  return 1;
 


}
void HandleTCPClient(int clntSock, string doc_root, unsigned long clnt_addr){
  printf("in handle tcp\n");
  char buffer[MAX_REQUEST]; 
  char repeat_buffer[MAX_REQUEST];
  memset(buffer, 0, MAX_REQUEST);

  
  struct HttpRawBuffer * raw_buf = (struct HttpRawBuffer*) malloc(sizeof(struct HttpRawBuffer));
  memset(raw_buf, 0, sizeof(HttpRawBuffer));


  int byte_in_string = 0;

  ssize_t numBytesRcvd = recv(clntSock, buffer, MAX_REQUEST, 0);
  if (numBytesRcvd < 0)
    printf("recv() failed\n");
 

  struct HttpMessage * message = (struct HttpMessage*) malloc(sizeof(struct HttpMessage));
  struct HttpRequest * request = (struct HttpRequest*) malloc(sizeof(struct HttpRequest));

  memset(message, 0, sizeof(HttpMessage));
  memset(request, 0, sizeof(HttpRequest));

 
  while (numBytesRcvd > 0) {

       
    //memset(buffer, '\0', MAX_REQUEST);
    memcpy((raw_buf->buffer)+byte_in_string, buffer, numBytesRcvd);
    byte_in_string += numBytesRcvd;

    //returns the position where the first request ends
    int did_receive = FrameRequest(raw_buf, message); 
  
    printf("after framing\n");

    //didn't receive \r\n
    while ( did_receive != -1) {
      
      int status = Parse_startline_header(message, request, clntSock, doc_root, clnt_addr);
      if (status == -1) {
        send_error(400, clntSock);
        return;
      }
      byte_in_string = byte_in_string - (did_receive+1);
      memcpy( repeat_buffer  , (raw_buf->buffer)+(did_receive+1), byte_in_string);
      memset( message->buffer, '\0'                             , MAX_REQUEST   );
      memset( raw_buf->buffer, '\0'                             , MAX_REQUEST   );
      memcpy( message->buffer, repeat_buffer                    , byte_in_string);
      memset(message, 0, sizeof(HttpMessage));
      memset(request, 0, sizeof(HttpRequest));
      if (status == 0){
        close(clntSock);
        return;
      }
      did_receive = FrameRequest(raw_buf, message);

    }

    printf("does it finish?\n");
    memset(buffer, '\0', MAX_REQUEST);
    numBytesRcvd = recv(clntSock, buffer, MAX_REQUEST, 0);   
    
    if (numBytesRcvd < 0)
      printf("recv() failed");

  }
  printf("asd?\n");

  return;
}







void DieWithUserMessage(const char *msg, const char *detail) {
  fputs(msg, stderr);
  fputs(": ", stderr);
  fputs(detail, stderr);
  fputc('\n', stderr);
  exit(1);
}

void DieWithSystemMessage(const char *msg) {
  perror(msg);
  exit(1);
}

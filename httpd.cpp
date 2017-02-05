#include <iostream>
#include "httpd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

using namespace std;
static const int MAXPENDING  = 5;
static const int MAX_REQUEST = 8192;
void * ThreadMain(void * arg);
struct ThreadArgs{
  int clnt_socket;
  string doc_root;
};


struct HttpRawBuffer{
  char buffer [MAX_REQUEST];

};

struct HttpMessage{
  char buffer[MAX_REQUEST];
}; 

struct HttpRequest{
  char * method;
  char * path;
  char * version;
  char * host; 
  char * body;
  char * connection;
};

void start_httpd(unsigned short port, string doc_root)
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


  // Mark the socket so it will listen for incoming connections
  if (listen(servSock, MAXPENDING) < 0)
    DieWithSystemMessage("listen() failed");


/*  pthread_t  client_thread[MAXPENDING];

  int i;
  for (i=0; i<MAXPENDING; i++){
    int thread_status = pthread_create(&thread_0[0], NULL, HandleTCPClient, );
    if thread_status < 0 
      cerr << "spawn thread failed" << endl;
    pthread_create(&client_thread[i], NULL, ThreadMain, threadArgs);
  }
 */


  for (;;) { // Run forever
    struct sockaddr_in clntAddr; // Client address
    // Set length of client address structure (in-out parameter)
    socklen_t clntAddrLen = sizeof(clntAddr);

    // Wait for a client to connect
    int clntSock = accept(servSock, (struct sockaddr *) &clntAddr, &clntAddrLen);
    if (clntSock < 0)
      DieWithSystemMessage("accept() failed");

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


    pthread_t thread;
    int thread_status = pthread_create( &thread, NULL, ThreadMain, thread_arg);
    if (thread_status<0)
      cerr<<"pthread create failed"<<endl;
  }


}



void * ThreadMain(void * thread_arg){

  pthread_detach (pthread_self());

  int client_socket    = ((struct ThreadArgs *) thread_arg)->clnt_socket;
  string doc_root = ((struct ThreadArgs *) thread_arg)->doc_root;


  free(thread_arg);

  HandleTCPClient( client_socket, doc_root);

  return thread_arg;

}


void send_response(int status, int clntSocket, string doc_root, void * request){

  HttpRequest * reqptr = (HttpRequest *) request;

  char * buffer;
  if (status==200) {
    buffer = (char *) "HTTP/1.1 200 OK\r\nServer: TritonSever\r\nLast-Modified: \r\nContent-Type: \r\nContent-Length: \r\n\r\n";
    doc_root = doc_root+reqptr->path;
  } else if (status == 400){
    buffer = (char *) "HTTP/1.1 400 Client Error\r\nServer: TritonSever\r\n\r\n";
  } else if (status == 403) {
    buffer = (char *) "HTTP/1.1 403 Forbidden\r\nServer: TritonSever\r\n\r\n";

  } else if (status == 404) {
    buffer = (char *) "HTTP/1.1 404 Not Found\r\nServer: TritonSever\r\n\r\n";

  } else {
    // 500
    buffer = (char *) "HTTP/1.1 500 Server Error\r\nServer: TritonSever\r\n\r\n";
  }
  ssize_t numBytesSent = send(clntSocket, buffer, strlen(buffer), 0);
  if (numBytesSent != (ssize_t)strlen(buffer))
      DieWithSystemMessage("send() failed");


}

int FrameRequest(void* raw_buf, void* message){


  printf("inside framing\n");

 // cout <<( (struct HttpMessage *) request) ->buffer << endl;
  int i;
  

  struct HttpRawBuffer * rawbufptr  = (struct HttpRawBuffer *) raw_buf ;
  struct HttpMessage   * messageptr = (struct HttpMessage   *) message ;
  for ( i=0; i< MAX_REQUEST-3; i++ ){
    char crlf1 =  rawbufptr -> buffer[i];
    char crlf2 =  rawbufptr -> buffer[i+1];
    char crlf3 =  rawbufptr -> buffer[i+2];
    char crlf4 =  rawbufptr -> buffer[i+3];
   /* printf("%c", crlf1);
    printf("%c", crlf2);
    printf("%c", crlf3);
    printf("%c", crlf4);*/
    if (crlf1 == '\r' && crlf2 == '\n' && crlf3 == '\r' && crlf4 == '\n'){
      memcpy(messageptr->buffer, rawbufptr->buffer, i+4 );
      return i+3;
    }
  }
  return -1;
}







int Parse_startline_header ( void* message, void* request, int clntSocket, string doc_root){


  printf("does it go in parse\n");

  const char delim0 [] = "\r\n";
 // const char delim1 [] = "\r\n\r\n";
  const char delim2 [] = " "; 
  const char delim3 [] = ": "; 

  struct HttpRequest * reqptr =  ((struct HttpRequest *) request);
  char * copy  = strdup ( ((struct HttpMessage *) message )-> buffer);
  char * point = strsep( &copy, delim2);
  if (point == NULL){
    send_response(400, clntSocket, doc_root, NULL );
    return -1; 
  }
  reqptr -> method = point;
  point = strsep (&copy, delim2);
  if (point == NULL){
    send_response(400, clntSocket, doc_root, NULL );
    return -1;
  }
  reqptr -> path = point;

  point = strsep (&copy, delim0);
  if (point == NULL){
    send_response(400, clntSocket, doc_root, NULL );
    return -1;
  }
  reqptr -> version = point;

  while (point!= NULL){
    point = strsep (&copy, delim3);
    if (strcmp(point, "Host")==0){
      point = strsep(&copy, delim0);
      reqptr->host = point;

    } else if (strcmp(point, "Connection")==0){
      point = strsep(&copy, delim0);
      reqptr->connection = point;
    } else { 
      point = strsep(&copy, delim0);
    }
  }
  if ( reqptr->host == NULL || reqptr->connection == NULL)
    send_response(400, clntSocket, doc_root, NULL );
    return -1;  
//  end = end + 3;
 
  send_response(200, clntSocket, doc_root, request);
  cout << "struct for request: " << reqptr->method << reqptr->path<< reqptr->version << endl;
 

  return 0;
 


}
void HandleTCPClient(int clntSock, string doc_root){
  char buffer[MAX_REQUEST]; 
  char repeat_buffer[MAX_REQUEST];
  
  struct HttpRawBuffer * raw_buf = (struct HttpRawBuffer*) malloc(sizeof(struct HttpRawBuffer));
  memset(raw_buf, 0, sizeof(HttpRawBuffer));


  int byte_in_string = 0;

  ssize_t numBytesRcvd = recv(clntSock, buffer, MAX_REQUEST, 0);
  if (numBytesRcvd < 0)
    DieWithSystemMessage("recv() failed");
 

  struct HttpMessage * message = (struct HttpMessage*) malloc(sizeof(struct HttpMessage));
  struct HttpRequest * request = (struct HttpRequest*) malloc(sizeof(struct HttpRequest));

  memset(message, 0, sizeof(HttpMessage));
  memset(request, 0, sizeof(HttpRequest));

  while (numBytesRcvd > 0) {

    memcpy((raw_buf->buffer)+byte_in_string, buffer, numBytesRcvd);
    byte_in_string += numBytesRcvd;

    //returns the position where the first request ends
    int did_receive = FrameRequest(raw_buf, message); 


  

  //  cout<< message->buffer <<endl;

    printf("after framing\n");

    //didn't receive \r\n
    if ( did_receive != -1) {
 //     printf("does it go here?\n");
 //
 //
      
      int status = Parse_startline_header(message, request, clntSock, doc_root);
      if (status == -1) 
        return;
      byte_in_string = byte_in_string - (did_receive+1);
      memcpy( repeat_buffer  , (raw_buf->buffer)+(did_receive+1), byte_in_string);
      memset( message->buffer, '\0'                             , MAX_REQUEST   );
      memset( raw_buf->buffer, '\0'                             , MAX_REQUEST   );
      memcpy( message->buffer, repeat_buffer                    , byte_in_string);
      memset(message, 0, sizeof(HttpMessage));
      memset(request, 0, sizeof(HttpRequest));

    }


    ssize_t numBytesSent = recv(clntSock, buffer, MAX_REQUEST, 0);
    if (numBytesSent < 0)
      DieWithSystemMessage("recv() failed");

  }

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

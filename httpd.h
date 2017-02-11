#ifndef HTTPD_H
#define HTTPD_H
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>

using namespace std;

static const int MAXPENDING  = 5;
static const int MAX_REQUEST = 8192;

struct ThreadArgs{
  int clnt_socket;
  string doc_root;
  unsigned long clnt_addr;
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



void start_httpd(unsigned short port, string doc_root);


void DieWithUserMessage(const char *msg, const char *detail);
// Handle error with sys msg
void DieWithSystemMessage(const char *msg);
// Print socket address
void HandleTCPClient(int clntSock, string doc_root, unsigned long clnt_addr);

int Parse_startline_header ( void* message, void* request, 
    int clntSocket, string doc_root, unsigned long clnt_addr);

int FrameRequest(void* raw_buf, void* message);

void send_response(int clntSocket, string doc_root, void * request, unsigned long clnt_addr);

int check_htaccess (string htaccess, int clntSocket, unsigned long clnt_addr);

int check_permission(string check_line, unsigned long clnt_addr);

void send_error(int status, int clntSocket);

void * ThreadMain(void * thread_arg);

#endif // HTTPD_H

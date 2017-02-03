#ifndef HTTPD_H
#define HTTPD_H
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>

using namespace std;

void start_httpd(unsigned short port, string doc_root);


void DieWithUserMessage(const char *msg, const char *detail);
// Handle error with sys msg
void DieWithSystemMessage(const char *msg);
// Print socket address
void HandleTCPClient(int clntSock);
#endif // HTTPD_H

/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "http-request.h"
#include "http-common.h"

#define MAXDATASIZE 100 // max number of bytes we can get at once 

using namespace std;

int main (int argc, char *argv[])
{
  //int server_fd = make_client_connection(argv, (const char *)PROXY_SERVER_PORT);
  //cout << server_fd;
  // command line parsing
  
  return 0;
}

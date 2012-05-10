/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

// C++ Libraries
#include <iostream>
#include <string>

// C Libraries
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

// C Network/Socket Libraries
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "http-request.h"
#include "http-common.h"

using namespace std;

int main (int argc, char *argv[])
{
  // Connect to local server
  int server_fd = make_client_connection("localhost", PROXY_SERVER_PORT);

  // Prepare request
  string client_req = "GET ";
  client_req.append(argv[1]);
  client_req.append(" HTTP/1.0\r\n\r\n");

  cout << client_req << endl;

  // Send request to server
  if (send(server_fd, client_req.c_str(), client_req.length(), 0) == -1)
    perror("send");

  // Receive response
  string server_res;

  if (get_data_from_host(server_fd, server_res) != 0)
  {
    // Couldn't get data
    exit(2);
  }

  cout << server_res << endl;

  // Close connection to local server
  close(server_fd);
  
  return 0;
}

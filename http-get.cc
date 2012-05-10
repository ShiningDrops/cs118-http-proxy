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

  // Loop until we get the last segment? packet?
  for (;;)
  {
    char res_buf[BUFSIZE];

    // Get data from remote
    int num_recv = recv(server_fd, res_buf, sizeof(res_buf), 0);
    if (num_recv < 0)
    {
      perror("recv");
      exit(1);
    }

    // If we didn't recieve anything, we hit the end
    else if (num_recv == 0)
      break;

    // Append the buffer to the response if we got something
    server_res.append(res_buf, num_recv);
  }

  cout << server_res << endl;

  // Close connection to local server
  close(server_fd);
  
  return 0;
}

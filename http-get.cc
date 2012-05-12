/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

// C++ Libraries
#include <iostream>
#include <string>
#include <fstream>

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
  // Requires 1+1 arg exactly
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <absolute_uri>\n", argv[0]);
    return 1;
  }

  // Connect to local server
  int server_fd = make_client_connection("localhost", PROXY_SERVER_PORT);
  if (server_fd < 0)
  {
    fprintf(stderr, "Couldn't establish connection to local proxy\n");
    return 1;
  }

  // Prepare request
  string client_req = "GET ";
  client_req.append(argv[1]);
  client_req.append(" HTTP/1.0\r\n\r\n");

  // Send request to server
  if (send(server_fd, client_req.c_str(), client_req.length(), 0) == -1)
  {
    perror("send");
    return 1;
  }

  // Receive response
  string server_res;

  if (get_data_from_host(server_fd, server_res) != 0)
  {
    // Couldn't get data
    fprintf(stderr, "Couldn't get data from host\n");
    return 1;
  }

  // Find location of "\r\n\r\n", after that the data starts
  size_t header_end = server_res.find("\r\n\r\n", 0);
  header_end += 4;

  // Make the filename
  string fname;

  // Find last occurance of '/', move one forward
  char *fname_slash = strrchr(argv[1], '/');
  fname_slash++;

  // If last char is slash (next char is null), name is index
  if (*(fname_slash) == '\0')
    fname = "index.html";

  // Else name is last section (not including slash)
  else
    fname = fname_slash;

  // Write to file
  ofstream local_file(fname.c_str(), ios::trunc);
  if (local_file.is_open())
  {
    local_file << (server_res.c_str() + header_end);
    local_file.close();
  }
  else
  {
    fprintf(stderr, "Unable to open file\n");
  }

  // Close connection to local server
  close(server_fd);
  
  return 0;
}

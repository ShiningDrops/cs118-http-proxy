/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include <pthread.h>

#include "http-request.h"
#include "http-common.h"

using namespace std;

/**
 * @brief Receives message fromt client and sends it to remote, back to client
 * @returns 0 if success, 1 if error
 */
int client_connected (int client_fd)
{
  // Set up buffer
  char buf[BUFSIZE];
  size_t buf_len = 0;

  // Loop until we get "\r\n\r\n"
  while (memmem(buf, buf_len, "\r\n\r\n", 4) == NULL)
  {
    int num_recv = recv(client_fd, buf + buf_len, sizeof(buf) - buf_len, 0);
    buf_len += num_recv;
  }

  // Parse the request, prepare response
  try {
    // This is the received request from the client
    HttpRequest client_req;
    client_req.ParseRequest(buf, buf_len);

    // We're not doing persistent stuff
    client_req.AddHeader("Connection", "close");

    // Format the new request
    size_t remote_length = client_req.GetTotalLength() + 1; // +1 for \0
    char *remote_req = (char *) malloc(remote_length);
    client_req.FormatRequest(remote_req);
    fprintf(stderr, "%s\n", remote_req);

    // If host not specified in first line, find it in the AddHeaders
    string remote_host;
    if (client_req.GetHost().length() == 0)
      remote_host = client_req.FindHeader("Host");
    else
      remote_host = client_req.GetHost();

    // Make connection to remote host
    int remote_fd = make_client_connection(remote_host.c_str(), REMOTE_SERVER_PORT);

    fprintf(stderr, "server: sending request\n");

    // Send new request to remote host
    if (send(remote_fd, remote_req, remote_length, 0) == -1)
      perror("send");

    free(remote_req);

    // Receive response
    string remote_res;

    // Loop until we get the last segment? packet?
    for (;;)
    {
      char res_buf[BUFSIZE];

      // Get data from remote
      int num_recv = recv(remote_fd, res_buf, sizeof(res_buf), 0);
      if (num_recv < 0)
      {
        perror("recv");
        exit(1);
      }

      // If we didn't recieve anything, we hit the end
      else if (num_recv == 0)
        break;

      // Append the buffer to the response if we got something
      remote_res.append(res_buf, num_recv);
    }

    // By now remote_res has entire response, ship that back wholesale back to the client
    if (send(client_fd, remote_res.c_str(), remote_res.length(), 0) == -1)
      perror("send");

    //fprintf(stderr, "%s\n", remote_res.c_str());

    close(remote_fd);
  }

  // Parsing error
  catch (ParseException ex) {
    printf("Exception raised: %s\n", ex.what());

    // When in doubt, assume HTTP/1.0
    string client_res = "HTTP/1.0 ";

    // Our server only has two bad responses, at least up to here
    string cmp = "Request is not GET";
    if (strcmp(ex.what(), cmp.c_str()) != 0)
      client_res += "400 Bad Request\r\n\r\n";
    else
      client_res += "501 Not Implemented\r\n\r\n";

    // Send the bad stuff!
    if (send(client_fd, client_res.c_str(), client_res.length(), 0) == -1)
      perror("send");

    // Close everything and GTFO
    close(client_fd);
    exit(2);
  }

  // Close everything
  close(client_fd);
  exit(0);

  return 0;
}

int main (int argc, char *argv[])
{
  // Create a server
  int sock_fd = make_server_listener(PROXY_SERVER_PORT);

  printf("server: waiting for connections...\n");

  // pthreads attributes
  pthread_addr_t pt_attr;
  pthread_attr_init(&pt_attr);
  pthread_attr_setdetachstate(&pt_attr, PTHREAD_CREATE_DETACHED);

  // Main accept loop
  while(1)
  {
    struct sockaddr_storage their_addr; // connector's address information
    char s[INET6_ADDRSTRLEN];
    socklen_t sin_size = sizeof their_addr;

    // Accept connections
    int client_fd = accept(sock_fd, (struct sockaddr *)&their_addr, &sin_size);
    if (client_fd == -1)
    {
      perror("accept");
      continue;
    }

    // Print out IP address
    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
    printf("server: got connection from %s\n", s);

    // Make threads
    pthread_t i_dont_care;
    pthread_create(&i_dont_care, &pt_attr, (void*) client_connected, client_fd);
/*    int pid = fork();
    if (pid == 0)
    {
      // Child process

      close(sock_fd);
      client_connected(client_fd);
    }
    else
    {
      // Parent process

      // Parent doesn't need new fd
      close(client_fd);
    }
  }*/

  return 0;
}

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

#include "http-headers.h"
#include "http-request.h"

using namespace std;

#define PORT_LISTEN "16969"
#define BACKLOG 100

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main (int argc, char *argv[])
{
  // Create address structs
  struct addrinfo hints, *res;
  int sockfd;

  // Load up address structs
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int addr_status = getaddrinfo(NULL, PORT_LISTEN, &hints, &res);
  if (addr_status != 0)
  {
    fprintf(stderr, "Cannot get info\n");
    return 1;
  }

  struct addrinfo *p;
  for (p = res; p != NULL; p = p->ai_next)
  {
    // Create the socket
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0)
    {
      perror("server: cannot open socket");
      continue;
    }

    // Set socket options
    int yes = 1;
    int opt_status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if (opt_status == -1)
    {
      perror("server: setsockopt");
      exit(1);
    }

    // Bind the socket to the port
    int bind_status = bind(sockfd, res->ai_addr, res->ai_addrlen);
    if (bind_status != 0)
    {
      close(sockfd);
      perror("server: Cannot bind socket");
      continue;
    }

    // Bind the first one we can
    break;
  }

  // No binds happened
  if (p == NULL)
  {
    fprintf(stderr, "server: failed to bind\n");
    return 2;
  }

  // Don't need the structure with address info any more
  freeaddrinfo(res);

  // Start listening
  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }

  /*
  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }
  */

  printf("server: waiting for connections...\n");

  // Main accept loop
  while(1)
  {
    struct sockaddr_storage their_addr; // connector's address information
    char s[INET6_ADDRSTRLEN];
    socklen_t sin_size = sizeof their_addr;

    // Accept connections
    int new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1)
    {
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
    printf("server: got connection from %s\n", s);

    int pid = fork();
    if (pid == 0)
    {
      // Child process

      // Child doesn't need the listener
      close(sockfd); 

      // Receive message
      char buf[2051];
      int num_recv = recv(new_fd, buf, sizeof(buf)-3, 0);

      // Add the last \r\n
      buf[num_recv] = '\r';
      buf[num_recv+1] = '\n';
      buf[num_recv+2] = '\0';
      num_recv += 2;
      fprintf(stderr, "%i, %s\n", num_recv, buf);
      //fprintf(stderr, "%i, %i, %i, %i\n", buf[num_recv-3], buf[num_recv-2], buf[num_recv-1], buf[num_recv]);

      string res = "HTTP/";
      // Parse the request
      try {
        HttpRequest req;
        req.ParseRequest(buf, num_recv);
      }
      catch (ParseException ex) {
        printf("Exception raised: %s\n", ex.what());

        // When in doubt, assume HTTP/1.0
        res += "1.0 ";

        // Our server only has two bad responses, at least up to here
        string cmp = "Request is not GET";
        if (strcmp(ex.what(), cmp.c_str()) != 0)
          res += "400 Bad Request\r\n\r\n";
        else
          res += "501 Not Implemented\r\n\r\n";

        // Send the bad stuff!
        if (send(new_fd, res.c_str(), res.length(), 0) == -1)
          perror("send");

        // Close everything and GTFO
        close(new_fd);
        exit(2);
      }

      // Close everything
      close(new_fd);
      exit(0);
    }
    else
    {
      // Parent process

      // Parent doesn't need new fd
      close(new_fd);
    }
  }

  return 0;
}

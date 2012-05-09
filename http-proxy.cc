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
#define PORT_REMOTE "80"
#define BACKLOG 100
#define BUFSIZE 2048

// get sockaddr, IPv4 or IPv6:
void *get_in_addr (struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/**
 * @brief Creates socket, binds it to port, & listens
 * @returns fd of socket, <0 if error
 */
int make_server_listener (const char *port)
{
  // Create address structs
  struct addrinfo hints, *res;
  int sock_fd;

  // Load up address structs
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int addr_status = getaddrinfo(NULL, port, &hints, &res);
  if (addr_status != 0)
  {
    fprintf(stderr, "Cannot get info\n");
    return -1;
  }

  // Loop through results, connect to first one we can
  struct addrinfo *p;
  for (p = res; p != NULL; p = p->ai_next)
  {
    // Create the socket
    sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock_fd < 0)
    {
      perror("server: cannot open socket");
      continue;
    }

    // Set socket options
    int yes = 1;
    int opt_status = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if (opt_status == -1)
    {
      perror("server: setsockopt");
      exit(1);
    }

    // Bind the socket to the port
    int bind_status = bind(sock_fd, res->ai_addr, res->ai_addrlen);
    if (bind_status != 0)
    {
      close(sock_fd);
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
    return -2;
  }

  // Don't need the structure with address info any more
  freeaddrinfo(res);

  // Start listening
  if (listen(sock_fd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }

  return sock_fd;
}

/**
 * @brief Creates socket, connects to remote host
 * @returns fd of socket, <0 if error
 */
int make_client_connection (const char *host, const char *port)
{
  // Create address structs
  struct addrinfo hints, *res;
  int sock_fd;

  // Load up address structs
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int addr_status = getaddrinfo(host, port, &hints, &res);
  if (addr_status != 0)
  {
    fprintf(stderr, "Cannot get info\n");
    return -1;
  }

  // Loop through results, connect to first one we can
  struct addrinfo *p;
  for (p = res; p != NULL; p = p->ai_next)
  {
    // Create the socket
    sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock_fd < 0)
    {
      perror("client: cannot open socket");
      continue;
    }

    // Make connection
    int connect_status = connect(sock_fd, p->ai_addr, p->ai_addrlen);
    if (connect_status < 0)
    {
      close(sock_fd);
      perror("client: connect");
      continue;
    }

    // Bind the first one we can
    break;
  }

  // No binds happened
  if (p == NULL)
  {
    fprintf(stderr, "client: failed to connect\n");
    return -2;
  }

  // Make the connection
  char s[INET6_ADDRSTRLEN];
  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
  printf("client: connecting to %s\n", s);

  // Don't need the structure with address info any more
  freeaddrinfo(res);

  return sock_fd;
}

void format_remote_request(HttpRequest &req_recv, string& remote_req)
{
  // Prepare a new request to send remotely
  char port_buffer[32];
  remote_req = "GET " + req_recv.GetPath() + " HTTP/1.0\r\n";
  remote_req += "Host: " + req_recv.GetHost() + "\r\n";
  remote_req += "Connection: close\r\n";
  // Port numbers come in shorts, so we have to do some sprintf wizardry
  sprintf(port_buffer, "Port: %d\r\n", req_recv.GetPort());
  remote_req.append(port_buffer);
  remote_req += "\r\n";
  
  //fprintf(stderr, "%s\n", remote_req.c_str());
}

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
    HttpRequest req_recv;
    req_recv.ParseRequest(buf, buf_len);

    // Format the new request
    string remote_req;
    format_remote_request(req_recv, remote_req);

    // Make connection to remote host
    int remote_fd = make_client_connection(req_recv.GetHost().c_str(), PORT_REMOTE);

    fprintf(stderr, "server: sending request\n");
    // send new request to remote host
    if (send(remote_fd, remote_req.c_str(), remote_req.length(), 0) == -1)
      perror("send");

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
    
    fprintf(stderr, "%s\n", remote_res.c_str());

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
  int sock_fd = make_server_listener(PORT_LISTEN);

  printf("server: waiting for connections...\n");

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

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
    printf("server: got connection from %s\n", s);

    int pid = fork();
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
  }

  return 0;
}

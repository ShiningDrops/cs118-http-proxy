/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

// C++ Libraries
#include <iostream>
#include <string>
#include <map>

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

/**
 * @brief Cache map typedef
 */
typedef map<string, string> cache_t;

/**
 * @brief pthread parameter structure
 */
typedef struct 
{
  int client_fd;
  cache_t *cache;
  pthread_mutex_t *mutex;
}
pt_params;

/**
 * @brief Receives message fromt client and sends it to remote, back to client
 * @returns 0 if success, 1 if error
 */
int client_connected (int client_fd, cache_t *cache, pthread_mutex_t *mutex)
{
  // Set up buffer
  string client_buffer;

  // Loop until we get "\r\n\r\n"
  while (memmem(client_buffer.c_str(), client_buffer.length(), "\r\n\r\n", 4) == NULL)
  {
    char buf[BUFSIZE];
    if (recv(client_fd, buf, sizeof(buf), 0) < 0)
    {
      perror("recv");
      return -1;
    }
    client_buffer.append(buf);
  }

  // Parse the request, prepare response
  HttpRequest client_req;
  try {
    client_req.ParseRequest(client_buffer.c_str(), client_buffer.length());
  }
  catch (ParseException ex) {
    fprintf(stderr, "Exception raised: %s\n", ex.what());

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
  }

  // We're not doing persistent stuff
  client_req.ModifyHeader("Connection", "close");

  // Format the new request
  size_t remote_length = client_req.GetTotalLength() + 1; // +1 for \0
  char *remote_req = (char *) malloc(remote_length);
  client_req.FormatRequest(remote_req);

  // If host not specified in first line, find it in the AddHeaders
  string remote_host;
  if (client_req.GetHost().length() == 0)
    remote_host = client_req.FindHeader("Host");
  else
    remote_host = client_req.GetHost();

  // String of full path, for searching/adding to cache
  string full_path = remote_host + client_req.GetPath();

  // Response "from remote host" to send back to client
  string remote_res;

  // Check cache for response
  cache_t::iterator it = cache->find(full_path);

  // In cache
  if (it != cache->end())
  {
    //fprintf(stderr, "in cache\n");
    remote_res = it->second;
  }

  // Not in cache, make a req for it
  else
  {
    // Make connection to remote host
    int remote_fd = make_client_connection(remote_host.c_str(), REMOTE_SERVER_PORT);
    if (remote_fd < 0 )
    {
      fprintf(stderr, "Couldn't make a connection to remote host %s on port %s\n", remote_host.c_str(), REMOTE_SERVER_PORT);
      free(remote_req);
      return -1;
    }

    //fprintf(stderr, "server: sending request\n");

    // Send new request to remote host
    if (send(remote_fd, remote_req, remote_length, 0) == -1)
    {
      perror("send");
      free(remote_req);
      close(remote_fd);
      return -1;
    }

    //fprintf(stderr, "not in cache\n");

    // Store data from remote host into response string
    if (get_data_from_host(remote_fd, remote_res) != 0)
    {
      free(remote_req);
      close(remote_fd);
      return -1;
    }

    // Add to the cache if Cache-Control is not private
    // This function must be atomic, as the cache is a shared object
    if (strstr(remote_res.c_str(), "Cache-Control: private") == NULL)
    {
      pthread_mutex_lock(mutex);
      cache->insert(pair<string, string>(full_path, remote_res));
      pthread_mutex_unlock(mutex);
    }

    close(remote_fd);
  }

  // By now remote_res has entire response, ship that back wholesale back to the client
  if (send(client_fd, remote_res.c_str(), remote_res.length(), 0) == -1)
  {
    perror("send");
    free(remote_req);
    return -1;
  }

  free(remote_req);
  return 0;
}

/**
 * @brief Calls client_connected()
 */
void *pt_client_connected (void *params)
{
  // Cast client_fd into an int*, dereference it, call the right function
  pt_params *p = (pt_params *) params;
  client_connected(p->client_fd, p->cache, p->mutex);

  // Close and free
  close(p->client_fd);
  free(params);

  return NULL;
}

int main (int argc, char *argv[])
{
  // Create a server
  int sock_fd = make_server_listener(PROXY_SERVER_PORT);
  if (sock_fd < 0)
  {
    fprintf(stderr, "Couldn't make proxy server listener on port %s\n", PROXY_SERVER_PORT);
    return 1;
  }

  // Initialize cache
  cache_t cache;
  pthread_mutex_t cache_mutex;
  pthread_mutex_init(&cache_mutex, NULL);

  //printf("server: waiting for connections...\n");

  // Main accept loop
  for (;;)
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
    //printf("server: got connection from %s\n", s);

    // Set up pthreads parameters
    pt_params *params = (pt_params *) malloc(sizeof(pt_params));
    params->client_fd = client_fd;
    params->cache = &cache;
    params->mutex = &cache_mutex;

    // Make threads to deal with logic
    pthread_t thread_id; // We're going to detach, IDGAF this variable can die
    pthread_create(&thread_id, NULL, pt_client_connected, (void *) params);
    pthread_detach(thread_id);
  }
  return 0;
}

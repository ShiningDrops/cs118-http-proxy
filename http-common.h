/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * @file Header for common functionality
 *
 * Skeleton for UCLA CS118 Spring quarter class
 */

#define PROXY_SERVER_PORT "16969"
#define REMOTE_SERVER_PORT "80"
#define BACKLOG 100
#define BUFSIZE 2048

int make_client_connection (const char *host, const char *port);
int make_server_listener (const char *port);
void *get_in_addr (struct sockaddr *sa);

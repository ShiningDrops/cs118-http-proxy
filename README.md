Code Description
================

A simple implementation of an HTTP proxy server and client in C++ using sockets and pthreads. **Basic** server-side caching is also implemented.

http-common
-----------

I added files, called `http-common.cc` and `http-common.h`, that contain
common functionality between the client and server, such as making connections
to hosts and retrieving data from a remote file descriptor.

http-proxy
----------

The main routine creates a server using code similar to Beej's HTTP server
example. The cache is initialized in the main routine so it is shared across
different threads. As stated on the followup discussion for question @36 on
Piazza, responses stored in the cache do not expire until http-proxy exits. The
cache is a C++ Standard Library map, which maps absolute URIs (string) to full
HTTP responses (string). Each thread must acquire a shared lock before writing
to the cache to prevent race conditions. The server, in the main routine, then
goes into an infinite loop, waiting to accept connections. Each connection
spawns a new thread using POSIX threads and processes the request. If a remote
request is made, the response from the remote host is sent to the client whole.

http-get
--------

The HTTP client makes a connection to the local proxy server with code similar
to Beej's HTTP client example. It prepares a proper HTTP/1.0 request using the
argument and sends that to the proxy server. As the spec did not say to
differentiate between response statuses, the client saves the response, without
headers, to disk. For example,

    http-get google.com

This is not a proper absolute URI. The client sends this request to the proxy,
who sends back a 400 Bad Request. There is no data in this response, so an
empty file named `google.com` is created. As per the spec, a file is named
`index.html` if the request URL ends in a `/`.

Build and Run
=============

The following lines should be added to your ~/.profile

    export PATH=/usr/local/cs/bin:$PATH
	export LD_LIBRARY_PATH=/u/cs/grad/afanasye/boost/lib:/usr/local_cs/linux/lib64/:$LD_LIBRARY_PATH

To configure environment

	./waf configure

To build/rebuild the code

	./waf

All compiled executables are located in build/, so you can run them as this:

	build/http-get

or 

	build/http-proxy
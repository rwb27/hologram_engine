/*
 * mini UDP server
 */

#ifndef __UDP_SERVER
#define __UDP_SERVER 1

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#define INET6 1
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#endif


#define ON_ERROR if(ret<0)
#define DIE_ON_ERROR(msg) if(ret<0){ perror(#msg); return -1; }
#define DIE_ON_ERROR_VOID(msg) if(ret<0){ perror(#msg); return; }
#define BACKLOG 1
#define MAX_LISTEN_SOCKETS 16

class UDPServer{
public:
	UDPServer(const char* port);
	~UDPServer();
	
	int receive(char* buffer, int buffer_length, int timeout, char * start, char * terminator);
	int reply(char* buffer, int buffer_length);
protected:
	void* get_in_addr(struct sockaddr * sa);
	
	unsigned long max_buffer_length;     //this is the size of the recieve buffer
	int socket_fds[MAX_LISTEN_SOCKETS];  //this is the socket (or more than one) we use for communication
	int last_socket_fd;					 //the last socket to be of any use to us (i.e. where the packet came from)
	int n_sockets;                       //the number of sockets we have
	int biggest_fd;                      //this is the hightest FD, needed for the select() call in receive.

	
	struct sockaddr_storage client_address; //the address of the (latest) hapless fool trying to connect
	socklen_t client_address_length;	 //the size 
};

#endif
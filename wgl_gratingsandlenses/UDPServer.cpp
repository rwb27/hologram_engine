/*
 *  network.cpp
 *  cmd2
 *
 *  Created by Richard Bowman on 13/02/2009.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "UDPServer.h"

UDPServer::UDPServer(const char* port){
	//initialise the client address vairable
	client_address_length=sizeof(client_address); 

	//thanks to beej (beej.us/guide/bgnet/) for the amazing network code howto
	struct addrinfo hints, *server_info, *p;
	int ret=0;
	
	
#ifdef WIN32
	WSADATA wsaData;
	ret = WSAStartup(MAKEWORD(1,1),&wsaData);
	DIE_ON_ERROR_VOID("Couldn't Initialise WinSock.\n");
#endif
	
	memset(&hints, 0, sizeof(hints)); //make sure hints is empty
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; //use this machine's IP
	
	ret = getaddrinfo(NULL, port, &hints, &server_info); //find this machine's IP, with port as defined in the macro
	DIE_ON_ERROR_VOID("Failed to get address\n")
	
	// loop through results and bind to whatever we can!  Perhaps future code could be more discriminate
	n_sockets=0; //this will be the number of sockets we've got.  We work with the first free slot (socket_fds[n_sockets])
	             //and increment n_sockets every time we find one we want to keep.
	biggest_fd=0;
	for(p = server_info; p!=NULL; p=p->ai_next){
		ret = socket_fds[n_sockets] = socket(p->ai_family, p->ai_socktype, p->ai_protocol); //try to create the socket
		ON_ERROR{
			perror("Couldn't create socket.");
			continue;
		} //if we can't, try the next result
		
		ret = bind(socket_fds[n_sockets], p->ai_addr, p->ai_addrlen); //try to bind the socket
		ON_ERROR{
#ifdef WIN32
			closesocket(socket_fds[n_sockets]);
#else
			close(socket_fds[n_sockets]);
#endif
			perror("Couldn't bind socket to port.");
			continue;
		} //if we can't, try the next result
		
		n_sockets++; //if we get to here, we've successfully bound the socket, so we try another one
		if(socket_fds[n_sockets-1]>biggest_fd) biggest_fd=socket_fds[n_sockets-1];
		
		//let the user know that we've bound to an address
#ifdef WIN32 //if we're compiling under Vista, we might have an IPv6 address, which would break this...
		WCHAR s[INET_ADDRSTRLEN];
		DWORD address_string_length=sizeof(s);
		WSAAddressToString((struct sockaddr*)p,INET_ADDRSTRLEN,NULL,s,&address_string_length);
#else
		char s[INET6_ADDRSTRLEN];
		inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p), s, sizeof(s));
#endif
		printf("Socket %d bound to port %s on IP address %s.\n",socket_fds[n_sockets],port,s);
	}
	
	ret = (n_sockets>0)?0:-1;
	DIE_ON_ERROR_VOID("Failed to bind socket on any available address, giving up and going home in the huff.")
	
	freeaddrinfo(server_info); //we no longer care who we are, might as well forget!
	
}


UDPServer::~UDPServer(){		
#ifdef WIN32
	for(int i=0; i<n_sockets; i++) closesocket(socket_fds[i]);
	WSACleanup();
#else
	for(int i=0; i<n_sockets; i++) close(socket_fds[i]);
#endif
}
	
int UDPServer::receive(char* buffer, int buffer_length, int timeout_usec, char* terminator){
	/*
	 * This function listens for timeout_usec microseconds on all the IP addresses we can bind to, and puts the first packet it
	 * finds into the buffer.
	 */
	int ret=0, message_length, packet_length;
	
	struct timeval timeout;  //this bit sets our attention span...
	timeout.tv_sec=0;
	timeout.tv_usec=timeout_usec;
	fd_set monitored_fds;
	FD_ZERO(&monitored_fds);
	for(int i=0; i<n_sockets; i++) FD_SET(socket_fds[i],&monitored_fds); //monitor all the sockets we've got
	select(biggest_fd+1,&monitored_fds,NULL,NULL,&timeout); //this waits until someone talks to us or we get bored
	
	bool somebodylovesyou=false;           //from now on in this function we'll only use the first socket we heard from.
	for(int i=0; i<n_sockets; i++){
		if(FD_ISSET(socket_fds[i],&monitored_fds)){
			last_socket_fd=socket_fds[i];  //this is the latest socket to yield any packets
			somebodylovesyou=true;
			break;
		}
	}
	
	if(somebodylovesyou){ //if we're being talked to		
		message_length=0; //we recieve multiple packets and concatenate them into one message.
		do{
			if(message_length>0){ //if this is not our first time, wait for another packet or time out.
				FD_ZERO(&monitored_fds);
				FD_SET(last_socket_fd,&monitored_fds);
				select(last_socket_fd+1,&monitored_fds,NULL,NULL,&timeout); //this waits for the next packet until we get bored
				ret = FD_ISSET(last_socket_fd,&monitored_fds)?0:-1;
				DIE_ON_ERROR("Timed out before the message finished.\n");
			}

			//recieve a packet (have faith...)
			//!TODO! we need to check this is ok the second/third time- it should time out
			packet_length = recvfrom(last_socket_fd, buffer+message_length, buffer_length-message_length-1, 0, (struct sockaddr*) &client_address, &client_address_length);
			ret = (packet_length>0)?0:-1;
			DIE_ON_ERROR("Problems recieving packet.");
			
			message_length+=packet_length;
			buffer[message_length]='\0';
		}while(strstr(buffer,terminator)==NULL && message_length<(buffer_length-1));
		
		buffer[message_length]='\0'; //terminated.
		
		return message_length;
	}else{
		return 0;
	}
}
		

void* UDPServer::get_in_addr(struct sockaddr *sa){
	if(sa->sa_family == AF_INET){
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}else{
#ifdef INET6
		return &(((struct sockaddr_in6*)sa)->sin6_addr);
#else
		return NULL;
#endif
	}
}

int UDPServer::reply(char* buffer, int buffer_length){
	//respond to the address that the last packet came from.
	return sendto(last_socket_fd,buffer, buffer_length, 0, (struct sockaddr*)&client_address, client_address_length);
}

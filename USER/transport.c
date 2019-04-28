/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *    Sergio R. Caprile - "commonalization" from prior samples and/or documentation extension
 *******************************************************************************/

//#include <sys/types.h>

#if !defined(SOCKET_ERROR)
	/** error in socket operation */
	#define SOCKET_ERROR -1
#endif

#if defined(WIN32)
/* default on Windows is 64 - increase to make Linux and Windows the same */
#define FD_SETSIZE 1024
#include <winsock2.h>
#include <ws2tcpip.h>
#define MAXHOSTNAMELEN 256
#define EAGAIN WSAEWOULDBLOCK
#define EINTR WSAEINTR
#define EINVAL WSAEINVAL
#define EINPROGRESS WSAEINPROGRESS
#define EWOULDBLOCK WSAEWOULDBLOCK
#define ENOTCONN WSAENOTCONN
#define ECONNRESET WSAECONNRESET
#define ioctl ioctlsocket
#define socklen_t int
#else
#define INVALID_SOCKET SOCKET_ERROR
//#include <sys/socket.h>
//#include <sys/param.h>
//#include <sys/time.h>
//#include <netinet/in.h>
//#include <netinet/tcp.h>
//#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
//#include <unistd.h>
#include <errno.h>
//#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#endif
#include "lwip/api.h"
#include "lwip_comm.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/opt.h"

#if defined(WIN32)
#include <Iphlpapi.h>
#else
//#include <sys/ioctl.h>
//#include <net/if.h>
#endif


int g_sockfd = INVALID_SOCKET;
#define MQTT_CLIENT_RX_BUFSIZE	2000	//接收缓冲区长度
/**
This simple low-level implementation assumes a single connection for a single thread. Thus, a static
variable is used for that connection.
On other scenarios, the user must solve this by taking into account that the current implementation of
MQTTPacket_read() has a function pointer for a function call to get the data to a buffer, but no provisions
to know the caller or other indicator (the socket id): int (*getfn)(unsigned char*, int)
*/
struct netconn *mqtt_clientconn;
u8 mqtt_client_recvbuf[MQTT_CLIENT_RX_BUFSIZE];	//TCP客户端接收数据缓冲区


int transport_sendPacketBuffer(int sock, unsigned char* buf, int buflen)
{
	int rc = 0;
	rc = write(sock, buf, buflen);
	return rc;
}


int transport_getdata(unsigned char* buf, int count)
{
	int rc = recv(g_sockfd, buf, count, 0);
	//printf("received %d bytes count %d\n", rc, (int)count);
	return rc;
}

int transport_getdatanb(void *sck, unsigned char* buf, int count)
{
	int sock = *((int *)sck); 	/* sck: pointer to whatever the system may use to identify the transport */
	/* this call will return after the timeout set on initialization if no bytes;
	   in your system you will use whatever you use to get whichever outstanding
	   bytes your socket equivalent has ready to be extracted right now, if any,
	   or return immediately */
	int rc = recv(sock, buf, count, 0);	
	if (rc == -1) {
		/* check error conditions from your system here, and return -1 */
		return 0;
	}
	return rc;
}

/**
return >=0 for a socket descriptor, <0 for an error code
@todo Basically moved from the sample without changes, should accomodate same usage for 'sock' for clarity,
removing indirections
*/
int transport_open(char *addr, int port)	//create a socket and connect the server
{
	int sockfd = -1;
	struct hostent *server;
	struct sockaddr_in serv_addr;
	int timeout = 1000;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0)
		printf("[ERROR] Create socket failed.\n");
	server = gethostbyname(addr);
	if(server == NULL)
		printf("[ERROR] Get host ip failed\n");
	
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
	
	if(connect(sockfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0){
		printf("[ERROR] connect failed\n");
		return -1;
	}
	
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
	return sockfd;
}

int transport_close(int sock)
{
	int rc;

	rc = shutdown(sock, SHUT_WR);
	rc = recv(sock, NULL, (size_t)0, 0);
	rc = close(sock);

	return rc;
}

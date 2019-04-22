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
#if defined(WIN32)
#include <Iphlpapi.h>
#else
//#include <sys/ioctl.h>
//#include <net/if.h>
#endif
#define MQTT_REMOTE_PORT				8087	//定义远端主机的IP地址
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

/*this function used to transfer char type IP address to int IP address*/
static void getIPaddress(char *ipaddr, unsigned char *ip_addr)
{
	if(ipaddr == NULL || ip_addr == NULL)
		return;
	char *p = strtok(ipaddr, ".");
	while(p){
		*ip_addr = atoi(p);
		p = strtok(NULL, ".");
		ip_addr++;
	}
}

int transport_sendPacketBuffer(int sock, unsigned char* buf, int buflen)
{
	int rc = 0;
	//rc = write(sock, buf, buflen);
	rc = netconn_write(mqtt_clientconn ,buf,strlen((char*)buf),NETCONN_COPY); //发送tcp_server_sentbuf中的数据
	if(rc != ERR_OK)
	{
		printf("发送失败\r\n");
	}
	return rc;
}


int transport_getdata(unsigned char* buf, int count)
{
	int recv_err;
	struct netbuf *recvbuf;
	if((recv_err = netconn_recv(mqtt_clientconn,&recvbuf)) != ERR_OK){
		return -1;
	}
	return recv_err;
}

int transport_getdatanb(void *sck, unsigned char* buf, int count)
{
	int recv_err;
	struct netbuf *recvbuf;
	if((recv_err = netconn_recv(mqtt_clientconn,&recvbuf)) != ERR_OK){
		return -1;
	}
	
	return recv_err;
}

/**
return >=0 for a socket descriptor, <0 for an error code
@todo Basically moved from the sample without changes, should accomodate same usage for 'sock' for clarity,
removing indirections
*/
int transport_open(char *addr, int port)	//create a socket and connect the server
{
	err_t err;
	static ip_addr_t server_ipaddr, loca_ipaddr;
	static u16_t 		 server_port, loca_port;
	server_port = MQTT_REMOTE_PORT;			//定义远程服务器端口
	getIPaddress(addr,lwipdev.remoteip);	//将char型数组转换成整形数组
	IP4_ADDR(&server_ipaddr, lwipdev.remoteip[0],lwipdev.remoteip[1], lwipdev.remoteip[2],lwipdev.remoteip[3]);
	mqtt_clientconn = netconn_new(NETCONN_TCP);  //创建一个TCP链接
	err = netconn_connect(mqtt_clientconn,&server_ipaddr,server_port);//连接服务器
	if(err != ERR_OK){
		netconn_delete(mqtt_clientconn); //返回值不等于ERR_OK,删除tcp_clientconn连接
		goto END;
	}
	mqtt_clientconn->recv_timeout = 10;
	netconn_getaddr(mqtt_clientconn,&loca_ipaddr,&loca_port,1); //获取本地IP主机IP地址和端口号
	printf("连接上服务器%d.%d.%d.%d,本机端口号为:%d\r\n",lwipdev.remoteip[0],lwipdev.remoteip[1], lwipdev.remoteip[2],lwipdev.remoteip[3],loca_port);
	goto END;
END:
	return err;
	
}
int transport_close(int sock)
{
	netconn_close(mqtt_clientconn);
	netconn_delete(mqtt_clientconn);
	return 0;
}

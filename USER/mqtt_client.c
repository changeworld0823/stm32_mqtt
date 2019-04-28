#include "mqtt_client.h"
#include "lwip/opt.h"
#include "lwip_comm.h"
#include "lwip/lwip_sys.h"
#include "lwip/api.h"
#include "includes.h"
#include "transport.h"
#include "MQTTPacket.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"

//TCP客户端任务
#define MQTT_CLIENT_PRIO			6
//任务堆栈大小
#define MQTT_CLIENT_STK_SIZE	300
//任务堆栈
OS_STK MQTT_CLIENT_TASK_STK[MQTT_CLIENT_STK_SIZE];


#define KEEPALIVE_INTERVAL	0

INT8U mqtt_client_pub(int sock, char *topic, char *payload, unsigned char payloadlen);
void mqtt_client_sub(int mysock, char *topicname);
int mqtt_connect(char *host, int port);
void mqtt_client_ping(int mysock);	


static void mqtt_client_thread(void *arg)
{
	static int mqtt_client_state = 0;
	while(1)
	{
		switch(mqtt_client_state){
			case 0:
				g_sockfd = mqtt_connect("192.168.1.11", 8087);
				if(g_sockfd < 0)
					break;
				mqtt_client_state ++;
				break;
			case 1:
				mqtt_client_pub(g_sockfd, "topic", "helloworld", 10);
				mqtt_client_state++;
				break;
			case 2:
				mqtt_client_sub(g_sockfd,"testtopic");
				mqtt_client_state++;
				break;
			default: break;
		}
		
		OSTimeDlyHMSM(0,0,0,10);  //延时10ms
	}
}

INT8U mqtt_client_init(void)
{
	INT8U res;
	OS_CPU_SR cpu_sr;
	
	OS_ENTER_CRITICAL();	//关中断
	res = OSTaskCreate(mqtt_client_thread,(void*)0,(OS_STK*)&MQTT_CLIENT_TASK_STK[MQTT_CLIENT_STK_SIZE-1],MQTT_CLIENT_PRIO); //创建TCP客户端线程
	OS_EXIT_CRITICAL();		//开中断
	
	return res;
}

INT8U mqtt_client_pub(int sock, char *topic, char *payload, unsigned char payloadlen)
{
	int rc = 0;
	char buf[200];
	int buflen = sizeof(buf);
	MQTTString topicString = MQTTString_initializer;	//初始化MQTT主题结构体
	int len = 0;	
	topicString.cstring = topic;

	len = MQTTSerialize_publish((unsigned char *)(buf+len), buflen - len,
		0, 0, 0, 0, topicString, (unsigned char *)payload, payloadlen);
	
	rc = transport_sendPacketBuffer(sock, (unsigned char *)buf, len);
	if(rc == len)
		printf("Successfully publish\n");
	else
		printf("Publish failed\n");
	return 0;
}

void mqtt_client_ping(int mysock)
{
	int len = 0;
	char buf[200];
	int buflen = sizeof(buf);
	len = MQTTSerialize_pingreq((unsigned char*)buf, buflen);
	transport_sendPacketBuffer(mysock, (unsigned char*)buf, len);
}

int mqtt_connect(char *host, int port)
{
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	int len = 0,rc = 0;
	char buf[200];
	int buflen = sizeof(buf);
	int mysock = -1;
	data.clientID.cstring = "mqtt_client_switch";
	data.keepAliveInterval = KEEPALIVE_INTERVAL;
	data.cleansession = 1;
	data.username.cstring = "";
	data.password.cstring = "";
	data.MQTTVersion = 4;
	
	mysock = transport_open(host, port);
	if(mysock < 0){
		return mysock;
	}

	len = MQTTSerialize_connect((unsigned char*)buf, buflen, &data);
	rc = transport_sendPacketBuffer(mysock, (unsigned char*)buf, len);
	if(rc == len)
		printf("Send CONNECT successfully");
	else
		printf("send CONNECT failed\n");
	printf("MQTT connect OK\n");
	return mysock;
}

void mqtt_client_sub(int mysock,char *topicname)
{
	int req_qos = 1;
	unsigned short msgid = 1;
	char buf[200];
	int buflen = sizeof(buf);
	int len = 0,rc = 0;
	MQTTString topicString = MQTTString_initializer;
	topicString.cstring = topicname;
	len = MQTTSerialize_subscribe((unsigned char*)buf, buflen, 0, msgid, 1, &topicString, &req_qos);
	rc = transport_sendPacketBuffer(mysock, (unsigned char *)buf, len);
	if(rc == len)
		printf("send SUNSCRIBE suc.\n");
	else printf("send SUBSCRIBE failed\n");
	printf("client sub:%s\n",topicString.cstring);
}

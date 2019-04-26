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
#define MQTT_CLIENT_PRIO		6
//任务堆栈大小
#define MQTT_CLIENT_STK_SIZE	300
//任务堆栈
OS_STK MQTT_CLIENT_TASK_STK[MQTT_CLIENT_STK_SIZE];
INT8U mqtt_client_pub(char *host_addr, int portnum);
void mqtt_thread(void *arg);

static void mqtt_client_thread(void *arg)
{
	static int mqtt_client_state = 1;
	int rc;
	while(1)
	{
		switch(mqtt_client_state){
			case 0:
				rc = transport_open(IP_ADDR,MQTT_REMOTE_PORT);
				if(rc == ERR_OK)
					mqtt_client_state ++;
				break;
			case 1:
				mqtt_client_pub(IP_ADDR, MQTT_REMOTE_PORT);
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
	res = OSTaskCreate(mqtt_thread,(void*)0,(OS_STK*)&MQTT_CLIENT_TASK_STK[MQTT_CLIENT_STK_SIZE-1],MQTT_CLIENT_PRIO); //创建TCP客户端线程
	OS_EXIT_CRITICAL();		//开中断
	
	return res;
}

INT8U mqtt_client_pub(char *host_addr, int portnum)
{
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;	//初始化MQTT连接数据结构体
	int rc = 0;
	char buf[200];
	int buflen = sizeof(buf);
	int mysock = 0;
	MQTTString topicString = MQTTString_initializer;	//初始化MQTT主题结构体
	char *payload = "hello world payload";
	int payloadlen = strlen("hello world payload");
	int len = 0;
	char *host = "192.168.1.11";
	int port = 8087;
	
	if(host_addr != NULL)	host = host_addr;		//获取服务器地址
	if(portnum != 0)	port = portnum;		//获取服务器监听端口
	
	mysock = transport_open(host,port);				//建立MQTT连接
	if(mysock < 0)	return mysock;
	printf("Sending to hostname %s port %d\n", host, port);
	
	data.clientID.cstring = "me";
	data.keepAliveInterval = 20;
	data.cleansession = 1;
	data.username.cstring = "admin";
	data.password.cstring = "password";
	data.MQTTVersion = 4;
	
	len = MQTTSerialize_connect((unsigned char *)buf, buflen, &data);
	
	topicString.cstring = "topic";

	len += MQTTSerialize_publish((unsigned char *)(buf+len), buflen - len,
		0, 0, 0, 0, topicString, (unsigned char *)payload, payloadlen);
	
	len += MQTTSerialize_disconnect((unsigned char *)(buf + len), buflen - len);
	
	rc = transport_sendPacketBuffer(mysock, (unsigned char *)buf, len);
	if(rc == len)
		printf("Successfully publish\n");
	else
		printf("Publish failed\n");
exit:
	transport_close(mysock);
	
	return 0;
}

int toStop = 0;
void cfinish(int sig)
{
	//signal(SIGINT, NULL);
	toStop = 1;
}

void stop_init(void)
{
	//signal(SIGINT,cfinish);
	//signal(SIGTERM, cfinish);
}
INT8U mqtt_client_sub(char *host_addr, char *portnum)
{
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	int rc = 0;
	int mysock = 0;
	unsigned char buf[200];
	int buflen = sizeof(buf);
	int msgid = 1;
	MQTTString topicString = MQTTString_initializer;
	int req_qos = 0;
	char *payload = "mypayload";
	int payloadlen = strlen(payload);
	int len = 0;
	char *host = "192.168.1.130";
	int port = 1883;
	
	stop_init();
	if(host_addr != NULL)	host = host_addr;		//获取服务器地址
	if(portnum != NULL)	port = atoi(portnum);		//获取服务器监听端口
	
	mysock = transport_open(host, port);
	if(mysock < 0)	return mysock;
	printf("Sending to hostname %s port %d\n", host, port);
	
	data.clientID.cstring = "me";
	data.keepAliveInterval = 20;
	data.username.cstring = "testuser";
	data.password.cstring = "password";
	
	len = MQTTSerialize_connect(buf, buflen, &data);
	rc = transport_sendPacketBuffer(mysock, buf, len);
	
	if(MQTTPacket_read(buf, buflen, transport_getdata) == CONNACK)
	{
		unsigned char sessionPresent, connack_rc;
		if(MQTTDeserialize_connack(&sessionPresent, &connack_rc, buf, buflen) != 1
			|| connack_rc != 0)
		{
			printf("unable to connect, return code %d\n", connack_rc);
			goto exit;
		}
	}
	else goto exit;
	
	topicString.cstring = "subtopic";
	len = MQTTSerialize_subscribe(buf, buflen, 0, msgid, 1, &topicString, &req_qos);
	
	rc = transport_sendPacketBuffer(mysock, buf, len);
	if(MQTTPacket_read(buf, buflen, transport_getdata) == SUBACK)
	{
		unsigned short submsgid;
		int subcount;
		int granted_qos;
		
		rc = MQTTDeserialize_suback(&submsgid, 1, &subcount, &granted_qos, buf, buflen);
		if(granted_qos != 0)
		{
			printf("granted qos != 0, %d\n",granted_qos);
			goto exit;
		}
	}
	else goto exit;
	
	printf("disconnecting\n");
	len = MQTTSerialize_disconnect(buf, buflen);
	rc = transport_sendPacketBuffer(mysock, buf, len);
exit:
	transport_close(mysock);
	
	return 0;
}

#define KEEPALIVE_INTERVAL	20
//time_t old_t;
void start_ping_timer(void)
{
	//time(&old_t);
	//old_t += KEEPALIVE_INTERVAL / 2 + 1;
}

int time_to_ping(void)
{
	//time_t t;
	//time(&t);
	//if(t >= old_t)
	//	return 1;
	return 0;
}

INT8U mqtt_client_ping()
{
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	int rc = 0;
	int mysock = 0;
	unsigned char buf[200];
	int buflen = sizeof(buf);
	int len = 0;
	char *host = "192.168.1.130";
	int port = 1883;
	
	mysock = transport_open(host, port);
	if(mysock < 0)
		return mysock;
	printf("sending to hostname %s port %d\n", host, port);
	
	data.clientID.cstring = "me";
	data.keepAliveInterval = KEEPALIVE_INTERVAL;
	data.cleansession = 1;
	data.username.cstring = "testuser";
	data.password.cstring = "password";
	
	len = MQTTSerialize_connect(buf, buflen, &data);
	rc = transport_sendPacketBuffer(mysock, buf, len);
	
}


#if 1
void mqtt_thread(void *arg)
{
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	MQTTString receivedTopic;
	int rc = 0;
	char buf[200];
	int buflen = sizeof(buf);
	int mysock = 0;
	MQTTString topicString = MQTTString_initializer;
	int payloadlen_in;
	unsigned char *payload_in;
	unsigned short msgid = 1;
	int subcount;
	int granted_qos = 0;
	unsigned char sessionPresent, connack_rc;
	unsigned short submsgid;
	int len = 0;
	int req_qos = 1;
	unsigned char dup;
	int qos;
	unsigned char retained;
	
	char *host = "192.168.1.11";
	int port = 8087;
	uint8_t msgtypes = CONNECT;
	//uint32_t curtick = xTaskGetTickCount();
	printf("socket connect to server\n");
	mysock = transport_open(host, port);
	if(mysock < 0)
		return ;
	printf("sending to hostname %s port %d\n", host, port);
	data.clientID.cstring = "me";
	data.keepAliveInterval = 50;
	data.cleansession = 1;
	data.username.cstring = "";
	data.password.cstring = "";
	data.MQTTVersion = 4;
	while(1)
	{
		/*if((xTaskGetTickCount() - curtick) > (data.keepAliveInterval/2*1000))
		{
			if(msgtypes == 0)
			{
				curtick = xTaskGetTickCount();
				msgtypes = PINGREQ;
			}
		}
		*/
		switch(msgtypes)
		{
			case CONNECT:
				len = MQTTSerialize_connect(buf, buflen, &data);
				rc = transport_sendPacketBuffer(mysock, (unsigned char*)buf, len);
				if(rc == len)
					printf("Send CONNECT successfully");
				else
					printf("send CONNECT failed\n");
				printf("MQTT connect OK\n");
				msgtypes = CONNACK;
				break;
			case CONNACK:
				if (MQTTDeserialize_connack(&sessionPresent, &connack_rc, buf, buflen) != 1 || connack_rc != 0)
				{
					printf("unable to connect,return code %d\n",connack_rc);
				}
				else printf("MQTT is connect OK\n");
				msgtypes = SUBSCRIBE;
				break;
			case SUBSCRIBE:
				topicString.cstring = "ledtest";
				len = MQTTSerialize_subscribe(buf, buflen, 0, msgid, 1, &topicString, &req_qos);
				rc = transport_sendPacketBuffer(mysock, (unsigned char *)buf, len);
				if(rc == len)
					printf("send SUNSCRIBE suc.\n");
				else printf("send SUBSCRIBE failed\n");
				printf("client sub:%s\n",topicString.cstring);
				msgtypes = 0;
				break;
			case SUBACK:        
				rc = MQTTDeserialize_suback(&submsgid, 1, &subcount, &granted_qos, buf, buflen);                                                        
				printf("granted qos is %d\n", granted_qos);                                                                
				msgtypes = 0;
				break;
			case PUBLISH:        
				rc = MQTTDeserialize_publish(&dup, &qos, &retained, &msgid, &receivedTopic, &payload_in, &payloadlen_in, buf, buflen);
				printf("message arrived : %s\n", payload_in);
				if(strstr(payload_in,"on"))
				{
					printf("LED on!!");
				}
				else if(strstr(payload_in,"off"))
				{
					printf("LED off!!");
				}
				if(qos == 1)
				{
					printf("publish qos is 1,send publish ack.");
					memset(buf,0,buflen);
					len = MQTTSerialize_ack(buf,buflen,PUBACK,dup,msgid);   //publish ack                        
					rc = transport_sendPacketBuffer(mysock, (unsigned char*)buf, len);
					if (rc == len)
							printf("send PUBACK Successfully");
					else
							printf("send PUBACK failed");                                        
				}
				msgtypes = 0;
				break;
			case PUBACK:        
				printf("PUBACK!");
				msgtypes = 0;
				break;
			case PUBREC:        
				printf("PUBREC!");     //just for qos2
				break;
			case PUBREL:        
				printf("PUBREL!");        //just for qos2
				break;
			case PUBCOMP:        
				printf("PUBCOMP!");        //just for qos2
				break;
			case PINGREQ:        
				len = MQTTSerialize_pingreq(buf, buflen);
				rc = transport_sendPacketBuffer(mysock, (unsigned char*)buf, len);
				if (rc == len)
						printf("send PINGREQ Successfully\n");
				else
						printf("send PINGREQ failed\n");        
				printf("time to ping mqtt server to take alive!");
				msgtypes = 0;
				break;
			case PINGRESP:        
				printf("mqtt server Pong");                                                        
				msgtypes = 0;
				break;
		}
		memset(buf,0,buflen);
		rc=MQTTPacket_read(buf, buflen, transport_getdata);        
		if(rc >0)
		{
				msgtypes = rc;
				printf("MQTT is get recv:");
		}
		//gpio_write(&gpio_led, !gpio_read(&gpio_led));
	}
 
exit:
    transport_close(mysock);
    printf("mqtt thread exit.");
    //vTaskDelete(NULL);
}
#endif
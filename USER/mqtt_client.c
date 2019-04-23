#include "mqtt_client.h"
#include "lwip/opt.h"
#include "lwip_comm.h"
#include "lwip/lwip_sys.h"
#include "lwip/api.h"
#include "includes.h"
#include "transport.h"

//TCP客户端任务
#define MQTT_CLIENT_PRIO		6
//任务堆栈大小
#define MQTT_CLIENT_STK_SIZE	300
//任务堆栈
OS_STK MQTT_CLIENT_TASK_STK[MQTT_CLIENT_STK_SIZE];

static void mqtt_client_thread(void *arg)
{
	while(1)
	{
		transport_open(IP_ADDR,MQTT_REMOTE_PORT);
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

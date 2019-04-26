#ifndef __MQTT_CLIENT_H
#define __MQTT_CLIENT_H
#include "sys.h"
#include "includes.h"

#define MQTT_REMOTE_PORT		1883	//定义远端主机的IP地址
#define IP_ADDR					"192.168.203.132"
INT8U mqtt_client_init(void);  //tcp客户端初始化(创建tcp客户端线程)
#endif

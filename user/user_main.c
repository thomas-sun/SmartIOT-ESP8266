/* main.c -- MQTT client example
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "mqtt.h"
#include "wifi.h"
#include "config.h"
#include "debug.h"
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"
#include "driver/key.h"
#include "user_io.h"
#include "smart_config.h"




#include "wifi.h"
#include "user_interface.h"
#include "osapi.h"
#include "espconn.h"
#include "os_type.h"
#include "mem.h"
#include "mqtt_msg.h"
#include "debug.h"
#include "user_config.h"
#include "config.h"


void ICACHE_FLASH_ATTR
ota(const char *server_ip, int port);


MQTT_Client mqttClient;
LOCAL struct keys_param keys;
LOCAL struct single_key_param *single_key;



/******************************************************************************
 * FunctionName : user_plug_long_press
 * Description  : key's long press function, needed to be installed
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_long_press(void)
{
	INFO("user_plug_long_press\r\n");
}

/******************************************************************************
 * FunctionName : user_plug_short_press
 * Description  : key's short press function, needed to be installed
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_short_press(void)
{
	INFO("user_plug_short_press\r\n");
	
	sysCfg.smart_config = 1;
	CFG_Save();
	
	system_restart();

}





void button_init()
{
	single_key = key_init_single(BTN_PIN, get_gpio_name(BTN_PIN), get_gpio_function(BTN_PIN),
		                                    NULL, user_short_press);
	keys.key_num = 1;
	keys.single_key = &single_key;
	key_init(&keys);

}



LOCAL const char * ICACHE_FLASH_ATTR get_my_id(void)
{
	// Use MAC address for Station as unique ID
	static char my_id[13];
	get_mac_address(my_id);
	return my_id;
}


void mqttConnectedCb(uint32_t *args)
{
	char topic[80];
	MQTT_Client* client = (MQTT_Client*)args;
	
	
	os_sprintf(topic, "%s/%s",sysCfg.manager_id, get_my_id());
	
	INFO("MQTT: Connected\r\nSubscribe:%s\r\n",topic);
	
	
	MQTT_Subscribe(client, topic, 0);
	
	
	//MQTT_Subscribe(client, "/mqtt/topic/0", 0);
	//MQTT_Subscribe(client, "/mqtt/topic/1", 1);
	//MQTT_Subscribe(client, "/mqtt/topic/2", 2);

	//MQTT_Publish(client, "/mqtt/topic/0", "hello0", 6, 0, 0);
	//MQTT_Publish(client, "/mqtt/topic/1", "hello1", 6, 1, 0);
	//MQTT_Publish(client, "/mqtt/topic/2", "hello2", 6, 2, 0);

}


void mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Disconnected\r\n");
}

void mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Published\r\n");
}

void delay_ms(int ms)
{
	int x;
	for(x = 0; x < ms; x++)
		os_delay_us(1000);
}



void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	
#if 1
	if(os_strcmp(data, "press 0") == 0) {
#if defined(ESP01)
		mcu_command("press 1");
#else
		set_gpio(SW1_PIN, 0);
		delay_ms(750);
		set_gpio(SW1_PIN, 1);
#endif
	} else if(os_strcmp(data, "press 1") == 0) {
#if defined(ESP01)
		mcu_command("press 2");
#else		
		set_gpio(SW2_PIN, 0);
		delay_ms(750);
		set_gpio(SW2_PIN, 1);
#endif		
	} else if(os_strcmp(data, "press 2") == 0) {
#if defined(ESP01)
		mcu_command("press 3");
#else		
		set_gpio(SW3_PIN, 0);
		delay_ms(750);
		set_gpio(SW3_PIN, 1);
#endif		
	} else if(os_strcmp(data, "press 3") == 0) {
#if defined(ESP01)
		mcu_command("press 4");
#else		
		set_gpio(SW4_PIN, 0);
		delay_ms(750);
		set_gpio(SW4_PIN, 1);
#endif		
	}

#else
	char *topicBuf = (char*)os_zalloc(topic_len+1),
			*dataBuf = (char*)os_zalloc(data_len+1);

	MQTT_Client* client = (MQTT_Client*)args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);
	os_free(topicBuf);
	os_free(dataBuf);
#endif	
}

void init_mqtt()
{
	static int b_init = 0;
	char topic[40];
	if(b_init == 1)
		return;
	
	b_init = 1;
	
	
	os_sprintf(topic, "SMART-%s", get_my_id());
	INFO("\r\nmqtt topic:%s ...\r\n", topic);
	
	//MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);
	MQTT_InitConnection(&mqttClient, sysCfg.cloud_server_ip, sysCfg.cloud_server_port, sysCfg.security);

	//MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);
	MQTT_InitClient(&mqttClient, topic, "", "", 120, 1);

	MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);	
	
}

void wifiConnectCb(uint8_t status)
{
	if(status == STATION_GOT_IP){
		init_mqtt();
		MQTT_Connect(&mqttClient);
	} else {
		MQTT_Disconnect(&mqttClient);
	}
}

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
 *******************************************************************************/
uint32 ICACHE_FLASH_ATTR
user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;
    
    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;
            
        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;
            
        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;
            
        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;
            
        default:
            rf_cal_sec = 0;
            break;
    }
    
    return rf_cal_sec;
}

static ETSTimer WiFiLinker;
static uint8_t wifiStatus = STATION_IDLE, lastWifiStatus = STATION_IDLE;

static void ICACHE_FLASH_ATTR wifi_check_ip(void *arg)
{
	struct ip_info ipConfig;

	os_timer_disarm(&WiFiLinker);
	wifi_get_ip_info(STATION_IF, &ipConfig);
	wifiStatus = wifi_station_get_connect_status();
	if (wifiStatus == STATION_GOT_IP && ipConfig.ip.addr != 0)
	{

	ota("192.168.0.101", 8080);
	//ota("45.32.55.222", 9090);




	}
	else
	{
		if(wifi_station_get_connect_status() == STATION_WRONG_PASSWORD)
		{

			INFO("STATION_WRONG_PASSWORD\r\n");
			wifi_station_connect();


		}
		else if(wifi_station_get_connect_status() == STATION_NO_AP_FOUND)
		{

			INFO("STATION_NO_AP_FOUND\r\n");
			wifi_station_connect();


		}
		else if(wifi_station_get_connect_status() == STATION_CONNECT_FAIL)
		{

			INFO("STATION_CONNECT_FAIL\r\n");
			wifi_station_connect();

		}
		else
		{
			INFO("STATION_IDLE\r\n");
		}

		os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
		os_timer_arm(&WiFiLinker, 500, 0);
	}

}


void user_init(void)
{
	// 115200 速度太快，接arduino常會有資料錯誤, 所以使用內定的74880即可
	//uart_init(BIT_RATE_115200, BIT_RATE_115200);
	delay_ms(1000);

	
	INFO("\r\nSystem started ...\r\n");
	CFG_Load();
	
#if defined(ESP01)	
	mcu_command("led off");
	#else	
	as_gpio(LED_PIN);
	set_gpio(LED_PIN, 0);
#endif
		
	
	if(sysCfg.smart_config) { // 配對模式
		sysCfg.smart_config = 0;
		CFG_Save();
		smart_config();
		
	} else {
	
#if defined(ESP01)	
		
#else
		as_gpio(SW1_PIN);
		set_gpio(SW1_PIN, 1);
		as_gpio(SW2_PIN);
		set_gpio(SW2_PIN, 1);
		as_gpio(SW3_PIN);
		set_gpio(SW3_PIN, 1);
		as_gpio(SW4_PIN);
		set_gpio(SW4_PIN, 1);
#endif

		//sysCfg.cloud_server_port = 1883;

		button_init();
		
		INFO("hardware version:%s\n", sysCfg.hardware_ver);
		INFO("firmware version:%s\n", sysCfg.firmware_ver);

		
		INFO("cloud server ip:%s:%d, security:%d\n", sysCfg.cloud_server_ip, sysCfg.cloud_server_port, sysCfg.security);
		INFO("manager id:%s\n", sysCfg.manager_id);
		


	
		// 同時有指定雲端主機位址, 跟manager_id(配對)
		//sysCfg.sta_ssid[0] && 
		if(sysCfg.manager_id[0] && sysCfg.cloud_server_ip[0] && sysCfg.cloud_server_port != 0) {
			
			INFO("cloud server ip:%s:%d\n", sysCfg.cloud_server_ip, sysCfg.cloud_server_port);
			WIFI_Connect(NULL,NULL, wifiConnectCb);		
			
			
		} else {
			// 等待使用者配對
			INFO("system ready ...\n");
		}
		
	}
	

}

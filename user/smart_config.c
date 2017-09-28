#include "ets_sys.h"
#include "osapi.h"
#include "ip_addr.h"
#include "espconn.h"
#include "mem.h"
#include "config.h"
#include "debug.h"

#include "user_interface.h"
#include "smartconfig.h"
#include "user_io.h"
#include "user_stdlib.h"




#define TARGET_PORT 	19760
#define LISTEN_PORT		19760	

#define ESP_TOUCH_TIME_ENTER 30000
#define ESP_TOUCH_TIMEOUT_MS 30000
#define ESPTOUCH_CONNECT_TIMEOUT_MS 20000


LOCAL esp_udp ssdp_udp;
LOCAL struct espconn pssdpudpconn;
LOCAL os_timer_t ssdp_time_serv;
LOCAL os_timer_t esptouch_tout_t;
LOCAL os_timer_t restart_timer;
LOCAL os_timer_t update_timer;


unsigned char g_mac_address[6];
uint8_t  lan_buf[200];
uint16_t lan_buf_len;
uint8 	 udp_sent_cnt = 0;
int 	 handshake = 0;
int ota_flag;

char ota_server[40];
int ota_port;
	
	

static void ICACHE_FLASH_ATTR
restart_cb(void* data)
{
	system_restart();

}


void ICACHE_FLASH_ATTR delay_restart(int delay_time_ms)
{
    os_timer_disarm(&restart_timer);
    os_timer_setfn(&restart_timer, restart_cb, NULL);
    os_timer_arm(&restart_timer, delay_time_ms, 0);
}	
	

	

void ICACHE_FLASH_ATTR get_mac_address(char *my_id)
{

	while(wifi_get_macaddr(0x00, g_mac_address) == false) { // 0 station, 1 soft-ap
		os_delay_us(30000); // 30ms
	};	
	
		
	// 隨便使用一組數字將mac address編碼，避免將mac address直接暴露
	g_mac_address[0] ^= 0x1c;
	g_mac_address[1] ^= 0xfa;
	g_mac_address[2] ^= 0xbb;
	g_mac_address[3] ^= 0x75;
	g_mac_address[4] ^= 0x26;
	g_mac_address[5] ^= 0x63;
	
	INFO("wifi_get_macaddr %02x%02x%02x%02x%02x%02x---\r\n",
		g_mac_address[0],
		g_mac_address[1],
		g_mac_address[2],
		g_mac_address[3],
		g_mac_address[4],
		g_mac_address[5]
	);	
	
	if(my_id != NULL) {
		os_sprintf(my_id, "%02x%02x%02x%02x%02x%02x",
			g_mac_address[0],
			g_mac_address[1],
			g_mac_address[2],
			g_mac_address[3],
			g_mac_address[4],
			g_mac_address[5]
		);		
	}
}




LOCAL void ICACHE_FLASH_ATTR
wifilan_time_callback(void)
{

	int ret;

	
	if ((udp_sent_cnt++) >30) {
		udp_sent_cnt = 0;
		
#if defined(ESP01)
	mcu_command("led off");
#else	  
	set_gpio(LED_PIN, 0);
#endif
		os_timer_disarm(&ssdp_time_serv);//s
		
		delay_restart(500);
		return;
	}

	ssdp_udp.remote_port = TARGET_PORT;
	ssdp_udp.remote_ip[0] = 255;
	ssdp_udp.remote_ip[1] = 255;
	ssdp_udp.remote_ip[2] = 255;
	ssdp_udp.remote_ip[3] = 255;
	
	
	
	if(handshake == 0) { // 一開始先等待管理者送來管理者帳號
		
		
	} else if(handshake == 1) { // 送出已經收到管理者帳號
		strcpy(lan_buf, "**#smart_device#** id_ok");
		os_printf("send:%s\n", lan_buf);
		ret = espconn_sendto(&pssdpudpconn, lan_buf, strlen(lan_buf));
	
		if (ret != 0) {
			INFO("UDP send error!");
		}
	
	} else if(handshake == 2) { // 發送ID給管理者
		os_sprintf(lan_buf, "**#smart_device#** device_id:%02x%02x%02x%02x%02x%02x product_id:%08x",
		g_mac_address[0],
		g_mac_address[1],
		g_mac_address[2],
		g_mac_address[3],
		g_mac_address[4],
		g_mac_address[5],
		sysCfg.product_id
		);
		INFO("send:%s\n", lan_buf);
		ret = espconn_sendto(&pssdpudpconn, lan_buf, strlen(lan_buf));
	
		if (ret != 0) {
			INFO("UDP send error!");
		}
	
	}

	INFO("Finish send notify!\n");
}

LOCAL void ICACHE_FLASH_ATTR
server_parsing(char *ptr, char *ip, int *port)
{
	sscanf(ptr, "%s", ip);
	ptr = (char *)strrchr(ip,':');
	if(ptr) {
		*port = atoi(ptr+1);
		*ptr = 0;
	} else {
		*port = 0;
	}
}


void ICACHE_FLASH_ATTR
ota(const char *server_ip, int port);

static void ICACHE_FLASH_ATTR
update_cb(void* data)
{
    os_timer_disarm(&update_timer);
	ota(ota_server, ota_port);

}


LOCAL void ICACHE_FLASH_ATTR
wifilan_recv_callbk(void *arg, char *pdata, unsigned short len)
{
	char *ptr;
	char fwid[40];
	pdata[len-1] = 0;
    os_printf("wifilan_recv_callbk: %s\r\n", pdata);
	
	if(strncmp(pdata, "**#smart_manager#** fwid:", 25) == 0) { // 要求更新韌體
	
		if(ota_flag == 0) {
			ota_flag = 1;
			
			espconn_delete(&pssdpudpconn);
			os_timer_disarm(&ssdp_time_serv);
			
			sscanf(pdata+25, "%s", fwid);
			// ota server ip:port
			ptr = (char *)os_strstr(pdata+31, "server://");
			if(strcmp(FWID, fwid) == 0 &&  ptr) { 
				server_parsing(ptr+9, ota_server, &ota_port);
				
				os_timer_disarm(&update_timer);
				os_timer_setfn(&update_timer, update_cb, NULL);
				os_timer_arm(&update_timer, 200, 0);	
			} else {
				delay_restart(500);
			}
		}
	
	
	} else if(strncmp(pdata, "**#smart_manager#** manager_id:", 31) == 0) { // 管理者送來管理者帳號
		
		if(handshake == 0) {
			sscanf(pdata+31, "%s", sysCfg.manager_id);
			
			handshake = 1; // 開始通知說已收到管理者帳號
			
			// 檢查是否設定雲端位址 ip:port
			ptr = (char *)os_strstr(pdata+31, "server://");
			if(ptr) { 
				server_parsing(ptr+9, sysCfg.cloud_server_ip, &sysCfg.cloud_server_port);
				sysCfg.security = 0;
				
			} else {
				// 檢查是否設定雲端位址 ip:port 需要加密
				ptr = (char *)os_strstr(pdata+31, "server_s://");
				if(ptr) { 
					server_parsing(ptr+11, sysCfg.cloud_server_ip, &sysCfg.cloud_server_port);
					sysCfg.security = 1;
				} else {
					os_strcpy(sysCfg.cloud_server_ip, "");
					sysCfg.cloud_server_port = 0;
					sysCfg.security = 0;
				}
			}
				
		}
			
		os_printf("handshake = 1 \r\n");
	} else if(strncmp(pdata, "**#smart_manager#** device_id", 29) == 0) { // 管理者要求裝置的ID
		if(handshake == 1)
			handshake = 2; // 開始發送裝置的ID
		os_printf("handshake = 2 \r\n");
	} else if(strncmp(pdata, "**#smart_manager#** over", 24) == 0) { // 管理者結束交握流程
		if(handshake == 2) {
			os_printf("handshake = 3 \r\n");
			udp_sent_cnt = 100;
			CFG_Save();
			handshake = 3;
		}
		
	}
}


void ICACHE_FLASH_ATTR
start_discover(void)
{
    handshake = 0;
	udp_sent_cnt = 0;
	ota_flag = 0;
	
	ssdp_udp.local_port = LISTEN_PORT;
	pssdpudpconn.type = ESPCONN_UDP;
	pssdpudpconn.proto.udp = &(ssdp_udp);
	espconn_regist_recvcb(&pssdpudpconn, wifilan_recv_callbk);
	espconn_create(&pssdpudpconn);
	
	get_mac_address(NULL);

	os_timer_disarm(&ssdp_time_serv);
	os_timer_setfn(&ssdp_time_serv, (os_timer_func_t *)wifilan_time_callback, NULL);
	os_timer_arm(&ssdp_time_serv, 1000, 1);//1s
}




static void ICACHE_FLASH_ATTR
esptouch_fail_cb(void* data)
{
    wifi_station_disconnect();
    smartconfig_stop();
    os_printf("ESP-TOUCH TIMEOUT\r\n");
    os_timer_disarm(&esptouch_tout_t);
	
#if defined(ESP01)
	mcu_command("led off");
#else	  
	set_gpio(LED_PIN, 0);
#endif
	delay_restart(500);
	
}

void ICACHE_FLASH_ATTR
smartconfig_done(sc_status status, void *pdata)
{
    switch(status) {
        case SC_STATUS_WAIT:
            os_printf("SC_STATUS_WAIT\n");
            break;
        case SC_STATUS_FIND_CHANNEL:
            os_printf("SC_STATUS_FIND_CHANNEL\n");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            os_printf("SC_STATUS_GETTING_SSID_PSWD\n");
			sc_type *type = pdata;
            if (*type == SC_TYPE_ESPTOUCH) {
                os_printf("SC_TYPE:SC_TYPE_ESPTOUCH\n");
            } else {
                os_printf("SC_TYPE:SC_TYPE_AIRKISS\n");
            }
			
            os_timer_disarm(&esptouch_tout_t);
            os_timer_setfn(&esptouch_tout_t, esptouch_fail_cb, NULL);
            os_timer_arm(&esptouch_tout_t, ESP_TOUCH_TIMEOUT_MS, 0);
			
            break;
        case SC_STATUS_LINK:
            os_printf("SC_STATUS_LINK\n");
            struct station_config *sta_conf = pdata;
	
	        wifi_station_set_config(sta_conf);
	        wifi_station_disconnect();
	        wifi_station_connect();
			
			os_timer_disarm(&esptouch_tout_t);
			os_timer_arm(&esptouch_tout_t, ESPTOUCH_CONNECT_TIMEOUT_MS, 0);
            break;
        case SC_STATUS_LINK_OVER:
            os_printf("SC_STATUS_LINK_OVER\n");
			os_timer_disarm(&esptouch_tout_t);
			
			
            if (pdata != NULL) {
				//SC_TYPE_ESPTOUCH
                uint8 phone_ip[4] = {0};

                os_memcpy(phone_ip, (uint8*)pdata, 4);
                os_printf("Phone ip: %d.%d.%d.%d\n",phone_ip[0],phone_ip[1],phone_ip[2],phone_ip[3]);
				
				start_discover();
            } else {
            	//SC_TYPE_AIRKISS - support airkiss v2.0
				//airkiss_start_discover();
            }
            smartconfig_stop();
            break;
    }
	
}




	




void smart_config()
{
#if defined(ESP01)
	mcu_command("led on");
#else	  
	set_gpio(LED_PIN, 1);
#endif

	
	smartconfig_set_type(SC_TYPE_ESPTOUCH); //SC_TYPE_ESPTOUCH,SC_TYPE_AIRKISS,SC_TYPE_ESPTOUCH_AIRKISS
	esptouch_set_timeout(5);
    wifi_set_opmode(STATION_MODE);

	
    os_timer_disarm(&esptouch_tout_t);
    os_timer_setfn(&esptouch_tout_t, esptouch_fail_cb, NULL);
    os_timer_arm(&esptouch_tout_t, ESP_TOUCH_TIME_ENTER, 0);

	
    smartconfig_start(smartconfig_done);
	
	//os_printf("smart_config ..... exit\n");
	
}


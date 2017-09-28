/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2016 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"
#include "upgrade.h"
#include "config.h"
#include "user_io.h"



#define UPGRADE_FRAME  "{\"path\": \"/v1/messages/\", \"method\": \"POST\", \"meta\": {\"Authorization\": \"token %s\"},\
\"get\":{\"action\":\"%s\"},\"body\":{\"pre_rom_version\":\"%s\",\"rom_version\":\"%s\"}}\n"

#define pheadbuffer "Connection: keep-alive\r\n\
Cache-Control: no-cache\r\n\
User-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/30.0.1599.101 Safari/537.36 \r\n\
Accept: */*\r\n\
Accept-Encoding: gzip,deflate\r\n\
Accept-Language: zh-CN,eb-US;q=0.8\r\n\r\n"

/**/


static struct espconn *pespconn = NULL;


static os_timer_t at_delay_check;
static struct espconn *pTcpServer = NULL;
static ip_addr_t host_ip;
int upgrade_flag;

LOCAL void ICACHE_FLASH_ATTR 
at_response_error()
{
	os_printf("device_upgrade_fail\r\n");
}

/**
  * @brief  Tcp client disconnect success callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
static void ICACHE_FLASH_ATTR
on_discon_cb(void *arg)
{
  if(upgrade_flag == true)
  {
    os_printf("device_upgrade_success\r\n");
    system_upgrade_reboot();
  }
  else
  {
    os_printf("device_upgrade_failed\r\n");
    system_restart();
  }
}

/**
  * @brief  Udp server receive data callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
LOCAL void ICACHE_FLASH_ATTR
on_recv(void *arg, char *pusrdata, unsigned short len)
{
  os_timer_disarm(&at_delay_check);
}

LOCAL void ICACHE_FLASH_ATTR
on_wait(void *arg)
{
  struct espconn *pespconn = arg;
  os_timer_disarm(&at_delay_check);
  if(pespconn != NULL)
  {
    espconn_disconnect(pespconn);
  }
  else
  {
    at_response_error();
  }
}

/******************************************************************************
 * FunctionName : user_esp_platform_sent_cb
 * Description  : Data has been sent successfully and acknowledged by the remote host.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
on_sent_cb(void *arg)
{
  struct espconn *pespconn = arg;
  os_timer_disarm(&at_delay_check);
  os_timer_setfn(&at_delay_check, (os_timer_func_t *)on_wait, pespconn);
  os_timer_arm(&at_delay_check, 10000, 0);
  os_printf("on_sent_cb\r\n");
}

/**
  * @brief  Tcp client connect success callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
static void ICACHE_FLASH_ATTR
on_connect_cb(void *arg)
{
  struct espconn *pespconn = (struct espconn *)arg;
  uint8_t user_bin[9] = {0};
  char *temp = NULL;

  at_port_print("+CIPUPDATE:2\r\n");


  espconn_regist_disconcb(pespconn, on_discon_cb);
  espconn_regist_recvcb(pespconn, on_recv);////////
  espconn_regist_sentcb(pespconn, on_sent_cb);

  temp = (uint8 *) os_zalloc(512);

  os_sprintf(temp,"GET /finish?result=%s HTTP/1.0\r\nHost: "IPSTR"\r\n"pheadbuffer"",upgrade_flag ? "success" : "fail",
             IP2STR(pespconn->proto.tcp->remote_ip));

  espconn_sent(pespconn, temp, os_strlen(temp));
  os_free(temp);
}

/**
  * @brief  Tcp client connect repeat callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
static void ICACHE_FLASH_ATTR
on_recon_cb(void *arg, sint8 errType)
{
  struct espconn *pespconn = (struct espconn *)arg;

    at_response_error();
    if(pespconn->proto.tcp != NULL)
    {
      os_free(pespconn->proto.tcp);
    }
    os_free(pespconn);
    os_printf("disconnect\r\n");

    at_response_error();
	system_restart();

}


void ICACHE_FLASH_ATTR
finish(const char *server_ip, int port, int flag)
{
  upgrade_flag = flag;
  os_printf("finish\r\n");
  pespconn = (struct espconn *)os_zalloc(sizeof(struct espconn));
  pespconn->type = ESPCONN_TCP;
  pespconn->state = ESPCONN_NONE;
  pespconn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
  pespconn->proto.tcp->local_port = espconn_port();
  pespconn->proto.tcp->remote_port = port;

  host_ip.addr = ipaddr_addr(server_ip);
  os_memcpy(pespconn->proto.tcp->remote_ip, &host_ip.addr, 4);
  espconn_regist_connectcb(pespconn, on_connect_cb);
  espconn_regist_reconcb(pespconn, on_recon_cb);
  espconn_connect(pespconn);
}


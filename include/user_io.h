#ifndef __USER_IO_H__
#define __USER_IO_H__

void as_gpio(int pinNum);
void set_gpio(int num, int v);
int get_gpio(int num);
uint32 get_gpio_function(int num);
uint32 get_gpio_name(int num);
void mcu_command(const char *cmd);

#define ESP01


#if defined(ESP01)

#define BTN_PIN	2
#define FLASH_SIZE	PRODUCT_FLASH_1M

#define FWID	"AD6B1AF5-AB08-4C01-9204-7274C37D6890"


#else

#define SW1_PIN	4 
#define SW2_PIN	5 
#define SW3_PIN	12
#define SW4_PIN	13


#define BTN_PIN	14
#define LED_PIN	15
#define FLASH_SIZE	PRODUCT_FLASH_4M

#define FWID	"37F53C19-3A1C-4308-8560-11C160C1AC65"

#endif




#endif


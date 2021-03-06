
/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote the original version of this file. 
 * As long as you retain this notice you can do whatever you want with this stuff.
 * If we meet some day, and you think this stuff is worth it, you can buy me a beer
 * in return. 
 * ----------------------------------------------------------------------------
 */

#include "ets_sys.h"
#include "osapi.h"
#include "espmissingincludes.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "gpio.h"
#include "cgi.h"
#include "driver/gpio16.h"
#include "driver/i2c_oled.h"

#define GPIO_OUTPUT1 13
#define GPIO_OUTPUT2 12
#define GPIO_LED1 16
#define GPIO_BUTTON1 2
#define GPIO_BUTTON2 0

static ETSTimer eventTimer;


void ICACHE_FLASH_ATTR ioOutput(int ena, int pin) {
        os_printf("IO_OUTPUT: '%d' to '%d'\n", pin, ena);
	if (ena) {
		gpio_output_set((1<<pin), 0, (1<<pin), 0);
	} else {
		gpio_output_set(0, (1<<pin), (1<<pin), 0);
	}
}

void ICACHE_FLASH_ATTR ioSetLed(int ena) {
	if (ena) {
		gpio16_output_set(1);
	} else {
		gpio16_output_set(0);
	}
}

void restoreFactorySettings() {
	wifi_station_disconnect();
	wifi_set_opmode(0x3); //reset to STA+AP mode

	struct softap_config apConfig;
	wifi_softap_get_config(&apConfig);
	os_strncpy((char*)apConfig.ssid, "smartswitch", 18);
	apConfig.authmode = 0; //Disable security
	wifi_softap_set_config(&apConfig);

	struct station_config stconf;
	os_strncpy((char*)stconf.ssid, "", 2);
	os_strncpy((char*)stconf.password, "", 2);
	wifi_station_set_config(&stconf);

	OLED_CLS();
	OLED_Print(2, 0, "RESET", 1);
	os_printf("Reset completed. Restarting system...\n");

	ioOutput(0, GPIO_OUTPUT1);
	ioOutput(0, GPIO_OUTPUT2);

	while (!GPIO_INPUT_GET(GPIO_BUTTON1)) { os_printf("."); };
	while (!GPIO_INPUT_GET(GPIO_BUTTON2)) { os_printf("."); };

	system_restart();
}

static void ICACHE_FLASH_ATTR eventTimerCb(void *arg) {
	static int apledCnt = 0;
        static bool apledState = 0;
	static int resetCnt=0;
	static bool button1 = 1;
	static bool button2 = 1;
	apledCnt ++;
	if (apledCnt > 2) {
		apledState = ~apledState;
		apledCnt = 0;
	}
	if ((!GPIO_INPUT_GET(GPIO_BUTTON1))&&(!GPIO_INPUT_GET(GPIO_BUTTON2))) {
		resetCnt++;
		os_printf("Both buttons pressed (reset) (%d/30)\n", resetCnt);
		if (apledState) {
			ioSetLed(1);
		} else {
			ioSetLed(0);
		}
		if (resetCnt>=30) { //3 sec pressed
			restoreFactorySettings();
		}
	} else {
		resetCnt=0;
		//Button 1
		if (!GPIO_INPUT_GET(GPIO_BUTTON1)) {
			if (!button1) {
				os_printf("Button 1 pressed!\n");
				toggleOutput1();
			}
			button1 = 1;
		} else {
			button1 = 0;
		}
		//Button 2
		if (!GPIO_INPUT_GET(GPIO_BUTTON2)) {
			if (!button2) {
				os_printf("Button 2 pressed!\n");
				toggleOutput2();
			}
			button2 = 1;
		} else {
			button2 = 0;
		}
		//Leds
		if (wifi_get_opmode()==1) { //Client mode
			if (wifi_station_get_connect_status()==STATION_GOT_IP) {
				ioSetLed(1);
			} else {
				if (apledState) {
					ioSetLed(1);
				} else {
					ioSetLed(0);
				}
			}
		}
		if ((wifi_get_opmode()==2)) { //AP mode
			ioSetLed(1);
		}
		if (wifi_get_opmode()==3) { //Client + AP mode
			if (wifi_station_get_connect_status()==STATION_GOT_IP) {
				ioSetLed(1);
			} else {
				if (apledState) {
					ioSetLed(1);
				} else {
					ioSetLed(0);
				}
			}
		}
	}
}

void ioInit() {
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);	//Button 1
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);	//Button 2
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);	//Output 2
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);	//Output 1
	gpio16_output_conf();					//Led
        
	gpio_output_set(0, 0, (1<<GPIO_OUTPUT1)+(1<<GPIO_OUTPUT2), (1<<GPIO_BUTTON2)+(1<<GPIO_BUTTON1));
	os_timer_disarm(&eventTimer);
	os_timer_setfn(&eventTimer, eventTimerCb, NULL);
	os_timer_arm(&eventTimer, 100, 1);
}

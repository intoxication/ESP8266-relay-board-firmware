/*
Some random cgi routines.
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#include <string.h>
#include <osapi.h>
#include "user_interface.h"
#include "mem.h"
#include "httpd.h"
#include "cgi.h"
#include "io.h"
#include "dht22.h"
#include "espmissingincludes.h"
#include "driver/i2c_oled.h"

static char currOutput1State=0;
static char currOutput2State=0;

#define GPIO_OUTPUT1 12
#define GPIO_OUTPUT2 13

int ICACHE_FLASH_ATTR cgiOLED(HttpdConnData *connData) {
	int len;
	char buff[1024];
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	len=httpdFindArg(connData->getArgs, "clear", buff, sizeof(buff));
	if (len>0) {
		os_printf("Debug: cgiOLED -> OLED_CLS();\n");
		OLED_CLS();
	}

	len=httpdFindArg(connData->getArgs, "power", buff, sizeof(buff));
	if (len>0) {
		char state=atoi(buff)&0x01;
		if (state) {
			os_printf("Debug: cgiOLED -> OLED_ON();\n");
			OLED_ON();
		} else {
			os_printf("Debug: cgiOLED -> OLED_OFF();\n");
			OLED_OFF();
		}
	}

	int textsize = 1;
	int textx = 0;
	int texty = 0;

	len=httpdFindArg(connData->getArgs, "x", buff, sizeof(buff));
	if (len>0) {
		textx = atoi(buff);
	}

	len=httpdFindArg(connData->getArgs, "y", buff, sizeof(buff));
	if (len>0) {
		texty = atoi(buff);
	}

	len=httpdFindArg(connData->getArgs, "size", buff, sizeof(buff));
	if (len>0) {
		textsize = atoi(buff);
	}

	len=httpdFindArg(connData->getArgs, "text", buff, sizeof(buff));
	if (len>0) {
		os_printf("Debug: Printing \"%s\" to oled with params [%d,%d,%d].\n",buff,textx,texty,textsize);
		OLED_Print(textx, texty, buff, textsize);
	}

	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Content-Type", "text/html");
	httpdEndHeaders(connData);

	os_strcpy(buff, "OLED form submitted.<br /><a href='home'>Go back</a>");
	espconn_sent(connData->conn, (uint8 *)buff, os_strlen(buff));

	//httpdRedirect(connData, "home?oled=done");
	
	return HTTPD_CGI_DONE;
}

//Cgi that turns output 1 on or off according to the 'state' param in the GET data
int ICACHE_FLASH_ATTR cgiSet1(HttpdConnData *connData) {
	int len;
	char buff[1024];
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	len=httpdFindArg(connData->getArgs, "state", buff, sizeof(buff));
	if (len>0) {
		currOutput1State=atoi(buff)&0x01;
		ioOutput(currOutput1State, GPIO_OUTPUT1);
	}

	if (currOutput1State) {
	  os_strcpy(buff, "1");
	} else {
	  os_strcpy(buff, "0");
	}

	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Content-Type", "text/html");
	httpdEndHeaders(connData);

	espconn_sent(connData->conn, (uint8 *)buff, os_strlen(buff));

	//httpdRedirect(connData, "");
	
	return HTTPD_CGI_DONE;
}

//Cgi that turns output 2 on or off according to the 'state' param in the GET data
int ICACHE_FLASH_ATTR cgiSet2(HttpdConnData *connData) {
	int len;
	char buff[1024];
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	len=httpdFindArg(connData->getArgs, "state", buff, sizeof(buff));
	if (len>0) {
		currOutput2State=atoi(buff);
		ioOutput(currOutput2State, GPIO_OUTPUT2);
	}

	if (currOutput2State) {
	  os_strcpy(buff, "1");
	} else {
	  os_strcpy(buff, "0");
	}

	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Content-Type", "text/html");
	httpdEndHeaders(connData);

	espconn_sent(connData->conn, (uint8 *)buff, os_strlen(buff));

	//httpdRedirect(connData, "");
	
	return HTTPD_CGI_DONE;
}



//Template code for the get page
void ICACHE_FLASH_ATTR tplGet(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	if (token==NULL) return;

	os_strcpy(buff, "-1");
	if (os_strcmp(token, "output1state")==0) {
		if (currOutput1State) {
			os_strcpy(buff, "1");
		} else {
			os_strcpy(buff, "0");
		}
	}
	if (os_strcmp(token, "output2state")==0) {
		if (currOutput2State) {
			os_strcpy(buff, "1");
		} else {
			os_strcpy(buff, "0");
		}
	}
	espconn_sent(connData->conn, (uint8 *)buff, os_strlen(buff));
}

static long hitCounter=0;

//Template code for the counter on the index page.
void ICACHE_FLASH_ATTR tplHome(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	if (token==NULL) return;

	os_strcpy(buff, "Unknown");

	if (os_strcmp(token, "counter")==0) {
		hitCounter++;
		os_sprintf(buff, "%ld", hitCounter);
	}

	if (os_strcmp(token, "output1state")==0) {
		if (currOutput1State) {
			os_strcpy(buff, "On");
		} else {
			os_strcpy(buff, "Off");
		}
	}
	if (os_strcmp(token, "output2state")==0) {
		if (currOutput2State) {
			os_strcpy(buff, "On");
		} else {
			os_strcpy(buff, "Off");
		}
	}
	if (os_strcmp(token, "output1check")==0) {
		if (currOutput1State) {
			os_strcpy(buff, "checked");
		} else {
			os_strcpy(buff, "");
		}
	}
	if (os_strcmp(token, "output2check")==0) {
		if (currOutput2State) {
			os_strcpy(buff, "checked");
		} else {
			os_strcpy(buff, "");
		}
	}

	//Ugly fix for printing a percent sign
	if (os_strcmp(token, "p")==0) {
		os_sprintf(buff, "%%");		
	}
	/* --- */
	espconn_sent(connData->conn, (uint8 *)buff, os_strlen(buff));
}


//Cgi that reads the SPI flash. Assumes 512KByte flash.
int ICACHE_FLASH_ATTR cgiReadFlash(HttpdConnData *connData) {
	int *pos=(int *)&connData->cgiData;
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	if (*pos==0) {
		os_printf("Start flash download.\n");
		httpdStartResponse(connData, 200);
		httpdHeader(connData, "Content-Type", "application/bin");
		httpdEndHeaders(connData);
		*pos=0x40200000;
		return HTTPD_CGI_MORE;
	}
	espconn_sent(connData->conn, (uint8 *)(*pos), 1024);
	*pos+=1024;
	if (*pos>=0x40200000+(512*1024)) return HTTPD_CGI_DONE; else return HTTPD_CGI_MORE;
}

//Template code for the DHT 22 page.
void ICACHE_FLASH_ATTR tplDHT(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	if (token==NULL) return;

	float * r = readDHT();
	float lastTemp=r[0];
	float lastHum=r[1];
	
	os_strcpy(buff, "Unknown");
	if (os_strcmp(token, "temperature")==0) {
			os_sprintf(buff, "%d.%d", (int)(lastTemp),(int)((lastTemp - (int)lastTemp)*100) );		
	}
	if (os_strcmp(token, "humidity")==0) {
			os_sprintf(buff, "%d.%d", (int)(lastHum),(int)((lastHum - (int)lastHum)*100) );		
	}	
	
	espconn_sent(connData->conn, (uint8 *)buff, os_strlen(buff));
}

int getOutput1( void ) {
  return currOutput1State;
}

int getOutput2( void ) {
  return currOutput2State;
}

int toggleOutput1 ( void ) {
  currOutput1State = ~currOutput1State&0x01;
  os_printf("O1_TOGGLE: %d\n", currOutput1State);
  ioOutput(currOutput1State, GPIO_OUTPUT1);
  return currOutput1State;
}

int toggleOutput2 ( void ) {
  currOutput2State = (~currOutput2State)&0x01;
  os_printf("O2_TOGGLE: %d\n", currOutput2State);
  ioOutput(currOutput2State, GPIO_OUTPUT2);
  return currOutput2State;
}

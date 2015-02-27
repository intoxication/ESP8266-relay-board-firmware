/*
Cgi/template routines for the /wifi url.
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
#include "espmissingincludes.h"
#include "gpio.h"

//WiFi access point data
typedef struct {
	char ssid[32];
	char rssi;
	char enc;
} ApData;

//Scan resolt
typedef struct {
	char scanInProgress;
	ApData **apData;
	int noAps;
} ScanResultData;

//Static scan status storage.
ScanResultData cgiWifiAps;

static void ICACHE_FLASH_ATTR rebootTimerCb(void *arg) {
	system_restart();
}

//Callback the code calls when a wlan ap scan is done. Basically stores the result in
//the cgiWifiAps struct.
void ICACHE_FLASH_ATTR wifiScanDoneCb(void *arg, STATUS status) {
	int n;
	struct bss_info *bss_link = (struct bss_info *)arg;
	os_printf("wifiScanDoneCb %d\n", status);
	if (status!=OK) {
		cgiWifiAps.scanInProgress=0;
		wifi_station_disconnect(); //test HACK
		return;
	}

	//Clear prev ap data if needed.
	if (cgiWifiAps.apData!=NULL) {
		for (n=0; n<cgiWifiAps.noAps; n++) os_free(cgiWifiAps.apData[n]);
		os_free(cgiWifiAps.apData);
	}

	//Count amount of access points found.
	n=0;
	while (bss_link != NULL) {
		bss_link = bss_link->next.stqe_next;
		n++;
	}
	//Allocate memory for access point data
	cgiWifiAps.apData=(ApData **)os_malloc(sizeof(ApData *)*n);
	cgiWifiAps.noAps=n;

	//Copy access point data to the static struct
	n=0;
	bss_link = (struct bss_info *)arg;
	while (bss_link != NULL) {
		cgiWifiAps.apData[n]=(ApData *)os_malloc(sizeof(ApData));
		cgiWifiAps.apData[n]->rssi=bss_link->rssi;
		cgiWifiAps.apData[n]->enc=bss_link->authmode;
		strncpy(cgiWifiAps.apData[n]->ssid, (char*)bss_link->ssid, 32);

		bss_link = bss_link->next.stqe_next;
		n++;
	}
	os_printf("Scan done: found %d APs\n", n);
	//We're done.
	cgiWifiAps.scanInProgress=0;
}


//Routine to start a WiFi access point scan.
static void ICACHE_FLASH_ATTR wifiStartScan() {
	int x;
	cgiWifiAps.scanInProgress=1;
	x=wifi_station_get_connect_status();
	if (x!=STATION_GOT_IP) {
		//Unit probably is trying to connect to a bogus AP. This messes up scanning. Stop that.
		os_printf("STA status = %d. Disconnecting STA...\n", x);
		wifi_station_disconnect();
	}
	wifi_station_scan(NULL, wifiScanDoneCb);
}

//This CGI is called from the bit of AJAX-code in wifi.tpl. It will initiate a
//scan for access points and if available will return the result of an earlier scan.
//The result is embedded in a bit of JSON parsed by the javascript in wifi.tpl.
int ICACHE_FLASH_ATTR cgiWiFiScan(HttpdConnData *connData) {
	int len;
	int i;
	char buff[1024];
	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Content-Type", "text/json");
	httpdEndHeaders(connData);

	if (cgiWifiAps.scanInProgress==1) {
		len=os_sprintf(buff, "{\n \"result\": { \n\"inProgress\": \"1\"\n }\n}\n");
		espconn_sent(connData->conn, (uint8 *)buff, len);
	} else {
		len=os_sprintf(buff, "{\n \"result\": { \n\"inProgress\": \"0\", \n\"APs\": [\n");
		espconn_sent(connData->conn, (uint8 *)buff, len);
		if (cgiWifiAps.apData==NULL) cgiWifiAps.noAps=0;
		for (i=0; i<cgiWifiAps.noAps; i++) {
			len=os_sprintf(buff, "{\"essid\": \"%s\", \"rssi\": \"%d\", \"enc\": \"%d\"}%s\n", 
					cgiWifiAps.apData[i]->ssid, cgiWifiAps.apData[i]->rssi, 
					cgiWifiAps.apData[i]->enc, (i==cgiWifiAps.noAps-1)?"":",");
			espconn_sent(connData->conn, (uint8 *)buff, len);
		}
		len=os_sprintf(buff, "]\n}\n}\n");
		espconn_sent(connData->conn, (uint8 *)buff, len);
		wifiStartScan();
	}
	return HTTPD_CGI_DONE;
}

//Temp store for new ap info.
static struct station_config stconf;

/*//This routine is ran some time after a connection attempt to an access point. If
//the connect succeeds, this gets the module in STA-only mode.
 static void ICACHE_FLASH_ATTR resetTimerCb(void *arg) {
	int x=wifi_station_get_connect_status();
	if (x==STATION_GOT_IP) {
		//Go to STA mode. This needs a reset, so do that.
		wifi_set_opmode(1);
		system_restart();
	} else {
		os_printf("Connect fail. Not going into STA-only mode.\n");
	}
} 

//Actually connect to a station. This routine is timed because I had problems
//with immediate connections earlier. It probably was something else that caused it,
//but I can't be arsed to put the code back :P
 static void ICACHE_FLASH_ATTR reassTimerCb(void *arg) {
	//int x;
	static ETSTimer resetTimer;
	wifi_station_disconnect();
	wifi_station_set_config(&stconf);
	wifi_station_connect();
	//x=wifi_get_opmode();
	//if (x!=1) {
		//Schedule disconnect/connect
		os_timer_disarm(&resetTimer);
		os_timer_setfn(&resetTimer, resetTimerCb, NULL);
		os_timer_arm(&resetTimer, 4000, 0);
	//}
} */

static void ICACHE_FLASH_ATTR reboot_setstTimerCb(void *arg) {
	wifi_station_disconnect();
	wifi_set_opmode(0x1);
        wifi_station_set_config(&stconf);
        wifi_station_connect();
	system_restart();
}

//This cgi uses the routines above to connect to a specific access point with the
//given ESSID using the given password.
int ICACHE_FLASH_ATTR cgiWiFiConnect(HttpdConnData *connData) {
	char essid[128];
	char passwd[128];
	static ETSTimer reassTimer;
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}
	
	httpdFindArg(connData->postBuff, "essid", essid, sizeof(essid));
	httpdFindArg(connData->postBuff, "passwd", passwd, sizeof(passwd));

	httpdRedirect(connData, "/wifi/newconnectioninfo");

	os_strncpy((char*)stconf.ssid, essid, 32);
	os_strncpy((char*)stconf.password, passwd, 64);

	os_printf("Installing new settings: %s, %s.\n", essid, passwd);

	/*wifi_station_disconnect();
	wifi_set_opmode(0x1);
        wifi_station_set_config(&stconf);
        wifi_station_connect();*/

	//Schedule disconnect/connect
	os_timer_disarm(&reassTimer);
	os_timer_setfn(&reassTimer, reboot_setstTimerCb, NULL);
	os_timer_arm(&reassTimer, 1000, 1);

	//system_restart();

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiReboot(HttpdConnData *connData) {
	static ETSTimer rebootTimer;
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	httpdRedirect(connData, "/wifi/rebooting");

	os_timer_disarm(&rebootTimer);
	os_timer_setfn(&rebootTimer, rebootTimerCb, NULL);
	os_timer_arm(&rebootTimer, 500, 1);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiWiFiapconfig(HttpdConnData *connData) {
	char essid[128];
	char passwd[128];
	char encmode[128];
	char channel[128];
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}
	
	httpdFindArg(connData->postBuff, "essid", essid, sizeof(essid));
	httpdFindArg(connData->postBuff, "passwd", passwd, sizeof(passwd));
	httpdFindArg(connData->postBuff, "encmode", encmode, sizeof(encmode));
	httpdFindArg(connData->postBuff, "channel", channel, sizeof(channel));

	struct softap_config apconf;
	wifi_softap_get_config(&apconf);
	os_strncpy((char*)apconf.ssid, essid, 32);
	os_strncpy((char*)apconf.password, passwd, 64);
	apconf.authmode = atoi(encmode);
	apconf.channel = atoi(channel);
	if ((apconf.authmode >= 0)&&(apconf.authmode < 5)&&(apconf.channel >= 1)&&(apconf.channel < 14)) {
		os_printf("Installing new AP settings: %s, %s, %s, %s.\n", essid, passwd, encmode, channel);
        	wifi_softap_set_config(&apconf);
		httpdRedirect(connData, "/wifi/saved");
	} else {
		os_printf("Could not set new AP settings: %s, %s, %s, %s.\n", essid, passwd, encmode, channel);
		httpdRedirect(connData, "/wifi/error");
	}
	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiWiFimodeconfig(HttpdConnData *connData) {
	char mode[128];
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}
	
	httpdFindArg(connData->postBuff, "sysmode", mode, sizeof(mode));

	int sysmode = atoi(mode);

	if ((sysmode > 0)&&(sysmode < 4)) {
		os_printf("Installing new mode settings: %s.\n", mode);
        	wifi_set_opmode(sysmode);
		httpdRedirect(connData, "/wifi/saved");
	} else {
		os_printf("Could not set new mode settings: %s.\n", mode);
		httpdRedirect(connData, "/wifi/error");
	}
	return HTTPD_CGI_DONE;
}

//Template code for the WLAN page.
void ICACHE_FLASH_ATTR tplWlan(HttpdConnData *connData, char *token, void **arg) {
	char buff[1024];
	int x;
	static struct station_config stconf;
	if (token==NULL) return;
	wifi_station_get_config(&stconf);
	struct softap_config apconf;
	wifi_softap_get_config(&apconf);
	os_strcpy(buff, "Unknown");
	if (os_strcmp(token, "WiFiMode")==0) {
		x=wifi_get_opmode();
		if (x==1) os_strcpy(buff, "Client");
		if (x==2) os_strcpy(buff, "SoftAP");
		if (x==3) os_strcpy(buff, "STA+AP");
	} else if (os_strcmp(token, "currSsid")==0) {
		os_strcpy(buff, (char*)stconf.ssid);
	} else if (os_strcmp(token, "WiFiPasswd")==0) {
		os_strcpy(buff, (char*)stconf.password);
	} else if (os_strcmp(token, "currAPSsid")==0) {
		os_strcpy(buff, (char*)apconf.ssid);
	} else if (os_strcmp(token, "WiFiAPPasswd")==0) {
		os_strcpy(buff, (char*)apconf.password);
	} else if (os_strcmp(token, "WiFiAPsec")==0) {
		os_strcpy(buff, "Unknown");
		if (apconf.authmode==0) {
			os_strcpy(buff, "Open");
		} else if (apconf.authmode==1) {
			os_strcpy(buff, "WEP");
		} else if (apconf.authmode==2) {
			os_strcpy(buff, "WPA PSK");
		} else if (apconf.authmode==3) {
			os_strcpy(buff, "WPA2 PSK");
		} else if (apconf.authmode==4) {
			os_strcpy(buff, "WPA & WPA2 PSK");
		}
	} else if (os_strcmp(token, "WiFiAPchannel")==0) {
		os_sprintf(buff, "%d", apconf.channel);
	} else if (os_strcmp(token, "ipaddress")==0) {
		struct ip_info pTempIp;
		if (wifi_get_opmode()==3) {
			wifi_get_ip_info(0x01, &pTempIp);
			os_sprintf(buff, "%d.%d.%d.%d\r\n",IP2STR(&pTempIp.ip));
		} else {
			os_strcpy(buff, "Unknown"); //TO-DO: find client IP address
		}
	}


	espconn_sent(connData->conn, (uint8 *)buff, os_strlen(buff));
}



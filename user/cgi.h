#ifndef CGI_H
#define CGI_H

#include "httpd.h"

void tplHome(HttpdConnData *connData, char *token, void **arg);
void tplGet(HttpdConnData *connData, char *token, void **arg);
void tplDHT(HttpdConnData *connData, char *token, void **arg);
int cgiOLED(HttpdConnData *connData);
int cgiSet1(HttpdConnData *connData);
int cgiSet2(HttpdConnData *connData);
int cgiReadFlash(HttpdConnData *connData);

int getOutput1( void );
int getOutput2( void );
int toggleOutput1( void );
int toggleOutput2( void );
#endif

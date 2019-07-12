#ifndef _SELFSERVICEWEBSERVER_H
#define _SELFSERVICEWEBSERVER_H

#include <HardwareSerial.h>

void SelfServiceWebServerSetup(HardwareSerial* debugger_in);
void SelfServiceWebServerLoop();
void SelfServiceWebServerActivePower(int32_t timestamp, int32_t p);

#endif//_SELFSERVICEWEBSERVER_H

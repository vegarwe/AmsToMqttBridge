#ifndef _OTAWEBSERVER_H
#define _OTAWEBSERVER_H

#include <HardwareSerial.h>

void OtaWebServerSetup(HardwareSerial* debugger_in);
void OtaWebServerLoop();
void OtaWebServerActivePower(int32_t timestamp, int32_t p);

#endif//_OTAWEBSERVER_H
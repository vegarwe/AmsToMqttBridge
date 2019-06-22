#ifndef _OTAWEBSERVER_H
#define _OTAWEBSERVER_H

#include <HardwareSerial.h>

void OtaWebServerSetup(HardwareSerial* debugger_in);
void OtaWebServerLoop();

#endif//_OTAWEBSERVER_H
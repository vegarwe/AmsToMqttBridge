#ifndef _SELFSERVICEWEBSERVER_H
#define _SELFSERVICEWEBSERVER_H

#include <Stream.h>
#include "configuration.h"

// otaUser/otaPass: HTTP Basic auth for /serverIndex and /update.
void SelfServiceWebServerSetup(configuration* config_in, Stream* debugger_in, const char* otaUser, const char* otaPass);
void SelfServiceWebServerLoop();
void SelfServiceWebServerActivePower(int32_t timestamp, int32_t p);

#endif//_SELFSERVICEWEBSERVER_H

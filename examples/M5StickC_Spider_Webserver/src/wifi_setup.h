#ifndef _wifi_setup_h_
#define _wifi_setup_h_

/********************************
* for Wifi Connection
*********************************/
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>

#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <Wire.h>

uint64_t GetChipid();
void WiFiMgrSetup(char WiFiAPname[]);
void OTASetup(char OTAHostname[]);

// --------------------------------------
// i2c_scanner
// --------------------------------------
void i2c_scan();

#endif

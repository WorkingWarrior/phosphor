#pragma once

#include <Arduino.h>

// Connects to saved Wi-Fi or opens the setup portal, then starts mDNS, NTP and the HTTP API.
void Net_Begin(bool force_portal);
void Net_Loop();
bool Net_Connected();
String Net_Ip();
int Net_Rssi();
// Forgets Wi-Fi credentials and restarts into the setup portal.
void Net_ResetWifi();
// POSIX TZ string, e.g. "<+03>-3"; kept in NVS for the next boot, the RTC is resynced.
void Net_SetTimezone(const char *posix_tz);

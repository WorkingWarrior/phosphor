#pragma once

#include <Arduino.h>

// Looks for a newer release, nothing more: the device never downloads or installs an update by itself.
// Once after boot (Wi-Fi and time up) and then every 24 h it reads the "version" field of the manifest on the project
// site (HTTPS) and compares it with FW_VERSION. busy() tells it when the radio plays; it then waits, so a stream never
// stutters under the TLS handshake. Builds that are not a release tag (FW_VERSION "dev", "v1.0.2-3-g1a2b3c4") never show anything.
void UpdateCheck_Begin(bool (*busy)());
// The newer release tag ("v1.2.0") or an empty string.
const char *UpdateCheck_Newer();
// Where to read about it (compiled in, never taken from the response).
const char *UpdateCheck_Site();
// HTTP status of the last request (negative = connection/TLS error), 0 before the first.
int UpdateCheck_Code();
// Free internal heap just before and after the last request, for /status (0 until one ran).
void UpdateCheck_Heap(uint32_t &before, uint32_t &after);

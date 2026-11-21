#pragma once

// Improv Serial v1 (https://www.improv-wifi.com/serial/) over the USB serial port, so the web installer (ESP Web Tools)
// can recognise an installed Phosphor and offer "Update" instead of a fresh install, and set the Wi-Fi from the browser.
// Anything on the port that is not an Improv packet is ignored; the debug log on Serial keeps working.

// Reads incoming bytes and moves a running Wi-Fi join or network scan forward. Cheap when idle: call it every loop.
void Improv_Poll();

// True while the board is joining a network the installer asked for; the normal reconnect timer should stay out of it.
bool Improv_Busy();

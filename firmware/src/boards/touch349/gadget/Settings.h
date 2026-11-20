#pragma once

// User settings of the desk gadget, kept in a small text file on the board's flash (LittleFS on the "ffat"
// partition) and edited from a phone at http://<name>.local/settings. Nothing here needs a rebuild: city, language,
// clock style, pomodoro lengths and the whole radio list.
//
// File format, one setting per line (the same text is what GET /api/settings returns and POST /api/settings takes):
//   city=Lisbon
//   lat=38.7167
//   lon=-9.1333
//   lang=tr                 tr | en
//   h24=1                   24-hour clock
//   focus=25  short=5  long=15   pomodoro minutes
//   group=80S|80s           radio button label (up to 6 letters) | full genre name; order = button order
//   station=0|Heart 80s|http://...   group index | name | stream URL (MP3 or AAC over HTTP/HTTPS)

#include <Arduino.h>

struct RadioGroup {
  String key;   // on the preset button
  String name;  // above the station name
};

struct RadioStation {
  uint8_t group;
  String name;
  String url;
};

namespace settings {

constexpr int MAX_GROUPS = 7;
constexpr int MAX_STATIONS = 60;

void begin();                                // mount the file system, load the file or the defaults
String text();                               // all settings in the file format
bool apply(const String &body, String &err); // parse, check, save, take effect
uint32_t version();                          // goes up on every apply (weather refetches, radio reloads)

bool tr();  // Turkish texts on screen, else English
bool h24();
String city();
float lat();
float lon();
int focusMin();
int shortMin();
int longMin();

int groupCount();
const RadioGroup &group(int i);
int stationCount();
const RadioStation &station(int i);

// UTC offset reported by the weather service for the chosen city; sets the clock's time zone.
void setUtcOffset(int seconds);
void tick();  // main loop: applies a new time zone

}  // namespace settings

// The /settings page (SettingsPage.cpp).
extern const char SETTINGS_PAGE[];

// Pick the text for the current language.
inline const char *L(const char *en, const char *tr) { return settings::tr() ? tr : en; }

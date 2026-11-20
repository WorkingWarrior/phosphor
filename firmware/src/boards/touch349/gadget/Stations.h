#pragma once
// Internet radio stations, by genre (design/retro/stations.json, verified 2026-11-19: HTTP 200, MP3 or AAC; most
// send ICY titles). Genre order is the order of the preset buttons on the radio page; pressing a genre again steps
// through its stations.

// These are only the defaults: the list people actually hear lives in the settings file (Settings.h) and is edited
// from the settings page.

struct DefaultStation {
  uint8_t genre;
  const char *name;
  const char *url;
};

struct DefaultGroup {
  const char *key;
  const char *name;
};

static const DefaultGroup DEFAULT_GROUPS[] = { { "80S", "80s" },    { "SYNTH", "Synthwave" }, { "90S", "90s" },
                                               { "CHILL", "Chillout" }, { "LOFI", "Lo-fi" },  { "JAZZ", "Jazz" },
                                               { "TR90", "Turkish 90s" } };
static constexpr int DEFAULT_GROUP_COUNT = sizeof(DEFAULT_GROUPS) / sizeof(DEFAULT_GROUPS[0]);
static constexpr int DEFAULT_STATION = 6;  // Nightride FM

static const DefaultStation DEFAULT_STATIONS[] = {
  { 0, "HEART 80S", "http://media-ssl.musicradio.com/Heart80sMP3" },  // mp3 128 kbps
  { 0, "SOMA UNDER 80S", "http://ice1.somafm.com/u80s-128-mp3" },  // mp3 128 kbps
  { 0, "80S80S WEB", "http://streams.80s80s.de/web/mp3-128" },  // mp3 128 kbps
  { 0, "0N 80S", "http://0n-80s.radionetz.de/0n-80s.mp3" },  // mp3 128 kbps
  { 0, "80S80S WAVE", "http://streams.80s80s.de/wave/mp3-128" },  // mp3 128 kbps
  { 0, "FLUX 80S", "http://streams.fluxfm.de/80er/mp3-128/streams.fluxfm.de/" },  // mp3 128 kbps
  { 1, "NIGHTRIDE FM", "http://stream.nightride.fm/nightride.mp3" },  // mp3 320 kbps
  { 1, "FREKVENCI NITE", "http://stream.frekvenci.com/nightdrive" },  // mp3 128 kbps
  { 1, "NEON DRIVE FM", "https://engine.sleradiogroup.com/stream/neon-drive" },  // mp3 128 kbps
  { 1, "SOMA VAPORWAVE", "http://ice1.somafm.com/vaporwaves-128-mp3" },  // mp3 128 kbps
  { 1, "SOMA SYNPHAERA", "http://ice1.somafm.com/synphaera-128-mp3" },  // mp3 128 kbps
  { 1, "SOMA POPTRON", "http://ice1.somafm.com/poptron-128-mp3" },  // mp3 128 kbps
  { 2, "HEART 90S", "http://media-ssl.musicradio.com/Heart90sMP3" },  // mp3 128 kbps
  { 2, "0N 90S", "http://0n-90s.radionetz.de/0n-90s.mp3" },  // mp3 128 kbps
  { 2, "90S90S WEB", "http://streams.90s90s.de/pop/mp3-128" },  // mp3 128 kbps
  { 2, "90S90S DANCE", "http://streams.90s90s.de/dance/mp3-128" },  // mp3 128 kbps
  { 2, "SUNSHINE 90ER", "http://stream.sunshine-live.de/90er/mp3-128/stream.sunshine-live.de" },  // mp3 128 kbps
  { 2, "ILOVE THE 90S", "http://streams.ilovemusic.de/iloveradio24.mp3" },  // mp3 192 kbps
  { 3, "GROOVE SALAD", "http://ice1.somafm.com/groovesalad-128-mp3" },  // mp3 128 kbps
  { 3, "DRONE ZONE", "http://ice1.somafm.com/dronezone-128-mp3" },  // mp3 128 kbps
  { 3, "0N CHILLOUT", "http://0n-chillout.radionetz.de/0n-chillout.mp3" },  // mp3 128 kbps
  { 3, "SOMA LUSH", "http://ice1.somafm.com/lush-128-mp3" },  // mp3 128 kbps
  { 3, "FLUX CHILLOUT", "http://streams.fluxfm.de/chillout/mp3-128/streams.fluxfm.de/" },  // mp3 128 kbps
  { 3, "ANTENNE CHILL", "http://mp3channels.webradio.antenne.de/chillout" },  // mp3 128 kbps
  { 4, "LOFI 24/7", "http://usa9.fastcast4u.com/proxy/jamz?mp=/1" },  // mp3 128 kbps
  { 4, "FLUX CHILLHOP", "https://streams.fluxfm.de/Chillhop/mp3-128/streams.fluxfm.de/" },  // mp3 128 kbps
  { 4, "SOMA FLUID", "http://ice1.somafm.com/fluid-128-mp3" },  // mp3 128 kbps
  { 4, "LOFI HIPHOP", "http://stream.zeno.fm/f3wvbbqmdg8uv" },  // mp3 128 kbps
  { 4, "CHILLHOP TUNES", "http://stream.zeno.fm/fyn8eh3h5f8uv" },  // mp3 128 kbps
  { 4, "FLUX LOFI", "http://streams.fluxfm.de/lofi/mp3-128/streams.fluxfm.de/" },  // mp3 128 kbps
  { 5, "SWISS JAZZ", "http://stream.srg-ssr.ch/m/rsj/mp3_128" },  // mp3 128 kbps
  { 5, "0N JAZZ", "http://0n-jazz.radionetz.de/0n-jazz.mp3" },  // mp3 128 kbps
  { 5, "SOMA SONIC UNI", "http://ice1.somafm.com/sonicuniverse-128-mp3" },  // mp3 128 kbps
  { 5, "0N SMOOTH JAZZ", "http://0n-smoothjazz.radionetz.de/0n-smoothjazz.mp3" },  // mp3 128 kbps
  { 5, "FLUX JAZZ", "http://streams.fluxfm.de/jazzschwarz/mp3-128/streams.fluxfm.de/" },  // mp3 128 kbps
  { 5, "JAZZ RADIO FR", "http://jazzradio.ice.infomaniak.ch/jazzradio-high.mp3" },  // mp3 192 kbps
  { 6, "90LAR RADYO", "http://37.247.98.8/stream/166/" },  // mp3 128 kbps
  { 6, "RADYO 90", "http://live.radyo.in/9054/stream" },  // mp3 128 kbps
  { 6, "POP 90 TR", "http://playerservices.streamtheworld.com/api/livestream-redirect/POP90128AAC.aac" },  // aac 128 kbps
  { 6, "JOY TURK", "http://playerservices.streamtheworld.com/api/livestream-redirect/JOY_TURK_ITUNES.mp3" },  // mp3 128 kbps
  { 6, "RETRO TURK", "http://playerservices.streamtheworld.com/api/livestream-redirect/RETROTURK_SC" },  // mp3 64 kbps
  { 6, "RADYO VIVA", "http://edge1.radyotvonline.net/shoutcast/play/radyoviva" },  // aac 128 kbps
};
static constexpr int DEFAULT_STATION_COUNT = sizeof(DEFAULT_STATIONS) / sizeof(DEFAULT_STATIONS[0]);

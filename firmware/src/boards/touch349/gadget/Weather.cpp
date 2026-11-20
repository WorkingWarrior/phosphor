#ifdef BOARD_TOUCH349

#include "Weather.h"
#include <HTTPClient.h>
#include <WiFi.h>

#include "Settings.h"

// For the city chosen on the settings page; timezone=auto also tells the clock its UTC offset (summer time included).
static String weatherUrl()
{
  return "http://api.open-meteo.com/v1/forecast?latitude=" + String(settings::lat(), 4) +
         "&longitude=" + String(settings::lon(), 4) +
         "&current=temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code,is_day"
         "&daily=weather_code,temperature_2m_max&timezone=auto&forecast_days=4" +
         (settings::fahrenheit() ? "&temperature_unit=fahrenheit&wind_speed_unit=mph" : "");
}
#define REFRESH_MS (20 * 60 * 1000UL)
#define RETRY_MS (60 * 1000UL)

static SemaphoreHandle_t mx = nullptr;
static WeatherNow current;

static Sky skyFromCode(int code)
{
  if (code == 0) return Sky::Clear;
  if (code <= 2) return Sky::PartCloud;
  if (code == 3) return Sky::Cloud;
  if (code == 45 || code == 48) return Sky::Fog;
  if (code >= 51 && code <= 57) return Sky::Drizzle;
  if ((code >= 61 && code <= 67)) return Sky::Rain;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return Sky::Snow;
  if (code >= 80 && code <= 82) return Sky::Shower;
  if (code >= 95) return Sky::Storm;
  return Sky::Unknown;
}

const char *Weather_SkyName(Sky s)
{
  switch (s) {
    case Sky::Clear: return L("CLEAR", "AÇIK");
    case Sky::PartCloud: return L("PARTLY CLOUDY", "PARÇALI");
    case Sky::Cloud: return L("CLOUDY", "BULUTLU");
    case Sky::Fog: return L("FOG", "SİSLİ");
    case Sky::Drizzle: return L("DRIZZLE", "ÇİSELEME");
    case Sky::Rain: return L("RAIN", "YAĞMUR");
    case Sky::Shower: return L("SHOWERS", "SAĞANAK");
    case Sky::Snow: return L("SNOW", "KAR");
    case Sky::Storm: return L("STORM", "FIRTINA");
    default: return "---";
  }
}

// Number after "key": inside the section that starts at section (e.g. "\"current\":{").
static bool numberAfter(const String &body, const char *section, const char *key, float &out)
{
  int s = body.indexOf(section);
  if (s < 0) return false;
  int k = body.indexOf(key, s + strlen(section));
  if (k < 0) return false;
  out = body.substring(k + strlen(key)).toFloat();
  return true;
}

// Values of a JSON array "key":[a,b,c] inside a section.
static int arrayAfter(const String &body, const char *section, const char *key, String *out, int max_n)
{
  int s = body.indexOf(section);
  if (s < 0) return 0;
  int k = body.indexOf(key, s);
  if (k < 0) return 0;
  int open = body.indexOf('[', k), close = body.indexOf(']', open);
  if (open < 0 || close < 0) return 0;
  String list = body.substring(open + 1, close);
  int n = 0, start = 0;
  while (n < max_n && start <= (int)list.length()) {
    int comma = list.indexOf(',', start);
    if (comma < 0) comma = list.length();
    String v = list.substring(start, comma);
    v.replace("\"", "");
    out[n++] = v;
    start = comma + 1;
  }
  return n;
}

static int weekdayOf(const String &date)  // "2026-11-19"
{
  int y = date.substring(0, 4).toInt(), m = date.substring(5, 7).toInt(), d = date.substring(8, 10).toInt();
  static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
  if (m < 3) y--;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

static bool fetch()
{
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(weatherUrl())) return false;
  int code = http.GET();
  if (code != 200) {
    Serial.printf("weather: HTTP %d\n", code);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  WeatherNow w;
  float v;
  int k = body.indexOf("\"utc_offset_seconds\":");
  if (k >= 0) settings::setUtcOffset(body.substring(k + 21).toInt());
  const char *cur = "\"current\":{";
  if (!numberAfter(body, cur, "\"temperature_2m\":", v)) return false;
  w.temp = lroundf(v);
  if (numberAfter(body, cur, "\"relative_humidity_2m\":", v)) w.humidity = lroundf(v);
  if (numberAfter(body, cur, "\"wind_speed_10m\":", v)) w.wind = lroundf(v);
  if (numberAfter(body, cur, "\"weather_code\":", v)) w.sky = skyFromCode((int)v);
  if (numberAfter(body, cur, "\"is_day\":", v)) w.day = v > 0.5f;

  String dates[4], codes[4], maxes[4];
  const char *daily = "\"daily\":{";
  int n = arrayAfter(body, daily, "\"time\":", dates, 4);
  arrayAfter(body, daily, "\"weather_code\":", codes, 4);
  arrayAfter(body, daily, "\"temperature_2m_max\":", maxes, 4);
  for (int i = 1; i < n && i <= 3; i++) {  // tomorrow and the two days after
    w.next[i - 1].weekday = weekdayOf(dates[i]);
    w.next[i - 1].t_max = lroundf(maxes[i].toFloat());
    w.next[i - 1].sky = skyFromCode(codes[i].toInt());
  }
  w.valid = true;
  w.fetched_ms = millis();
  xSemaphoreTake(mx, portMAX_DELAY);
  current = w;
  xSemaphoreGive(mx);
  Serial.printf("weather: %d C, %s\n", w.temp, Weather_SkyName(w.sky));
  return true;
}

static void weatherTask(void *)
{
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      delay(5000);
      continue;
    }
    uint32_t ver = settings::version();
    uint32_t wait = fetch() ? REFRESH_MS : RETRY_MS, t0 = millis();
    while (millis() - t0 < wait && settings::version() == ver) delay(1000);  // a new city fetches at once
  }
}

void Weather_Begin()
{
  mx = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(weatherTask, "weather", 6144, nullptr, 1, nullptr, 0);
}

WeatherNow Weather_Get()
{
  if (!mx) return WeatherNow();
  xSemaphoreTake(mx, portMAX_DELAY);
  WeatherNow w = current;
  xSemaphoreGive(mx);
  return w;
}

#endif

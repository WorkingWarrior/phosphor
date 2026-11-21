#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <esp_heap_caps.h>
#include "UpdateCheck.h"
#include "state.h"

#define MANIFEST_URL "https://melihkoray.github.io/phosphor/manifest.json"
#define SITE "melihkoray.github.io/phosphor"
#define FIRST_DELAY_MS 20000UL        // after boot: give Wi-Fi, NTP and the first radio start time to settle
#define POLL_MS 5000UL                // waiting for Wi-Fi / time
#define BUSY_RETRY_MS 60000UL         // radio playing: look again in a minute
#define FAIL_RETRY_MS (60UL * 60000)  // a failed request: again in an hour
#define INTERVAL_MS (24UL * 3600000)  // a good one: again in 24 h
#define MIN_FREE_HEAP 38000           // a TLS handshake wants most of the internal RAM that is left (about 47 KB idle); below it, wait

static bool (*busy_fn)() = nullptr;
static char newer[16] = "";
static uint32_t heap_before = 0, heap_after = 0;
static int last_code = 0;  // HTTP status (or the HTTPClient error) of the last request, for /status

// "v1.2.0" or "1.2.0" -> major*1e6 + minor*1e3 + patch; -1 for anything else (a commit hash, "dev", "v1.0.2-3-g1a2b3c4").
static long parseVersion(const char *s)
{
  if (*s == 'v') s++;
  int part[3] = { 0, 0, 0 };
  for (int i = 0; i < 3; i++) {
    if (*s < '0' || *s > '9') return -1;
    while (*s >= '0' && *s <= '9') part[i] = part[i] * 10 + (*s++ - '0');
    if (i < 2) {
      if (*s != '.') return -1;
      s++;
    }
  }
  if (*s) return -1;
  if (part[0] > 999 || part[1] > 999 || part[2] > 999) return -1;
  return part[0] * 1000000L + part[1] * 1000L + part[2];
}

// The value of "version" in the small JSON body; false if it is not there.
static bool versionField(const String &body, char *out, size_t cap)
{
  int k = body.indexOf("\"version\"");
  if (k < 0) return false;
  k = body.indexOf(':', k);
  int a = k < 0 ? -1 : body.indexOf('"', k);
  int b = a < 0 ? -1 : body.indexOf('"', a + 1);
  if (b < 0 || (size_t)(b - a - 1) >= cap) return false;
  strncpy(out, body.c_str() + a + 1, b - a - 1);
  out[b - a - 1] = 0;
  return true;
}

// The precompiled mbedTLS allocates from internal RAM only, and a handshake needs more than the ~48 KB the radio and the
// pages leave free (the request failed with -1 there). For the length of this one request its allocator is pointed at
// PSRAM (internal as the fallback), then put back to the stock one: the internal heap is not touched, so nothing else stutters.
extern "C" {
int mbedtls_platform_set_calloc_free(void *(*calloc_func)(size_t, size_t), void (*free_func)(void *));
}
static void *psramCalloc(size_t n, size_t size)
{
  void *p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
static void *internalCalloc(size_t n, size_t size) { return heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); }  // the stock setting (CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC)
static void psramFree(void *p) { heap_caps_free(p); }

// One request. Returns true when the manifest was read (whatever it said).
static bool check()
{
  heap_before = ESP.getFreeHeap();
  bool ok = false;
  char remote[24] = "";
  mbedtls_platform_set_calloc_free(psramCalloc, psramFree);
  {
    // setInsecure(): the answer only drives an informational notice; the address shown is compiled in, not read from the reply.
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(8);
    HTTPClient http;
    http.setTimeout(8000);
    http.setConnectTimeout(8000);
    if (http.begin(client, MANIFEST_URL)) {
      int code = http.GET();
      last_code = code;
      int len = http.getSize();
      if (code == 200 && len <= 4096) {  // the file is under 1 KB
        String body = http.getString();
        ok = versionField(body, remote, sizeof(remote));
      } else {
        Serial.printf("update: HTTP %d\n", code);
      }
      http.end();
    }
  }  // the TLS client is freed here
  mbedtls_platform_set_calloc_free(internalCalloc, psramFree);
  heap_after = ESP.getFreeHeap();
  Serial.printf("update: free heap %u -> %u B, remote %s\n", (unsigned)heap_before, (unsigned)heap_after, ok ? remote : "(none)");
  if (!ok) return false;

  long mine = parseVersion(FW_VERSION), theirs = parseVersion(remote);
#ifdef UPDATE_TEST_VERSION  // bench builds only: pretend the site announced this release
  mine = parseVersion("v1.0.0");
  theirs = parseVersion(UPDATE_TEST_VERSION);
  strncpy(remote, UPDATE_TEST_VERSION, sizeof(remote) - 1);
#endif
  if (mine >= 0 && theirs > mine) {
    snprintf(newer, sizeof(newer), "v%ld.%ld.%ld", theirs / 1000000L, theirs / 1000 % 1000, theirs % 1000);
    Serial.printf("update: %s is available (this is %s)\n", newer, FW_VERSION);
  } else {
    newer[0] = 0;
  }
  state.dirty = true;
  return true;
}

static void updateTask(void *)
{
  delay(FIRST_DELAY_MS);
  for (;;) {
    struct tm t;
    bool ready = WiFi.status() == WL_CONNECTED && getLocalTime(&t, 0) && t.tm_year >= 124;
    if (!ready) {
      delay(POLL_MS);
      continue;
    }
    if ((busy_fn && busy_fn()) || ESP.getFreeHeap() < MIN_FREE_HEAP) {
      delay(BUSY_RETRY_MS);
      continue;
    }
    delay(check() ? INTERVAL_MS : FAIL_RETRY_MS);
  }
}

void UpdateCheck_Begin(bool (*busy)())
{
  busy_fn = busy;
  xTaskCreatePinnedToCore(updateTask, "update_check", 10240, nullptr, 1, nullptr, 0);
}

const char *UpdateCheck_Newer() { return newer; }
const char *UpdateCheck_Site() { return SITE; }
int UpdateCheck_Code() { return last_code; }
void UpdateCheck_Heap(uint32_t &before, uint32_t &after)
{
  before = heap_before;
  after = heap_after;
}

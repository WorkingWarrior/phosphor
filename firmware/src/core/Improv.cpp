// Improv Serial v1, see Improv.h.
//
// Packet: "IMPROV" | version 1 | type | length | data[length] | checksum (sum of all bytes before it) | '\n' on the way out.
// Types: 1 state, 2 error, 3 RPC command (from the installer), 4 RPC result (from us).
// RPC data: command | length of the rest | rest. Strings in results and in the Wi-Fi command are length-prefixed.
// Commands handled: 1 send Wi-Fi settings, 2 current state, 3 device info, 4 scan networks.

#include "Improv.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include "net.h"
#include "state.h"

namespace {

// The installer compares this with "name" in docs/manifest.json: equal means "Update" (and no erase question).
const char FIRMWARE_NAME[] = "Phosphor";
const char CHIP_NAME[] = "ESP32-S3";

const uint32_t CONNECT_TIMEOUT_MS = 20000;  // the installer itself gives up after 30 s
const uint32_t FAST_FAIL_AFTER_MS = 2000;   // a rejected password reports failure at once, but not from a stale status
const uint32_t SCAN_TIMEOUT_MS = 10000;
const uint32_t RX_GAP_MS = 1000;            // a half packet is dropped after this long without a byte
const size_t MAX_SCAN_RESULTS = 20;

const uint8_t PROTOCOL_VERSION = 1;
const uint8_t HEADER[6] = { 'I', 'M', 'P', 'R', 'O', 'V' };
const size_t HEAD_LEN = 9;  // header, version, type, length

enum : uint8_t { STATE_AUTHORIZED = 0x02, STATE_PROVISIONING = 0x03, STATE_PROVISIONED = 0x04 };  // 0x01 would need a button press
enum : uint8_t { ERR_NONE = 0x00, ERR_INVALID_RPC = 0x01, ERR_UNKNOWN_RPC = 0x02, ERR_UNABLE_TO_CONNECT = 0x03 };
enum : uint8_t { TYPE_STATE = 0x01, TYPE_ERROR = 0x02, TYPE_RPC = 0x03, TYPE_RESULT = 0x04 };
enum : uint8_t { CMD_WIFI = 0x01, CMD_STATE = 0x02, CMD_INFO = 0x03, CMD_SCAN = 0x04 };

enum class Job { None, Connect, Scan };

uint8_t rx[HEAD_LEN + 255 + 1];
size_t rx_n = 0;
uint32_t rx_ms = 0;

Job job = Job::None;
uint32_t job_ms = 0;
String want_ssid;                 // the network being joined
String old_ssid, old_pass;        // what was saved before, put back if the join fails

// ---- sending ----------------------------------------------------------------------------------------------------

// One Serial.write per packet so a debug line from another task cannot land in the middle of it.
void sendPacket(uint8_t type, const uint8_t *data, size_t len)
{
  uint8_t out[HEAD_LEN + 255 + 2];
  memcpy(out, HEADER, sizeof(HEADER));
  out[6] = PROTOCOL_VERSION;
  out[7] = type;
  out[8] = (uint8_t)len;
  memcpy(out + HEAD_LEN, data, len);
  uint8_t sum = 0;
  for (size_t i = 0; i < HEAD_LEN + len; i++) {
    sum += out[i];
  }
  out[HEAD_LEN + len] = sum;
  out[HEAD_LEN + len + 1] = '\n';
  Serial.write(out, HEAD_LEN + len + 2);
}

void sendState(uint8_t value)
{
  sendPacket(TYPE_STATE, &value, 1);
}

void sendError(uint8_t value)
{
  sendPacket(TYPE_ERROR, &value, 1);
}

// RPC result: command | total length of the strings | (length, bytes)... ; no strings at all ends a scan list.
void sendResult(uint8_t command, const char *const *strings, size_t count)
{
  uint8_t data[255];
  size_t n = 2;
  for (size_t i = 0; i < count; i++) {
    size_t len = strlen(strings[i]);
    if (n + 1 + len > sizeof(data)) {
      if (n + 1 >= sizeof(data)) {
        break;
      }
      len = sizeof(data) - n - 1;
    }
    data[n++] = (uint8_t)len;
    memcpy(data + n, strings[i], len);
    n += len;
  }
  data[0] = command;
  data[1] = (uint8_t)(n - 2);
  sendPacket(TYPE_RESULT, data, n);
}

String settingsUrl()
{
  return "http://" + Net_Ip() + "/settings";
}

uint8_t currentState()
{
  if (job == Job::Connect) {
    return STATE_PROVISIONING;
  }
  return Net_Connected() ? STATE_PROVISIONED : STATE_AUTHORIZED;
}

// ---- Wi-Fi join and scan ----------------------------------------------------------------------------------------

// The network saved in the Wi-Fi driver, whether or not it is connected right now.
void savedNetwork(String &ssid, String &pass)
{
  wifi_config_t conf;
  memset(&conf, 0, sizeof(conf));
  if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
    ssid = String(reinterpret_cast<char *>(conf.sta.ssid));
    pass = String(reinterpret_cast<char *>(conf.sta.password));
  } else {
    ssid = "";
    pass = "";
  }
}

// The esp32 core keeps the network in NVS on every begin() (storage is chosen once, when Wi-Fi first starts, and
// Net_Begin starts it with persistence on), so this is the same place the setup portal saves to.
void startConnect(const String &ssid, const String &pass)
{
  savedNetwork(old_ssid, old_pass);
  want_ssid = ssid;
  job = Job::Connect;
  job_ms = millis();
  sendError(ERR_NONE);
  sendState(STATE_PROVISIONING);
  if (Net_Connected()) {
    WiFi.disconnect(false, false);  // a fresh attempt, even for the network we are on (new password, same name)
  }
  WiFi.begin(ssid.c_str(), pass.length() ? pass.c_str() : nullptr);
}

void finishConnect(bool ok)
{
  job = Job::None;
  if (ok) {
    String url = settingsUrl();
    const char *strings[] = { url.c_str() };
    sendState(STATE_PROVISIONED);
    sendResult(CMD_WIFI, strings, 1);
    Serial.printf("improv: joined %s, %s\n", want_ssid.c_str(), Net_Ip().c_str());
    return;
  }
  // Put the previous network back (or forget the bad one) so a typo does not cost the working setup.
  if (old_ssid.length()) {
    WiFi.begin(old_ssid.c_str(), old_pass.length() ? old_pass.c_str() : nullptr);
  } else {
    WiFi.disconnect(false, true);
  }
  sendState(STATE_AUTHORIZED);
  sendError(ERR_UNABLE_TO_CONNECT);
  Serial.printf("improv: could not join %s\n", want_ssid.c_str());
}

void pollConnect()
{
  wl_status_t status = WiFi.status();
  uint32_t waited = millis() - job_ms;
  if (status == WL_CONNECTED && WiFi.SSID() == want_ssid) {
    finishConnect(true);
  } else if (waited > CONNECT_TIMEOUT_MS || (status == WL_CONNECT_FAILED && waited > FAST_FAIL_AFTER_MS)) {
    finishConnect(false);
  }
}

void startScan()
{
  if (WiFi.scanNetworks(true, false) == WIFI_SCAN_FAILED) {
    sendResult(CMD_SCAN, nullptr, 0);
    return;
  }
  job = Job::Scan;
  job_ms = millis();
}

// One result per network (ssid, rssi, "YES" if it needs a password), then an empty one. The installer waits for the
// empty one, so every way out of a scan ends here.
void finishScan(int found)
{
  job = Job::None;
  size_t sent = 0;
  for (int i = 0; i < found && sent < MAX_SCAN_RESULTS; i++) {
    String ssid = WiFi.SSID(i);
    bool dup = false;
    for (int j = 0; j < i && !dup; j++) {
      dup = WiFi.SSID(j) == ssid;
    }
    if (!ssid.length() || dup) {
      continue;
    }
    char rssi[8];
    snprintf(rssi, sizeof(rssi), "%d", (int)WiFi.RSSI(i));
    const char *strings[] = { ssid.c_str(), rssi, WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "NO" : "YES" };
    sendResult(CMD_SCAN, strings, 3);
    sent++;
  }
  WiFi.scanDelete();
  sendResult(CMD_SCAN, nullptr, 0);
}

void pollScan()
{
  int found = WiFi.scanComplete();
  if (found == WIFI_SCAN_RUNNING) {
    if (millis() - job_ms > SCAN_TIMEOUT_MS) {
      WiFi.scanDelete();
      job = Job::None;
      sendResult(CMD_SCAN, nullptr, 0);
    }
    return;
  }
  finishScan(found < 0 ? 0 : found);
}

// ---- receiving --------------------------------------------------------------------------------------------------

// data = what follows the RPC command byte and its length byte.
void handleCommand(uint8_t command, const uint8_t *data, size_t len)
{
  switch (command) {
    case CMD_WIFI: {
      // ssid length, ssid, password length, password; the lengths must account for every byte.
      if (len < 2 || 2 + (size_t)data[0] > len) {
        sendError(ERR_INVALID_RPC);
        return;
      }
      size_t ssid_len = data[0];
      size_t pass_len = data[1 + ssid_len];
      if (!ssid_len || ssid_len > 32 || pass_len > 63 || 2 + ssid_len + pass_len != len) {
        sendError(ERR_INVALID_RPC);
        return;
      }
      if (job == Job::Connect) {
        return;  // already joining; the answer to that attempt is on its way
      }
      String ssid, pass;
      for (size_t i = 0; i < ssid_len; i++) {
        ssid += (char)data[1 + i];
      }
      for (size_t i = 0; i < pass_len; i++) {
        pass += (char)data[2 + ssid_len + i];
      }
      startConnect(ssid, pass);
      return;
    }
    case CMD_STATE: {
      uint8_t value = currentState();
      sendState(value);
      if (value == STATE_PROVISIONED) {  // the installer waits for the address when we are on a network
        String url = settingsUrl();
        const char *strings[] = { url.c_str() };
        sendResult(CMD_STATE, strings, 1);
      }
      return;
    }
    case CMD_INFO: {
      const char *strings[] = { FIRMWARE_NAME, FW_VERSION, CHIP_NAME, state.name.c_str() };
      sendResult(CMD_INFO, strings, 4);
      return;
    }
    case CMD_SCAN:
      if (job == Job::None) {
        startScan();
      } else {
        sendResult(CMD_SCAN, nullptr, 0);  // busy: an empty list, not silence, so the installer does not wait for ever
      }
      return;
    default:
      sendError(ERR_UNKNOWN_RPC);
  }
}

void handlePacket()
{
  size_t len = rx[8];
  uint8_t sum = 0;
  for (size_t i = 0; i < HEAD_LEN + len; i++) {
    sum += rx[i];
  }
  if (sum != rx[HEAD_LEN + len]) {
    sendError(ERR_INVALID_RPC);
    return;
  }
  if (rx[7] != TYPE_RPC) {
    return;  // states and results only travel from us to the installer
  }
  // data = command, length of the rest, rest
  if (len < 2 || rx[HEAD_LEN + 1] != len - 2) {
    sendError(ERR_INVALID_RPC);
    return;
  }
  handleCommand(rx[HEAD_LEN], rx + HEAD_LEN + 2, len - 2);
}

// True when the byte belongs to the packet being collected (or completed it); false when it broke it.
bool accept(uint8_t b)
{
  if (rx_n < sizeof(HEADER) && b != HEADER[rx_n]) {
    return false;
  }
  if (rx_n == sizeof(HEADER) && b != PROTOCOL_VERSION) {
    return false;
  }
  rx[rx_n++] = b;
  if (rx_n >= HEAD_LEN && rx_n == HEAD_LEN + (size_t)rx[8] + 1) {
    rx_n = 0;
    handlePacket();
  }
  return true;
}

void feed(uint8_t b)
{
  uint32_t now = millis();
  if (rx_n && now - rx_ms > RX_GAP_MS) {
    rx_n = 0;
  }
  rx_ms = now;
  // A byte that breaks a packet may itself start the next one ("I" of a lost packet followed by a whole one).
  if (!accept(b)) {
    rx_n = 0;
    accept(b);
  }
}

}  // namespace

void Improv_Poll()
{
  for (int i = 0; i < 512 && Serial.available() > 0; i++) {
    int b = Serial.read();
    if (b >= 0) {
      feed((uint8_t)b);
    }
  }
  if (job == Job::Connect) {
    pollConnect();
  } else if (job == Job::Scan) {
    pollScan();
  }
}

bool Improv_Busy()
{
  return job == Job::Connect;
}

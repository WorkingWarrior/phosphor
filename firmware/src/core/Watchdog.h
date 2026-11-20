#pragma once

// If loop() stops coming round (a driver waiting for hardware that never answers), the board reboots itself instead of
// sitting there with Wi-Fi up and nothing else working. Handlers that legitimately run long (OTA, audio upload) feed it.

#include <esp_task_wdt.h>
#include <esp_system.h>

#define WATCHDOG_S 40

inline void Watchdog_Begin()
{
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = WATCHDOG_S * 1000;
  cfg.trigger_panic = true;
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) {
    esp_task_wdt_init(&cfg);
  }
  esp_task_wdt_add(nullptr);  // the Arduino loop task
}

inline void Watchdog_Feed()
{
  esp_task_wdt_reset();
}

// Why the board last restarted; shown in /status so a freeze that fixed itself leaves a trace.
inline const char *Watchdog_ResetReason()
{
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "int_wdt";
    case ESP_RST_TASK_WDT: return "task_wdt";
    case ESP_RST_WDT: return "wdt";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_USB: return "usb";
    default: return "other";
  }
}

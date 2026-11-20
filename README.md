# Phosphor

A retro VFD-style desk gadget for the **Waveshare ESP32-S3-Touch-LCD-3.49** (640×172 bar display). Clock, weather,
internet radio, focus timer and a set of slow pixel scenes, all drawn to look like an old vacuum-fluorescent display.

![Phosphor in the hand: tilting the board tilts the sea under the liner](media/hero.gif)

**[Install from the browser →](https://melihkoray.github.io/phosphor/)** (Chrome or Edge, USB cable, no tools needed)

> [!WARNING]
> Installing erases the board. Whatever runs on it now, the Waveshare factory demo included, is replaced and is not
> backed up. Phosphor cannot restore it.

## What it does

| | |
|---|---|
| ![Clock](media/clock.gif) | **Clock.** Big segment digits, date, alarm, battery. 12 or 24 hour. |
| ![Weather](media/weather.gif) | **Weather.** Now and the next three days for your city ([Open-Meteo](https://open-meteo.com), no key). |
| ![Radio](media/radio.gif) | **Internet radio.** Up to 7 genre buttons; tap again for the next station. MP3 and AAC streams. |
| ![Timer](media/timer.gif) | **Timer.** Tap-to-add minutes, alarm at the end. |
| ![Study](media/study.gif) | **Focus timer.** Pomodoro rounds in a little study room. |

Pixel scenes, to leave running: aquarium, snowy cabin, fireplace with a sleeping cat, seaside at night, an ocean liner
that tilts with the board, a lava lamp and an hourglass you turn over.

| ![Snow](media/snow.gif) | ![Fireplace](media/fire.gif) |
|---|---|
| ![Seaside](media/sea.gif) | ![Aquarium](media/aquarium.gif) |

## Setup

1. Open the [installer page](https://melihkoray.github.io/phosphor/), connect the board with a USB-C data cable, press
   **Install Phosphor** and pick **USB JTAG/serial debug unit** from the port list (`cu.usbmodem…` on a Mac, `COMx` on
   Windows, `ttyACM0` on Linux). Not sure? Unplug the board and plug it back in: the entry that comes back is the board.
2. After the restart the screen shows a Wi-Fi name like `Phosphor-Setup-1A2B`. Join it from your phone, pick your home
   Wi-Fi and give the device a name (for example `desk`). The name field starts as `phosphor-1a2b`, with the same
   four characters as the setup Wi-Fi.
3. Open `http://desk.local/settings` (with your device's name) on the phone. If the phone cannot open `.local` addresses
   (some Android phones), use the board's IP address from your router's device list. There you set:
   - **City:** search by name; weather and the time zone follow it.
   - **Language:** English or Turkish screen texts.
   - **Clock:** 12 or 24 hour.
   - **Focus timer:** focus, short and long break minutes.
   - **Radio:** groups and stations. Search [Radio Browser](https://www.radio-browser.info) or paste a stream URL,
     test it on the device, reorder, delete, export the list to a file and import it again.

Settings are stored on the board and survive restarts and updates.

**Touch:** swipe left or right for the next page, up or down for the volume; taps work the page's own buttons. Back
buttons: PWR turns the screen off and on (hold 3 s on battery to power off), BOOT changes brightness (hold 5 s to forget
Wi-Fi), RESET restarts.

## Building it yourself

Needs [arduino-cli](https://arduino.github.io/arduino-cli/) and the ESP32 core (`arduino-cli core install esp32:esp32@3.3.12`).

```bash
./build.sh                          # compile; output in build/
./build.sh /dev/cu.usbmodem101      # compile and flash over USB
```

To update over Wi-Fi, copy `firmware/secrets.h.example` to `firmware/secrets.h`, set a long random `OTA_TOKEN`, flash
once over USB, then `./build.sh desk.local`. Without a token, Wi-Fi updates are switched off.

Every push to `main` is built by GitHub Actions and published to the installer page; tags (`v1.0.0`) become releases.

## HTTP API

Everything the settings page does is plain HTTP on your local network:

| Request | |
|---|---|
| `GET /status` | JSON status |
| `GET /screen` | the current screen, raw RGB565 640×172 |
| `POST /page` `name=clock\|radio\|weather\|timer\|aquarium\|snow\|fire\|sea\|study\|wave\|lava\|hourglass` | show a page |
| `POST /cmd` `name=radio arg=next\|stop\|<index>` | radio control (also `vol`, `alarm`, `timer`, `theme arg=vfd\|ega\|natural`) |
| `POST /text` `text=... title=... ttl=60` | show a message |
| `GET/POST /api/settings` | the settings as `key=value` lines (POST needs the header `X-Phosphor: 1`) |
| `POST /update` | firmware update, needs the `X-Token` header (see above) |

## Security notes

- The device is meant for a home network. Pages, radio and settings can be changed by anyone on the same Wi-Fi, without
  a password. Do not expose it to the internet.
- Firmware updates, renaming and Wi-Fi reset over the network need `OTA_TOKEN`; the published builds have none, so
  those are off.
- There is no microphone code and no cloud account. The board talks to Open-Meteo (weather), NTP servers (time) and the
  radio streams you choose; the settings page in your phone's browser also talks to Open-Meteo geocoding and Radio Browser.

## Credits and license

GPL-3.0, see [LICENSE](LICENSE). Uses [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S) (GPL-3.0,
vendored in `libraries/`), [U8g2](https://github.com/olikraus/u8g2) (BSD-2-Clause),
[WiFiManager](https://github.com/tzapu/WiFiManager) (MIT), [SensorLib](https://github.com/lewisxhe/SensorLib) (MIT),
the Espressif AXS15231B display and touch drivers (Apache-2.0) and
[ESP Web Tools](https://esphome.github.io/esp-web-tools/) for the installer. Not affiliated with Waveshare.

Made by [Melih Koray](https://melihkoray.com).

#!/usr/bin/env bash
# Build (and optionally flash) the Phosphor firmware for the Waveshare ESP32-S3-Touch-LCD-3.49.
#
#   ./build.sh                     compile only; outputs in build/
#   ./build.sh /dev/cu.usbmodemXXX upload over USB (serial port path)
#   ./build.sh phosphor-1a2b.local upload over Wi-Fi (OTA); needs OTA_TOKEN in firmware/secrets.h
#
# The firmware reports the version in /status, the log and the installer: env PHOSPHOR_VERSION (CI sets the tag name),
# else `git describe --tags --always` (v1.0.2, or v1.0.2-3-g1a2b3c4 between releases), else "dev".
#
# Needs arduino-cli with the esp32:esp32 core installed (arduino-cli core install esp32:esp32).
# Libraries U8g2, WiFiManager and SensorLib are installed from the Library Manager; ESP32-audioI2S is vendored in libraries/.
# Besides the app image, build/phosphor-full.bin is written: bootloader + partitions + boot_app0 + app merged into one
# file that is flashed at offset 0x0 (for web installers or `esptool write_flash 0x0 build/phosphor-full.bin`).
set -euo pipefail
cd "$(cd "$(dirname "$0")" && pwd)"

TARGET="${1:-}"
SKETCH="firmware"
BUILD="build"
FQBN="esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,CDCOnBoot=cdc"
U8G2_VERSION="2.36.18"
SENSORLIB_VERSION="0.3.1"
WIFIMANAGER_VERSION="2.0.17"

VERSION="${PHOSPHOR_VERSION:-$(git describe --tags --always 2>/dev/null || echo dev)}"

command -v arduino-cli >/dev/null || { echo "arduino-cli not found: https://arduino.github.io/arduino-cli/"; exit 1; }

arduino-cli lib install "U8g2@${U8G2_VERSION}" "WiFiManager@${WIFIMANAGER_VERSION}" "SensorLib@${SENSORLIB_VERSION}"

arduino-cli compile --fqbn "$FQBN" \
  --libraries libraries \
  --build-property "compiler.c.extra_flags=-DBOARD_TOUCH349" \
  --build-property "compiler.cpp.extra_flags=-DBOARD_TOUCH349 -DFW_VERSION=\"${VERSION}\"" \
  --build-path "$BUILD" "$SKETCH"

# The esp32 core already writes the merged image next to the app binary.
if [[ -f "$BUILD/$SKETCH.ino.merged.bin" ]]; then
  cp "$BUILD/$SKETCH.ino.merged.bin" "$BUILD/phosphor-full.bin"
else
  # Fallback: merge by hand. ESP32-S3 offsets: bootloader 0x0, partition table 0x8000, boot_app0 0xe000, app 0x10000.
  PLATFORM="$(arduino-cli compile --fqbn "$FQBN" --show-properties "$SKETCH" | sed -n 's/^runtime.platform.path=//p')"
  esptool --chip esp32s3 merge_bin -o "$BUILD/phosphor-full.bin" --flash_mode dio --flash_freq 80m --flash_size 16MB \
    0x0 "$BUILD/$SKETCH.ino.bootloader.bin" \
    0x8000 "$BUILD/$SKETCH.ino.partitions.bin" \
    0xe000 "$PLATFORM/tools/partitions/boot_app0.bin" \
    0x10000 "$BUILD/$SKETCH.ino.bin"
fi
echo "Version: $VERSION"
echo "Built: $BUILD/$SKETCH.ino.bin (app), $BUILD/phosphor-full.bin (full flash image, write at 0x0)"

[[ -z "$TARGET" ]] && exit 0

if [[ "$TARGET" == /dev/* ]]; then
  arduino-cli upload --fqbn "$FQBN" --port "$TARGET" --input-dir "$BUILD" "$SKETCH"
else
  TOKEN=""
  [[ -f "$SKETCH/secrets.h" ]] && TOKEN="$(sed -n 's/^#define OTA_TOKEN "\(.*\)"/\1/p' "$SKETCH/secrets.h")"
  if [[ -z "$TOKEN" ]]; then
    echo "OTA needs OTA_TOKEN in $SKETCH/secrets.h (copy secrets.h.example); it must also be flashed on the device."
    exit 1
  fi
  echo "OTA upload to $TARGET ..."
  curl -4 -fsS --max-time 120 -H "X-Token: $TOKEN" -F "firmware=@$BUILD/$SKETCH.ino.bin" "http://$TARGET/update"
  echo
fi
echo "Uploaded to $TARGET"

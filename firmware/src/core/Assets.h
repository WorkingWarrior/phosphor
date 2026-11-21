#pragma once

// The asset pack: pictures, fonts and other read-only data kept out of the app image, in the "assets" partition
// (partitions.csv). tools/pack_assets.py builds it from assets/; the web installer writes it next to the firmware and
// POST /assets replaces it over Wi-Fi. The pack is memory-mapped, so find() returns a pointer straight into flash.
//
// Layout (little endian): "PHPK", u16 format (1), u16 count, u32 total bytes, char version[16], then count entries of
// { char name[24]; u32 offset; u32 size; } and the data, each blob 4-byte aligned. Offsets are from the pack start.

#include <Arduino.h>

namespace assets {

bool begin();                     // maps the pack; false when there is none (old partition table, nothing written)
bool ok();
const char *version();            // the pack's version string, "" without a pack
const uint8_t *find(const char *name, uint32_t *size = nullptr);  // nullptr when missing

// Replacing the pack (POST /assets): unmaps it, then writes the stream from the start of the partition.
bool writeBegin();
bool write(const uint8_t *data, size_t len);
bool writeEnd();  // true when the new pack reads back valid; restart afterwards

}  // namespace assets

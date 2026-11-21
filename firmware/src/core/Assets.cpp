#include "Assets.h"
#include <esp_partition.h>

namespace assets {

namespace {

struct Header {
  char magic[4];
  uint16_t format;
  uint16_t count;
  uint32_t total;
  char version[16];
};

struct Entry {
  char name[24];
  uint32_t offset;
  uint32_t size;
};

const esp_partition_t *part = nullptr;
const uint8_t *base = nullptr;
esp_partition_mmap_handle_t map_handle;
bool mapped = false;

size_t w_done = 0, w_erased = 0;
bool w_ok = false;

const esp_partition_t *partition()
{
  if (!part) part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)0x40, "assets");
  return part;
}

bool valid(const Header &h, size_t room)
{
  return !memcmp(h.magic, "PHPK", 4) && h.format == 1 && h.total <= room &&
         sizeof(Header) + (size_t)h.count * sizeof(Entry) <= h.total;
}

void unmap()
{
  if (mapped) esp_partition_munmap(map_handle);
  mapped = false;
  base = nullptr;
}

}  // namespace

bool begin()
{
  if (mapped) return true;
  const esp_partition_t *p = partition();
  if (!p) {
    Serial.println("assets: no partition (old partition table)");
    return false;
  }
  Header h;
  if (esp_partition_read(p, 0, &h, sizeof(h)) != ESP_OK || !valid(h, p->size)) {
    Serial.println("assets: no pack");
    return false;
  }
  const void *ptr = nullptr;
  if (esp_partition_mmap(p, 0, h.total, ESP_PARTITION_MMAP_DATA, &ptr, &map_handle) != ESP_OK) {
    Serial.println("assets: map failed");
    return false;
  }
  base = (const uint8_t *)ptr;
  mapped = true;
  Serial.printf("assets: %u items, %u bytes, version %.16s\n", h.count, (unsigned)h.total, h.version);
  return true;
}

bool ok() { return mapped; }

const char *version()
{
  static char v[17];
  if (!mapped) return "";
  memcpy(v, ((const Header *)base)->version, 16);
  v[16] = 0;
  return v;
}

const uint8_t *find(const char *name, uint32_t *size)
{
  if (!mapped) return nullptr;
  const Header *h = (const Header *)base;
  const Entry *e = (const Entry *)(base + sizeof(Header));
  for (int i = 0; i < h->count; i++) {
    if (!strncmp(e[i].name, name, sizeof(e[i].name))) {
      if (e[i].offset + e[i].size > h->total) return nullptr;
      if (size) *size = e[i].size;
      return base + e[i].offset;
    }
  }
  return nullptr;
}

bool writeBegin()
{
  const esp_partition_t *p = partition();
  unmap();  // nothing may read the pack while its flash changes
  w_done = w_erased = 0;
  w_ok = p != nullptr;
  return w_ok;
}

bool write(const uint8_t *data, size_t len)
{
  const esp_partition_t *p = partition();
  if (!w_ok || !p || w_done + len > p->size) return w_ok = false;
  // erase a sector ahead of the data, so only as much flash is erased as the new pack needs
  while (w_erased < w_done + len) {
    if (esp_partition_erase_range(p, w_erased, p->erase_size) != ESP_OK) return w_ok = false;
    w_erased += p->erase_size;
  }
  if (esp_partition_write(p, w_done, data, len) != ESP_OK) return w_ok = false;
  w_done += len;
  return true;
}

bool writeEnd()
{
  const esp_partition_t *p = partition();
  if (!w_ok || !p) return false;
  Header h;
  return esp_partition_read(p, 0, &h, sizeof(h)) == ESP_OK && valid(h, p->size) && h.total <= w_done;
}

}  // namespace assets

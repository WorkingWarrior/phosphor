#!/usr/bin/env python3
"""Packs assets/ into the asset pack the firmware maps from its "assets" partition (firmware/src/core/Assets.h).

usage: tools/pack_assets.py <assets_dir> <out.bin> <version>

Every file under assets_dir becomes one entry, named by its file name without the extension (art/snow_c64.img ->
"snow_c64"), so names must be unique across folders and at most 23 bytes.
Layout (little endian): "PHPK", u16 format 1, u16 count, u32 total, char version[16], then count x
{ char name[24]; u32 offset; u32 size; }, then the data, each blob 4-byte aligned.
"""
import os
import struct
import sys

PARTITION_SIZE = 0x600000  # firmware/partitions.csv

src, out, version = sys.argv[1:4]
files = []
for root, _, names in os.walk(src):
    for n in names:
        if not n.startswith('.'):
            files.append(os.path.join(root, n))
files.sort()

entries, seen = [], set()
for path in files:
    name = os.path.splitext(os.path.basename(path))[0]
    if len(name.encode()) > 23:
        sys.exit(f'{path}: name longer than 23 bytes')
    if name in seen:
        sys.exit(f'{path}: duplicate name {name}')
    seen.add(name)
    with open(path, 'rb') as f:
        entries.append((name, f.read()))

head = 4 + 2 + 2 + 4 + 16
offset = head + len(entries) * 32
table, data = b'', b''
for name, blob in entries:
    offset = (offset + 3) & ~3
    pad = offset - (head + len(entries) * 32 + len(data))
    data += b'\0' * pad
    table += struct.pack('<24sII', name.encode(), offset, len(blob))
    data += blob
    offset += len(blob)
total = head + len(table) + len(data)
if total > PARTITION_SIZE:
    sys.exit(f'pack is {total} bytes, the partition holds {PARTITION_SIZE}')
with open(out, 'wb') as f:
    f.write(struct.pack('<4sHHI16s', b'PHPK', 1, len(entries), total, version.encode()[:16]))
    f.write(table)
    f.write(data)
print(f'{out}: {len(entries)} items, {total} bytes ({total * 100 // PARTITION_SIZE}% of the partition), version {version}')

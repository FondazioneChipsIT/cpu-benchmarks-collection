#!/usr/bin/env python3
import sys, struct

dtb_file  = sys.argv[1]
memh_file = sys.argv[2]
base_addr = int(sys.argv[3], 16)

data = open(dtb_file, "rb").read()
if len(data) % 8:
    data += b'\x00' * (8 - len(data) % 8)

with open(memh_file, "w") as f:
    f.write(f"@{base_addr // 8:08x}\n")
    for i in range(0, len(data), 8):
        word = struct.unpack(">Q", data[i:i+8])[0]
        f.write(f"{word:016x}\n")

print(f"DTB size: {len(data)} bytes ({len(data)//1024} KB), scritto in {memh_file}")

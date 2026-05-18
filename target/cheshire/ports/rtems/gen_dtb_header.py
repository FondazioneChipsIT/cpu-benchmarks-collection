#!/usr/bin/env python3
import sys

with open(sys.argv[1], 'rb') as f:
    data = f.read()

chunk = [f'0x{b:02x}' for b in data]
rows = [chunk[i:i+12] for i in range(0, len(chunk), 12)]
formatted = ',\n  '.join(', '.join(r) for r in rows)

header = f"""/* SPDX-License-Identifier: BSD-2-Clause */
/* WARNING: Automatically generated -- do not edit! */
#ifndef __cheshire_dtb_h
#define __cheshire_dtb_h
#include <rtems/score/basedefs.h>
#include <sys/types.h>
const unsigned char system_dtb[] RTEMS_ALIGNED(8) = {{
  {formatted}
}};
const size_t system_dtb_size = sizeof(system_dtb);
#endif /* __cheshire_dtb_h */
"""

with open(sys.argv[2], 'w') as f:
    f.write(header)
print(f"[DTB] Written {len(data)} bytes to {sys.argv[2]}")

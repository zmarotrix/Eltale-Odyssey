"""Dump Quest 64's spell table from a USA 1.0 ROM.

The data block at ROM 0xD4B3C0 is copied to RAM 0x800C0000 at boot. At RAM 0x800C1B14
there are 5 pointers (Fire, Earth, Water, Wind, Enemy) to arrays of 0x44-byte spell records.
A spell ID is (table << 8) | index.
"""
import csv, struct, sys

ROM_BASE, RAM_BASE = 0xD4B3C0, 0x800C0000
TABLE_PTRS = 0x800C1B14
REC = 0x44
NAMES = sys.argv[2:] or []

rom = open(sys.argv[1], "rb").read()
def ram(addr, n): o = addr - RAM_BASE + ROM_BASE; return rom[o:o + n]
ptrs = struct.unpack(">5I", ram(TABLE_PTRS, 20))
ends = list(ptrs[1:]) + [TABLE_PTRS]

rows = []
for t, (start, end) in enumerate(zip(ptrs, ends)):
    for i in range((end - start) // REC):
        b = ram(start + i * REC, REC)
        rows.append((t, i, start + i * REC, b))

with open("spells_raw.csv", "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["id", "ram"] + [f"h{o:02X}" for o in range(0, REC, 2)])
    for t, i, addr, b in rows:
        w.writerow([f"{t:X}{i:02X}", f"{addr:08X}"] + [f"{v:04X}" for v in struct.unpack(f">{REC//2}H", b)])
print(f"{len(rows)} spells; tables at " + ", ".join(f"{p:08X}" for p in ptrs))

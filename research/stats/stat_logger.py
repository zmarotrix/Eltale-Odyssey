"""Poll Brian's stat block through the dev panel and log every change.

Writes one line per changed field to the log file given as the first argument.
"""
import json
import struct
import sys
import time
import urllib.request

BASE = "http://127.0.0.1:43064"
START, LENGTH = 0x8007BA80, 0x60
# Known fields (offset from START, size, name). Everything else is logged as raw words.
FIELDS = {
    0x04: (2, "hp"), 0x06: (2, "maxHp"), 0x08: (2, "mp"), 0x0A: (2, "maxMp"),
    0x0C: (2, "agility"), 0x0E: (2, "defense"), 0x10: (4, "elementExp"), 0x14: (4, "word_94"),
    0x24: (1, "fire"), 0x25: (1, "earth"), 0x26: (1, "water"), 0x27: (1, "wind"),
}
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))


def snapshot():
    req = urllib.request.Request(f"{BASE}/api/mem?addr={START}&len={LENGTH}", headers={"X-Q64-Dev": "1"})
    return bytes.fromhex(json.load(opener.open(req, timeout=2))["hex"])


def decode(raw):
    out = {}
    covered = set()
    for off, (size, name) in FIELDS.items():
        out[name] = int.from_bytes(raw[off:off + size], "big")
        covered.update(range(off, off + size))
    for off in range(0, LENGTH, 4):
        if not covered.intersection(range(off, off + 4)):
            out[f"raw_{START + off:08X}"] = struct.unpack(">I", raw[off:off + 4])[0]
    return out


log = open(sys.argv[1], "a", buffering=1, encoding="utf-8")
prev = None
while True:
    try:
        cur = decode(snapshot())
    except Exception as e:
        time.sleep(1)
        continue
    stamp = time.strftime("%H:%M:%S")
    if prev is None:
        log.write(f"{stamp} BASELINE {json.dumps(cur)}\n")
    else:
        for k, v in cur.items():
            if prev.get(k) != v:
                log.write(f"{stamp} {k}: {prev.get(k)} -> {v}\n")
    prev = cur
    time.sleep(0.5)

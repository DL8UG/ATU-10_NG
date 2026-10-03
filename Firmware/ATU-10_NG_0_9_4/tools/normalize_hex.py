#!/usr/bin/env python3
"""Rewrite an XC8 Intel HEX file in the layout mikroC produces.

The ATU-10's on-board USB programmer (PIC16F1454) parses the HEX file
copied onto its drive. Its parser is closed source, so the output mimics
the mikroC files it is known to accept:

  - CRLF line endings, upper case hex digits
  - only data (00), extended linear address (04) and EOF (01) records
  - no leading 04 record, a 04 record only when the upper address changes
  - contiguous data split into records of <= 16 bytes, written in
    ascending order (mikroC's order is not required, the original files
    are not sorted)

It also refuses a hex file that would lock out the USB programmer: it
programs through ICSP in low-voltage mode, so LVP and MCLRE must stay on
and code / write protection off (CONFIG2, CONFIG4, CONFIG5).

Usage:
  normalize_hex.py IN OUT [--config W1,W2,W3,W4,W5]   normalize IN into OUT
  normalize_hex.py --check FILE                       check FILE's layout
"""

import sys

PROG_END = 0x10000                 # 32K words program flash, byte address
CONFIG_START = 0x1000E             # CONFIG1..CONFIG5, word address 0x8007
CONFIG_END = CONFIG_START + 10
CONFIG_MASK = [0x2977, 0x3EE3, 0x3F7F, 0x3003, 0x0003]   # implemented bits


def parse(path):
    mem = {}
    upper = 0
    with open(path, 'rb') as f:
        for n, raw in enumerate(f.read().decode('ascii').splitlines(), 1):
            line = raw.strip()
            if not line:
                continue
            if line[0] != ':':
                sys.exit(f'{path}:{n}: missing start code')
            rec = bytes.fromhex(line[1:])
            count, addr, rtype = rec[0], rec[1] << 8 | rec[2], rec[3]
            data = rec[4:4 + count]
            if len(rec) != count + 5 or sum(rec) & 0xFF:
                sys.exit(f'{path}:{n}: bad length or checksum')
            if rtype == 0x00:
                for i, b in enumerate(data):
                    mem[upper + addr + i] = b
            elif rtype == 0x01:
                break
            elif rtype == 0x04:
                upper = (data[0] << 8 | data[1]) << 16
            elif rtype == 0x02:
                upper = (data[0] << 8 | data[1]) << 4
            # 03/05 (start address) carry no data for a PIC
    return mem


def record(addr, rtype, data):
    rec = bytes([len(data), addr >> 8 & 0xFF, addr & 0xFF, rtype]) + bytes(data)
    return ':' + (rec + bytes([-sum(rec) & 0xFF])).hex().upper()


def normalize(mem):
    lines = []
    upper = 0
    addrs = sorted(mem)
    i = 0
    while i < len(addrs):
        # contiguous run within one 64K segment
        start = addrs[i]
        j = i
        while (j + 1 < len(addrs) and addrs[j + 1] == addrs[j] + 1
               and addrs[j + 1] >> 16 == start >> 16):
            j += 1
        if start >> 16 != upper:
            upper = start >> 16
            lines.append(record(0, 0x04, [upper >> 8, upper & 0xFF]))
        for a in range(start, addrs[j] + 1, 16):
            chunk = [mem[x] for x in range(a, min(a + 16, addrs[j] + 1))]
            lines.append(record(a & 0xFFFF, 0x00, chunk))
        i = j + 1
    lines.append(':00000001FF')
    return '\r\n'.join(lines) + '\r\n'


def check_ranges(mem, config=None):
    errors = []
    bad = [a for a in mem if not (a < PROG_END or CONFIG_START <= a < CONFIG_END)]
    if bad:
        errors.append(f'data outside program/config memory at 0x{min(bad):X}..0x{max(bad):X}')
    if any(a & 1 == 0 and a + 1 not in mem for a in mem):
        errors.append('incomplete program words')
    if config:
        for k, want in enumerate(config):
            a = CONFIG_START + 2 * k
            got = mem.get(a, 0xFF) | mem.get(a + 1, 0x3F) << 8
            if (got ^ want) & CONFIG_MASK[k]:
                errors.append(f'CONFIG{k + 1} is 0x{got:04X}, expected 0x{want:04X}')
    return errors


# (config word index, bit mask, value the bits must have, meaning)
FLASHABLE = [
    (1, 0x0001, 0x0001, 'MCLRE must be on'),
    (3, 0x2000, 0x2000, 'LVP must be on'),
    (3, 0x0003, 0x0003, 'WRT (write protection) must be off'),
    (4, 0x0001, 0x0001, 'CP (code protection) must be off'),
    (4, 0x0002, 0x0002, 'CPD (data protection) must be off'),
]


def check_flashable(mem):
    errors = []
    for k, mask, want, text in FLASHABLE:
        a = CONFIG_START + 2 * k
        if a not in mem:
            errors.append(f'CONFIG{k + 1} missing, {text}')
            continue
        got = mem[a] | mem.get(a + 1, 0) << 8
        if got & mask != want:
            errors.append(f'CONFIG{k + 1} is 0x{got:04X}: {text}')
    return errors


def check_layout(path):
    errors = []
    raw = open(path, 'rb').read()
    if b'\n' in raw.replace(b'\r\n', b''):
        errors.append('line endings are not CRLF')
    text = raw.decode('ascii')
    if text != text.upper():
        errors.append('lower case hex digits')
    for n, line in enumerate(text.split('\r\n'), 1):
        if not line:
            continue
        rec = bytes.fromhex(line[1:])
        count, rtype = rec[0], rec[3]
        if rtype not in (0x00, 0x01, 0x04):
            errors.append(f'line {n}: record type {rtype:02X}')
        if count > 16:
            errors.append(f'line {n}: record longer than 16 bytes')
        if rtype == 0x04 and n == 1:
            errors.append('leading 04 record')
    return errors


def main(argv):
    if len(argv) == 3 and argv[1] == '--check':
        mem = parse(argv[2])
        errors = check_layout(argv[2]) + check_ranges(mem) + check_flashable(mem)
        for e in errors:
            print(f'{argv[2]}: {e}')
        if not errors:
            print(f'{argv[2]}: OK')
        return 1 if errors else 0
    if len(argv) not in (3, 5) or (len(argv) == 5 and argv[3] != '--config'):
        print(__doc__)
        return 2
    config = [int(w, 16) for w in argv[4].split(',')] if len(argv) == 5 else None
    mem = parse(argv[1])
    errors = check_ranges(mem, config) + check_flashable(mem)
    for e in errors:
        print(f'{argv[1]}: {e}')
    if errors:
        return 1
    with open(argv[2], 'wb') as f:
        f.write(normalize(mem).encode('ascii'))
    prog = sum(1 for a in mem if a < PROG_END) // 2
    print(f'{argv[2]}: {prog} program words, config OK, USB programmer can flash it')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))

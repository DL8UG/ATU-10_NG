#!/usr/bin/env python3
"""Show or change the settings ("Cells") in an ATU-10 NG firmware hex file.

The Cells are 12 BCD coded bytes at byte address 0xEEE0 of the hex file,
stored as RETLW instructions (high byte 0x34). They fill exactly the two
records ':10EEE000...' (Cells 1-8) and ':10EEF000...' (Cells 9-12 + 4
spare). Only these two records are changed, everything else in the file
stays byte for byte as it is.

Usage:
  cells.py show FILE                     list the Cells
  cells.py set FILE OUT N=VALUE [...]    change Cells (decimal values), write OUT
  cells.py check FILE [--defaults]       check layout (and that the defaults are set)
"""

import sys

ADDR = 0xEEE0
COUNT = 12
RETLW = 0x34

# number, name, unit / meaning, min, max, default  (keep in step with src/cells.c)
CELLS = [
    (1, 'Display off', 'minutes, 0 = never', 0, 99, 5),
    (2, 'Power off', 'minutes, 0 = never', 0, 99, 30),
    (3, 'Relay pulse', 'ms', 2, 30, 7),
    (4, 'Min. power for tuning', '0.1 W', 1, 99, 10),
    (5, 'Max. power for tuning', 'W', 1, 99, 15),
    (6, 'Auto tune SWR change', '(value - 10) / 10, 13 = 0.3', 11, 99, 13),
    (7, 'Auto tune', '1 = on, 0 = off', 0, 1, 1),
    (8, 'Calibration b', 'value / 10', 0, 99, 4),
    (9, 'Calibration a', '1 + value / 100', 0, 99, 14),
    (10, 'Peak hold', '10 ms', 1, 99, 60),
    (11, 'Tuning target', 'SWR 1 + value / 100, 0 = always full search', 0, 99, 8),
    (12, 'Search effort', '1 = quick, 2 = normal, 3 = thorough', 1, 3, 2),
]


class HexFile:
    def __init__(self, path):
        raw = open(path, 'rb').read()
        self.crlf = b'\r\n' in raw
        self.lines = raw.decode('ascii').splitlines()
        self.rec = {}   # record start address -> line index (upper address 0 only)
        upper = 0
        for n, line in enumerate(self.lines):
            if not line.startswith(':'):
                continue
            b = bytes.fromhex(line[1:])
            if len(b) != b[0] + 5 or sum(b) & 0xFF:
                raise SystemExit(f'{path}: line {n + 1}: bad record')
            if b[3] == 0x04:
                upper = b[4] << 8 | b[5]
            elif b[3] == 0x00 and upper == 0:
                self.rec[b[1] << 8 | b[2]] = n

    def record(self, addr):
        n = self.rec.get(addr)
        if n is None:
            return None
        return bytearray(bytes.fromhex(self.lines[n][1:]))

    def layout_errors(self):
        errors = []
        for addr in (ADDR, ADDR + 16):
            b = self.record(addr)
            if b is None:
                errors.append(f'no record starting at 0x{addr:04X}')
            elif b[0] != 16:
                errors.append(f'record 0x{addr:04X} has {b[0]} bytes, expected 16')
            elif any(b[4 + 2 * i + 1] != RETLW for i in range(8)):
                errors.append(f'record 0x{addr:04X} does not hold RETLW words')
        return errors

    def raw_cells(self):
        data = self.record(ADDR)[4:20] + self.record(ADDR + 16)[4:20]
        return [data[2 * i] for i in range(COUNT)]

    def set_raw(self, index, value):
        addr = ADDR + (index // 8) * 16
        b = self.record(addr)
        b[4 + 2 * (index % 8)] = value
        b[-1] = -sum(b[:-1]) & 0xFF
        self.lines[self.rec[addr]] = ':' + b.hex().upper()

    def save(self, path):
        nl = '\r\n' if self.crlf else '\n'
        open(path, 'wb').write((nl.join(self.lines) + nl).encode('ascii'))


def decode(raw, cell):
    hi, lo = raw >> 4, raw & 0x0F
    if hi > 9 or lo > 9:
        return None
    v = hi * 10 + lo
    return v if cell[3] <= v <= cell[4] else None


def show(h):
    for cell, raw in zip(CELLS, h.raw_cells()):
        v = decode(raw, cell)
        state = f'{v:3}' if v is not None else f'invalid, firmware uses {cell[5]}'
        print(f'{cell[0]:2}  0x{raw:02X}  {state:>3}  {cell[1]} ({cell[2]}, {cell[3]}..{cell[4]})')


def main(argv):
    if len(argv) >= 3 and argv[1] == 'show':
        h = HexFile(argv[2])
        errors = h.layout_errors()
        if errors:
            print('\n'.join(errors))
            return 1
        show(h)
        return 0
    if len(argv) >= 3 and argv[1] == 'check':
        h = HexFile(argv[2])
        errors = h.layout_errors()
        if not errors:
            for cell, raw in zip(CELLS, h.raw_cells()):
                if decode(raw, cell) is None:
                    errors.append(f'Cell {cell[0]} = 0x{raw:02X} is invalid')
                elif '--defaults' in argv and decode(raw, cell) != cell[5]:
                    errors.append(f'Cell {cell[0]} = 0x{raw:02X}, default is {cell[5]}')
        for e in errors:
            print(f'{argv[2]}: {e}')
        if not errors:
            print(f'{argv[2]}: Cells OK')
        return 1 if errors else 0
    if len(argv) >= 5 and argv[1] == 'set':
        h = HexFile(argv[2])
        errors = h.layout_errors()
        if errors:
            print('\n'.join(errors))
            return 1
        for arg in argv[4:]:
            n, _, v = arg.partition('=')
            n, v = int(n), int(v)
            if not 1 <= n <= COUNT:
                raise SystemExit(f'no Cell {n}')
            cell = CELLS[n - 1]
            if not cell[3] <= v <= cell[4]:
                raise SystemExit(f'Cell {n} ({cell[1]}): {v} is outside {cell[3]}..{cell[4]}')
            h.set_raw(n - 1, (v // 10) << 4 | v % 10)
        h.save(argv[3])
        show(h)
        return 0
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))

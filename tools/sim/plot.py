#!/usr/bin/env python3
"""Charts for the documentation, as self-contained SVG files (light and dark).

Usage:
  plot.py landscape MAP.tsv OUT.svg TITLE     SWR over L x C with the search path
                                              (MAP.tsv from: sim --ant N MHz --map MAP.tsv)
  plot.py times COLD.tsv HOP.tsv OUT.svg      tuning time: full search vs band change
  plot.py screen IN.pbm OUT.png               display picture, enlarged, OLED look

Colors follow the data viz reference palette: sequential blue for
magnitude, categorical slots 1 (blue) and 2 (orange); text in ink tokens.
"""

import base64
import math
import struct
import sys
import zlib

# ---- palette (light / dark)
SEQ = ['#cde2fb', '#b7d3f6', '#9ec5f4', '#86b6ef', '#6da7ec', '#5598e7', '#3987e5',
       '#2a78d6', '#256abf', '#1c5cab', '#184f95', '#104281', '#0d366b']   # steps 100..700
STYLE = '''<style>
  svg { --surface: #fcfcfb; --ink: #0b0b0b; --ink2: #52514e; --muted: #8a8984; --grid: #e4e3df;
        --s1: #2a78d6; --s2: #eb6834; font-family: system-ui, -apple-system, "Segoe UI", sans-serif; }
  @media (prefers-color-scheme: dark) {
    svg { --surface: #1a1a19; --ink: #ffffff; --ink2: #c3c2b7; --muted: #8f8e86; --grid: #33332f;
          --s1: #3987e5; --s2: #d95926; }
    .light { display: none; }
  }
  @media not (prefers-color-scheme: dark) { .dark { display: none; } }
  .bg { fill: var(--surface); }
  .t1 { fill: var(--ink); } .t2 { fill: var(--ink2); } .tm { fill: var(--muted); }
  .title { font-size: 15px; font-weight: 600; fill: var(--ink); }
  .sub { font-size: 12px; fill: var(--ink2); }
  .ax { font-size: 11px; fill: var(--ink2); }
  .gridline { stroke: var(--grid); stroke-width: 1; }
  .axis { stroke: var(--muted); stroke-width: 1; }
</style>'''


def hex_rgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


def ramp(t, steps):
    """t 0..1 along the ramp, linear between steps"""
    t = min(max(t, 0.0), 1.0) * (len(steps) - 1)
    i = min(int(t), len(steps) - 2)
    f = t - i
    a, b = hex_rgb(steps[i]), hex_rgb(steps[i + 1])
    return tuple(round(a[k] + (b[k] - a[k]) * f) for k in range(3))


def png(width, height, pixels):
    """pixels: rows of (r, g, b) tuples -> PNG bytes"""
    raw = b''.join(b'\x00' + bytes(c for px in row for c in px) for row in pixels)

    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def data_uri(b):
    return 'data:image/png;base64,' + base64.b64encode(b).decode()


# ---------------------------------------------------------------- landscape

def quality(swr):
    """0 = SWR 100 or worse .. 1 = SWR 1.0, logarithmic"""
    return 1 - min(math.log10(max(swr, 1.0)) / 2, 1.0)


def landscape(path, out, title):
    swr = {}
    steps, opt = [], None
    for line in open(path):
        p = line.split('\t')
        if p[0] == 'map':
            swr[int(p[1]), int(p[2]), int(p[3])] = float(p[4])
        elif p[0] == 'step':
            steps.append((int(p[1]), int(p[2]), int(p[3]), float(p[4])))
        elif p[0] == 'opt':
            opt = (int(p[1]), int(p[2]), int(p[3]), float(p[4]))
    steps = steps[1:]                     # the first one is the reset before the tune
    final = steps[-1]

    S = 2                                 # px per relay step
    W, PW = 760, 128 * S
    top, left, gap = 112, 64, 92
    H = top + PW + 64
    panels = [(0, 'capacitor at the input (SW 0)'), (1, 'capacitor at the output (SW 1)')]
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" '
         f'role="img" aria-label="{title}">', STYLE, f'<rect class="bg" width="{W}" height="{H}"/>',
         f'<text class="title" x="16" y="26">{title}</text>',
         f'<text class="sub" x="16" y="46">Colour: the SWR at each relay setting (scale top right); '
         f'the search measured {len(steps)} of 32768.</text>']
    # legend
    lx = 16
    o.append(f'<circle cx="{lx + 5}" cy="70" r="3.5" fill="none" stroke="var(--ink2)" stroke-width="1.5"/>'
             f'<text class="ax" x="{lx + 14}" y="74">grid point measured</text>')
    lx += 150
    o.append(f'<line x1="{lx}" y1="70" x2="{lx + 18}" y2="70" stroke="var(--s2)" stroke-width="2"/>'
             f'<circle cx="{lx + 9}" cy="70" r="2.5" fill="var(--s2)"/>'
             f'<text class="ax" x="{lx + 24}" y="74">local search</text>')
    lx += 120
    o.append(f'<circle cx="{lx + 6}" cy="70" r="6" fill="none" stroke="var(--ink)" stroke-width="2"/>'
             f'<text class="ax" x="{lx + 16}" y="74">result SWR {final[3]:.2f}</text>')
    lx += 140
    o.append(f'<path d="M{lx} 64 l12 12 M{lx + 12} 64 l-12 12" stroke="var(--ink)" stroke-width="2"/>'
             f'<text class="ax" x="{lx + 18}" y="74">best possible SWR {opt[3]:.2f}</text>')
    # colour key
    kx, ky = W - 150, 20
    for k in range(60):
        r, g, b = ramp(k / 59, SEQ)
        o.append(f'<rect class="light" x="{kx + k * 2}" y="{ky}" width="2" height="8" fill="rgb({r},{g},{b})"/>')
        r, g, b = ramp(1 - k / 59, SEQ)
        o.append(f'<rect class="dark" x="{kx + k * 2}" y="{ky}" width="2" height="8" fill="rgb({r},{g},{b})"/>')
    o.append(f'<text class="ax" x="{kx - 6}" y="{ky + 8}" text-anchor="end">SWR 100+</text>'
             f'<text class="ax" x="{kx + 126}" y="{ky + 8}">1.0</text>')

    grid = set()
    for sw, l, c, s in steps:
        grid.add((sw, l, c))
    for n, (sw, label) in enumerate(panels):
        x0 = left + n * (PW + gap)
        for mode, rev in (('light', False), ('dark', True)):
            rows = []
            for c in range(127, -1, -1):          # C upwards
                rows.append([ramp(1 - quality(swr[sw, l, c]) if rev else quality(swr[sw, l, c]), SEQ)
                             for l in range(128)])
            o.append(f'<image class="{mode}" x="{x0}" y="{top}" width="{PW}" height="{PW}" '
                     f'style="image-rendering: pixelated" href="{data_uri(png(128, 128, rows))}"/>')
        o.append(f'<rect x="{x0}" y="{top}" width="{PW}" height="{PW}" fill="none" class="axis" stroke="var(--muted)"/>')
        o.append(f'<text class="sub" x="{x0}" y="{top - 8}">{label}</text>')
        for v in (0, 32, 64, 96, 127):
            px = x0 + v * S + S / 2
            py = top + (127 - v) * S + S / 2
            o.append(f'<text class="ax" x="{px:.0f}" y="{top + PW + 16}" text-anchor="middle">{v}</text>')
            o.append(f'<text class="ax" x="{x0 - 6}" y="{py + 4:.0f}" text-anchor="end">{v}</text>')
        o.append(f'<text class="ax" x="{x0 + PW / 2}" y="{top + PW + 34}" text-anchor="middle">L relays (0 .. 18.5 uH)</text>')
        o.append(f'<text class="ax" transform="translate({x0 - 34} {top + PW / 2}) rotate(-90)" '
                 f'text-anchor="middle">C relays (0 .. 5 nF)</text>')

        def xy(l, c):
            return x0 + l * S + S / 2, top + (127 - c) * S + S / 2
        # grid points: the first measurements before the local search starts
        n_grid = 0
        for sw2, l, c, s in steps:
            if l in (0, 3, 10, 30, 90) and c in (0, 3, 10, 30, 90):
                n_grid += 1
            else:
                break
        for sw2, l, c, s in steps[:n_grid]:
            if sw2 == sw:
                x, y = xy(l, c)
                o.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.5" fill="none" stroke="var(--ink2)" stroke-width="1.5"/>')
        # local search path (consecutive relay steps on this side)
        pts = [xy(l, c) for sw2, l, c, s in steps[n_grid:] if sw2 == sw]
        if len(pts) > 1:
            o.append('<polyline fill="none" stroke="var(--s2)" stroke-width="1.5" stroke-linejoin="round" '
                     'stroke-opacity="0.85" points="' + ' '.join(f'{x:.1f},{y:.1f}' for x, y in pts) + '"/>')
        for x, y in pts:
            o.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="2" fill="var(--s2)"/>')
        if opt[0] == sw:
            x, y = xy(opt[1], opt[2])
            o.append(f'<path d="M{x - 6:.1f} {y - 6:.1f} l12 12 M{x + 6:.1f} {y - 6:.1f} l-12 12" '
                     f'stroke="var(--ink)" stroke-width="2"/>')
        if final[0] == sw:
            x, y = xy(final[1], final[2])
            o.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="8" fill="none" stroke="var(--ink)" stroke-width="2"/>')
    o.append('</svg>')
    open(out, 'w').write('\n'.join(o) + '\n')


# ---------------------------------------------------------------- tuning times

def load_times(path, skip_first=0):
    t = []
    for i, line in enumerate(open(path)):
        p = line.split('\t')
        if len(p) >= 8 and (not skip_first or i % 12 >= skip_first):
            t.append(float(p[7]))
    return t


def times(cold, hop, out):
    a = load_times(cold)
    b = load_times(hop, skip_first=3)     # hop runs: 12 tunes per set, the first round fills the memory
    bins = list(range(0, 15))
    def hist(v):
        h = [0] * len(bins)
        for x in v:
            h[min(int(x), len(bins) - 1)] += 1
        return [100.0 * n / len(v) for n in h]
    ha, hb = hist(a), hist(b)
    W, H = 760, 330
    left, top, pw, ph = 56, 92, 680, 180
    ymax = math.ceil(max(ha + hb) / 10) * 10
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="img" '
         f'aria-label="Tuning time">', STYLE, f'<rect class="bg" width="{W}" height="{H}"/>',
         '<text class="title" x="16" y="26">How long a tune takes (simulated)</text>',
         f'<text class="sub" x="16" y="46">Share of tunes per second of tuning time. Mean: full search '
         f'{sum(a) / len(a):.1f} s, band change with the memory {sum(b) / len(b):.1f} s. Last bar: 14 s and more.</text>']
    lx = 16
    for cls, text in (('--s1', f'full search ({len(a)} tunes)'), ('--s2', f'band change, memory filled ({len(b)} tunes)')):
        o.append(f'<rect x="{lx}" y="62" width="12" height="12" rx="2" fill="var({cls})"/>'
                 f'<text class="ax" x="{lx + 18}" y="72">{text}</text>')
        lx += 260
    for v in range(0, ymax + 1, 10):
        y = top + ph - ph * v / ymax
        o.append(f'<line class="gridline" x1="{left}" x2="{left + pw}" y1="{y:.1f}" y2="{y:.1f}"/>'
                 f'<text class="ax" x="{left - 8}" y="{y + 4:.1f}" text-anchor="end">{v} %</text>')
    bw = pw / len(bins)
    for i in range(len(bins)):
        for k, (h, col) in enumerate(((ha, '--s1'), (hb, '--s2'))):
            hgt = ph * h[i] / ymax
            if hgt <= 0:
                continue
            x = left + i * bw + 6 + k * (bw - 12) / 2 + (1 if k else 0)
            w = (bw - 12) / 2 - 1
            y = top + ph - hgt
            r = min(4, hgt, w / 2)
            o.append(f'<path d="M{x:.1f} {top + ph:.1f} V{y + r:.1f} Q{x:.1f} {y:.1f} {x + r:.1f} {y:.1f} '
                     f'H{x + w - r:.1f} Q{x + w:.1f} {y:.1f} {x + w:.1f} {y + r:.1f} V{top + ph:.1f} Z" '
                     f'fill="var({col})"><title>{i}-{i + 1} s: {h[i]:.1f} %</title></path>')
        o.append(f'<text class="ax" x="{left + i * bw:.1f}" y="{top + ph + 16}" text-anchor="middle">{i}</text>')
    o.append(f'<line class="axis" x1="{left}" x2="{left + pw}" y1="{top + ph}" y2="{top + ph}"/>')
    o.append(f'<text class="ax" x="{left + pw / 2}" y="{top + ph + 36}" text-anchor="middle">tuning time, seconds</text>')
    o.append('</svg>')
    open(out, 'w').write('\n'.join(o) + '\n')


# ---------------------------------------------------------------- display picture

def screen(pbm, out):
    tok = open(pbm).read().split()
    w, h = int(tok[1]), int(tok[2])
    bits = [int(x) for x in tok[3:3 + w * h]]
    z, pad = 4, 8
    on, off = (190, 225, 255), (8, 10, 14)
    rows = []
    for y in range(h * z + 2 * pad):
        row = []
        for x in range(w * z + 2 * pad):
            px, py = (x - pad) // z, (y - pad) // z
            lit = 0 <= px < w and 0 <= py < h and bits[py * w + px] and (x - pad) % z < z - 1 and (y - pad) % z < z - 1
            row.append(on if lit else off)
        rows.append(row)
    open(out, 'wb').write(png(w * z + 2 * pad, h * z + 2 * pad, rows))


def main(argv):
    if len(argv) == 5 and argv[1] == 'landscape':
        landscape(argv[2], argv[3], argv[4])
    elif len(argv) == 5 and argv[1] == 'times':
        times(argv[2], argv[3], argv[4])
    elif len(argv) == 4 and argv[1] == 'screen':
        screen(argv[2], argv[3])
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))

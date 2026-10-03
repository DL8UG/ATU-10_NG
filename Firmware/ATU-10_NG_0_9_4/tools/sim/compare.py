#!/usr/bin/env python3
"""Summarize simulator runs (tools/sim/sim output), optionally side by side.

Usage: compare.py A.tsv [B.tsv] [--table] [--worse]

Per suite: number of cases, mean SWR reached (capped at 10), share <= 1.5,
share within 0.05 of the best possible SWR (brute force), the same for the
matchable cases only (best possible <= 1.5), mean relay steps and mean /
max estimated tuning time. With two files: cases better / worse in B.
"""

import sys
from collections import OrderedDict

CLOSE = 0.05


def load(path):
    rows = []
    for line in open(path):
        p = line.rstrip('\n').split('\t')
        if len(p) < 8:
            continue
        rows.append(dict(suite=p[0], case=p[1], mhz=float(p[2]), swr=float(p[3]),
                         best=float(p[4]), steps=int(p[5]), meas=int(p[6]), time=float(p[7])))
    return rows


def stats(rows):
    n = len(rows)
    m = [r for r in rows if r['best'] <= 1.5]
    pct = lambda k, rs: 100.0 * k / len(rs) if rs else 0.0
    return dict(
        n=n,
        mean=sum(min(r['swr'], 10) for r in rows) / n,
        le15=pct(sum(r['swr'] <= 1.5 for r in rows), rows),
        close=pct(sum(r['swr'] <= r['best'] + CLOSE for r in rows), rows),
        nm=len(m),
        mclose=pct(sum(r['swr'] <= r['best'] + CLOSE for r in m), m),
        mle15=pct(sum(r['swr'] <= 1.5 for r in m), m),
        steps=sum(r['steps'] for r in rows) / n,
        time=sum(r['time'] for r in rows) / n,
        tmax=max(r['time'] for r in rows),
    )


def by_suite(rows):
    d = OrderedDict()
    for r in rows:
        d.setdefault(r['suite'], []).append(r)
    d['ALL'] = rows
    return d


HEAD = (f'{"":10}{"suite":8} {"cases":>5} {"meanSWR":>7} {"<=1.5":>6} {"~best":>6} '
        f'| {"match":>5} {"<=1.5":>6} {"~best":>6} | {"steps":>6} {"t_avg":>5} {"t_max":>5}')


def line(name, suite, s):
    return (f'{name:10}{suite:8} {s["n"]:5} {s["mean"]:7.3f} {s["le15"]:5.1f}% {s["close"]:5.1f}% '
            f'| {s["nm"]:5} {s["mle15"]:5.1f}% {s["mclose"]:5.1f}% '
            f'| {s["steps"]:6.1f} {s["time"]:5.1f} {s["tmax"]:5.1f}')


def main(argv):
    files = [a for a in argv[1:] if not a.startswith('--')]
    if not files:
        print(__doc__)
        return 2
    runs = [(f.split('/')[-1].rsplit('.', 1)[0], load(f)) for f in files]
    print(HEAD)
    suites = by_suite(runs[0][1]).keys()
    for suite in suites:
        for name, rows in runs:
            rs = by_suite(rows).get(suite)
            if rs:
                print(line(name, suite, stats(rs)))
    if len(runs) == 2:
        a, b = runs[0][1], runs[1][1]
        better = [(x, y) for x, y in zip(a, b) if y['swr'] < x['swr'] - 0.02]
        worse = [(x, y) for x, y in zip(a, b) if y['swr'] > x['swr'] + 0.02]
        print(f'{runs[1][0]} vs {runs[0][0]}: {len(better)} better, {len(worse)} worse, '
              f'{len(a) - len(better) - len(worse)} equal (+-0.02)')
        if '--worse' in argv:
            for x, y in worse:
                print(f'  WORSE {x["suite"]:8} {x["case"]:40} {x["mhz"]:7.3f}  best {x["best"]:.3f}  '
                      f'{runs[0][0]} {x["swr"]:.3f}  {runs[1][0]} {y["swr"]:.3f}')
    if '--table' in argv:
        for r in runs[-1][1]:
            print(f'{r["suite"]:8} {r["case"]:40} {r["mhz"]:7.3f}  best {r["best"]:6.3f}  '
                  f'got {r["swr"]:6.3f}  steps {r["steps"]:4}  {r["time"]:5.1f} s')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))

#!/usr/bin/env python3
"""Compare two --frame-stats CSV files: what a change did to the load (VS-13, perf proposal 4.6).

  v32sim-statdiff.py BEFORE.csv AFTER.csv [--skip N] [--columns a,b,...] [--fail-over PCT] [--fail-columns a,b,...] [--sort file|ratio] [--json]

For every numeric column (default: all after `frame`, except `end`) it prints the mean, 95th percentile and maximum of both
runs, with the change of the mean in percent. --skip leaves the first N frames out of both (the BIOS logo takes about 250).
Frames are paired by position, so the two runs should play the same input; only the totals are compared, never row by row.
--fail-over PCT exits with status 1 if the mean of any of --fail-columns (default: cycles, gpu_pixels, draws) rose by more
than PCT percent: a gate for a build script. --json prints the table as JSON instead of text. --sort ratio lists the biggest changes of the mean first (a column
that appears from nothing first of all). Either file may be CSV or JSON Lines (--stats-format=json); the failing columns may be
given by column name (draws) or stable name (gpu.draws).
"""
import argparse, csv, json, sys

STABLE = {'gpu.draws': 'draws', 'gpu.dropped': 'dropped', 'gpu.clears': 'clears', 'gpu.tex_switches': 'tex_switches',
          'gpu.region_switches': 'region_switches', 'gpu.regions': 'regions', 'spu.cmds': 'spu_cmds', 'spu.plays': 'spu_plays',
          'gpu.scaled_draws': 'scaled_draws', 'gpu.rotated_draws': 'rotated_draws', 'gpu.region_defs': 'region_defs',
          'gpu.color_changes': 'color_changes', 'gpu.blend_changes': 'blend_changes', 'spu.assignments': 'spu_assigns',
          'spu.param_writes': 'spu_params', 'bus.movs_words': 'movs_words', 'bus.sets_words': 'sets_words',
          'bus.gamepad_reads': 'gamepad_reads', 'cpu.instructions': 'instructions', 'cpu.cycles': 'cycles',
          'gpu.pixels': 'gpu_pixels', 'bus.memcard_accesses': 'memcard_accesses', 'spu.channels_playing': 'spu_playing', 'cpu.pct': 'cpu_pct', 'gpu.pct': 'gpu_pct'}

def read_rows(path):
    with open(path, newline='') as f:
        text = f.read()
    if text.lstrip().startswith('{'):                       # JSON Lines (--stats-format=json, stats save FILE json)
        rows = [json.loads(l) for l in text.splitlines() if l.strip()]
        return [{STABLE.get(k, k): str(v) for k, v in r.items()} for r in rows if 'frame' in r]
    return list(csv.DictReader(text.splitlines()))

def load(path, skip):
    rows = read_rows(path)[skip:]
    cols = {}
    for name in (rows[0].keys() if rows else []):
        if name in ('frame', 'end'):
            continue
        try:
            cols[name] = sorted(float(r[name]) for r in rows)
        except (ValueError, TypeError):
            pass
    return len(rows), cols

def stat(v):
    n = len(v)
    return (sum(v) / n, v[(n - 1) * 95 // 100], v[-1]) if n else (0.0, 0.0, 0.0)

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('before'); ap.add_argument('after')
    ap.add_argument('--skip', type=int, default=0)
    ap.add_argument('--columns')
    ap.add_argument('--fail-over', type=float)
    ap.add_argument('--fail-columns', default='cycles,gpu_pixels,draws')
    ap.add_argument('--sort', choices=('file', 'ratio'), default='file')
    ap.add_argument('--json', action='store_true')
    a = ap.parse_args()
    nb, cb = load(a.before, a.skip)
    na, ca = load(a.after, a.skip)
    names = [c for c in ([STABLE.get(x, x) for x in a.columns.split(',')] if a.columns else list(cb)) if c in cb and c in ca]
    table, worst = [], []
    for c in names:
        (mb, pb, xb), (ma, pa, xa) = stat(cb[c]), stat(ca[c])
        pct = (100.0 * (ma - mb) / mb) if mb else (0.0 if ma == 0 else float('inf'))
        table.append({'column': c, 'before': {'mean': mb, 'p95': pb, 'max': xb}, 'after': {'mean': ma, 'p95': pa, 'max': xa}, 'mean_change_pct': (None if pct == float('inf') else pct)})
        if a.fail_over is not None and c in [STABLE.get(x, x) for x in a.fail_columns.split(',')] and pct > a.fail_over:
            worst.append((c, pct))
    if a.sort == 'ratio':
        table.sort(key=lambda t: -(float('inf') if t['mean_change_pct'] is None else abs(t['mean_change_pct'])))
    if a.json:
        json.dump({'frames': {'before': nb, 'after': na}, 'columns': table}, sys.stdout, indent=1)
        print()
    else:
        print('frames: before %d, after %d' % (nb, na))
        print('%-22s %26s %26s %9s' % ('', 'before: mean p95 max', 'after: mean p95 max', 'mean'))
        for t in table:
            b, f = t['before'], t['after']
            print('%-22s %10.2f %7.0f %7.0f %10.2f %7.0f %7.0f %s' % (t['column'], b['mean'], b['p95'], b['max'], f['mean'], f['p95'], f['max'], ('%+8.1f%%' % t['mean_change_pct']) if t['mean_change_pct'] is not None else '     new'))
    for c, pct in worst:
        sys.stderr.write('[statdiff] FAILED: mean %s rose %.1f%% (limit %.1f%%)\n' % (c, pct, a.fail_over))
    return 1 if worst else 0

sys.exit(main())

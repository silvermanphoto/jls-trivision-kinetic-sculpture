#!/usr/bin/env python3
"""Clearance between neighbouring Trivision prisms, over every pair of angles.

Model: each prism is an equilateral triangle whose side is the face width,
turning about its centre (the shaft); neighbouring shafts sit one pitch apart.
Angle 0 is square on a face (one face flat to the front). Angles are degrees
from square, positive the same way for every prism; a prism looks the same
every 120 degrees.

Gap: when two neighbours are apart, the true distance between them; when they
overlap, minus the overlap depth (the shortest push that separates them, from
the separating-axis test). Zero or less means the prisms touch.

The defaults are the February 2026 simulators' figures (face 4.2536 in, pitch
4.45 in). The as-built pitch has not been measured, and the frame drawings
disagree (4.45, 4.70 and 5.12 in), so pass the measured face width (corner to
corner across one face) and the measured shaft-to-shaft pitch.

  python3 prism_clearance.py --face 4.237 --pitch 4.70     summary
  python3 prism_clearance.py --pitch 4.70 --offset 20      turning together, 20 degrees apart
  python3 prism_clearance.py --pitch 4.70 --parked 30      one turns past a neighbour parked 30 degrees off square
  python3 prism_clearance.py --pitch 4.70 --pair 12 0      one pair of angles, left then right
  python3 prism_clearance.py --pitch 4.70 --csv move.csv   a planned move: one row per moment, one column
                                                           per prism left to right, angles in degrees;
                                                           every neighbouring pair is checked

Exit status 1 when the checked motion touches; for the summary, when either of
the two motions the project instructions call safe (exact unison from square,
one prism turning beside square neighbours) touches.
"""
import argparse
import csv
import math
import sys

FACE_DEFAULT = 4.2536
PITCH_DEFAULT = 4.45


def triangle(face, angle, cx=0.0):
    """Corners of a prism turned `angle` degrees from square, centred at (cx, 0)."""
    r = face / math.sqrt(3.0)  # centre to corner
    # Corner at the back; the face between the 210 and 330 degree corners is flat to the front.
    return [(cx + r * math.cos(math.radians(b + angle)), r * math.sin(math.radians(b + angle)))
            for b in (90.0, 210.0, 330.0)]


def normals(tri):
    out = []
    for i in range(3):
        (x1, y1), (x2, y2) = tri[i], tri[(i + 1) % 3]
        length = math.hypot(x2 - x1, y2 - y1)
        out.append(((y2 - y1) / length, (x1 - x2) / length))
    return out


def seg_dist(p, a, b):
    ax, ay = a
    ex, ey = b[0] - ax, b[1] - ay
    t = ((p[0] - ax) * ex + (p[1] - ay) * ey) / (ex * ex + ey * ey)
    t = 0.0 if t < 0.0 else 1.0 if t > 1.0 else t
    return math.hypot(p[0] - ax - t * ex, p[1] - ay - t * ey)


def tri_gap(a, na, b, nb):
    """Signed gap between triangles a and b (with their edge normals na, nb)."""
    depth = math.inf
    for nx, ny in na + nb:
        pa = [nx * x + ny * y for x, y in a]
        pb = [nx * x + ny * y for x, y in b]
        push = min(max(pa) - min(pb), max(pb) - min(pa))
        if push <= 0.0:  # a separating axis: apart, so return the true distance
            return min(min(seg_dist(p, q[i], q[(i + 1) % 3]) for p in s for i in range(3))
                       for s, q in ((a, b), (b, a)))
        depth = min(depth, push)
    return -depth


def gap(face, pitch, left, right):
    a, b = triangle(face, left), triangle(face, right, pitch)
    return tri_gap(a, normals(a), b, normals(b))


class Grid:
    """Gap for every pair of angles in [0, 120) at a fixed step."""

    def __init__(self, face, pitch, step):
        self.n = int(round(120.0 / step))
        self.step = 120.0 / self.n
        left = [triangle(face, k * self.step) for k in range(self.n)]
        right = [[(x + pitch, y) for x, y in t] for t in left]
        nrm = [normals(t) for t in left]
        self.g = [[tri_gap(left[i], nrm[i], right[j], nrm[j]) for j in range(self.n)]
                  for i in range(self.n)]

    def at(self, i, j):
        return self.g[i % self.n][j % self.n]


def signed(angle):
    """Angle in (-60, 60]: how far from the nearest square position, and which way."""
    a = math.fmod(angle, 120.0)
    a = a + 120.0 if a < 0 else a
    return a - 120.0 if a > 60.0 else a


def runs(angles, step):
    """Group sorted angles into contiguous ranges."""
    out = []
    for a in sorted(angles):
        if out and a - out[-1][1] <= step * 1.5:
            out[-1][1] = a
        else:
            out.append([a, a])
    return out


def verdict(g):
    return "clear" if g > 0 else "TOUCHING"


def summary(face, pitch, step):
    r = face / math.sqrt(3.0)
    print("Trivision prism clearance")
    print(f"  face, corner to corner   {face:.4f} in")
    print(f"  shaft pitch              {pitch:.4f} in")
    reach = "wider than the pitch: neighbours' corners can meet" if 2 * r > pitch else \
        "no wider than the pitch: neighbours cannot meet"
    print(f"  corner sweep             {2 * r:.4f} in across, {reach}")
    grid = Grid(face, pitch, step)
    n, s = grid.n, grid.step
    print(f"\nGap in inches, zero or less = touching; angles checked every {s:g} degrees")

    unison = min(grid.at(i, i) for i in range(n))
    beside = min(min(grid.at(i, 0), grid.at(0, i)) for i in range(n))
    print(f"  Exact unison from square (all turning together)      {unison:7.3f}  {verdict(unison)}")
    print(f"  One turning, neighbour square on a face              {beside:7.3f}  {verdict(beside)}")

    worst, wi, wj = min((grid.at(i, j), i, j) for i in range(n) for j in range(n))
    print(f"  Worst pair of angles (left {signed(wi * s):+.2f}, right {signed(wj * s):+.2f})      "
          f"{worst:7.3f}  {verdict(worst)}")

    first, deepest = None, math.inf
    for k in range(1, n // 2 + 1):
        m = min(min(grid.at(i, i + k), grid.at(i + k, i)) for i in range(n))
        deepest = min(deepest, m)
        if m <= 0 and first is None:
            first = k * s
    if first is None:
        print("  Turning together at a fixed offset: never touches")
    else:
        print(f"  Turning together at a fixed offset: touches from {first:g} degrees apart"
              f" (deepest overlap {-deepest:.3f} in)")

    parked, deepest = [], math.inf
    for j in range(n):
        m = min(min(grid.at(i, j), grid.at(j, i)) for i in range(n))
        if m <= 0:
            parked.append(signed(j * s))
            deepest = min(deepest, m)
    if not parked:
        print("  One turning past a neighbour parked off square: never touches")
    else:
        spans = ", ".join(f"{lo:+g} to {hi:+g}" for lo, hi in runs(parked, s))
        print(f"  One turning past a neighbour parked off square: touches when the parked prism sits"
              f" {spans} degrees from square (deepest overlap {-deepest:.3f} in)")
    return 0 if unison > 0 and beside > 0 else 1


def sweep(face, pitch, step, fixed_pair):
    """Minimum gap over a full turn, where fixed_pair(theta) gives (left, right)."""
    n = int(round(120.0 / step))
    return min((gap(face, pitch, *fixed_pair(k * 120.0 / n)), k * 120.0 / n) for k in range(n))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--face", type=float, default=FACE_DEFAULT, help="face width, corner to corner, inches")
    ap.add_argument("--pitch", type=float, default=PITCH_DEFAULT, help="shaft-to-shaft distance, inches")
    ap.add_argument("--step", type=float, default=0.25, help="angle step in degrees (default 0.25)")
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--offset", type=float, help="turning together this many degrees apart")
    mode.add_argument("--parked", type=float, help="one turns past a neighbour parked this many degrees off square")
    mode.add_argument("--pair", type=float, nargs=2, metavar=("LEFT", "RIGHT"), help="one pair of angles")
    mode.add_argument("--csv", help="rows of angles, one column per prism, left to right")
    a = ap.parse_args()
    if a.face <= 0 or a.pitch <= 0 or a.step <= 0:
        ap.error("face, pitch and step must be positive")

    if a.offset is not None:
        m1, t1 = sweep(a.face, a.pitch, a.step, lambda t: (t, t + a.offset))
        m2, t2 = sweep(a.face, a.pitch, a.step, lambda t: (t + a.offset, t))
        m = min(m1, m2)
        print(f"Turning together {a.offset:g} degrees apart: minimum gap {m:.3f} in  {verdict(m)}")
    elif a.parked is not None:
        m1, _ = sweep(a.face, a.pitch, a.step, lambda t: (t, a.parked))
        m2, _ = sweep(a.face, a.pitch, a.step, lambda t: (a.parked, t))
        m = min(m1, m2)
        print(f"One turning past a neighbour parked {a.parked:g} degrees off square: "
              f"minimum gap {m:.3f} in  {verdict(m)}")
    elif a.pair is not None:
        m = gap(a.face, a.pitch, a.pair[0], a.pair[1])
        print(f"Left {a.pair[0]:g}, right {a.pair[1]:g} degrees: gap {m:.3f} in  {verdict(m)}")
    elif a.csv:
        m, where, touching, checks, rows = math.inf, None, 0, 0, 0
        with open(a.csv, newline="") as f:
            for line_no, row in enumerate(csv.reader(f), 1):
                try:
                    angles = [float(c) for c in row if c.strip() != ""]
                except ValueError:
                    continue  # a header or a note
                if len(angles) < 2:
                    continue
                rows += 1
                for p in range(len(angles) - 1):
                    g = gap(a.face, a.pitch, angles[p], angles[p + 1])
                    checks += 1
                    touching += g <= 0
                    if g < m:
                        m, where = g, (line_no, p + 1, p + 2)
        if rows == 0:
            sys.exit(f"{a.csv}: no rows of angles found")
        print(f"{rows} rows, {checks} neighbour checks: minimum gap {m:.3f} in  {verdict(m)}"
              f" (line {where[0]}, prisms {where[1]} and {where[2]}); {touching} checks touching")
    else:
        return summary(a.face, a.pitch, a.step)
    return 0 if m > 0 else 1


if __name__ == "__main__":
    sys.exit(main())

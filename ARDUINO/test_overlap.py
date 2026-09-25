# RETIRED: superseded by prism_clearance.py. The February 2026 clearance scripts disagree with one another; kept for history only, do not rely on their results.
import math

def get_vertices(theta, offset_x):
    W = 4.2536
    R = W / (2 * math.sin(math.radians(60)))
    v = [(offset_x + R * math.cos(math.radians(90 + theta))),
         (offset_x + R * math.cos(math.radians(210 + theta))),
         (offset_x + R * math.cos(math.radians(330 + theta)))]
    return v

# Try moving them one at a time.
# A moves 120 deg, THEN B moves 120 deg.
for t in range(0, 121, 10):
    vA = get_vertices(t, 0)
    vB = get_vertices(0, 4.45)
    print(f"A={t} B=0: Max A_x={max(vA):.3f}, Min B_x={min(vB):.3f}, Clearance={min(vB)-max(vA):.3f}")


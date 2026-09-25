# RETIRED: superseded by prism_clearance.py. The February 2026 clearance scripts disagree with one another; kept for history only, do not rely on their results.
import math
from shapely.geometry import Polygon

def get_polygon(theta, offset_x):
    W = 4.2536
    R = W / (2 * math.sin(math.radians(60)))
    v = []
    for angle in [90, 210, 330]:
        rad = math.radians(angle + theta)
        x = offset_x + R * math.cos(rad)
        y = R * math.sin(rad)
        v.append((x, y))
    return Polygon(v)

def check_stagger(stagger_deg, rot_dir):
    min_dist = 999
    # check from t=0 to 120 + stagger
    for t in range(0, 120 + stagger_deg + 1):
        # Prism A starts at t=0, goes to 120
        thetaA = min(max(t, 0), 120) * rot_dir
        # Prism B starts at t=stagger, goes to 120
        thetaB = min(max(t - stagger_deg, 0), 120) * rot_dir
        
        polyA = get_polygon(thetaA, 0)
        polyB = get_polygon(thetaB, 4.45)
        
        d = polyA.distance(polyB)
        if d < min_dist:
            min_dist = d
            
    return min_dist

print("Exact Clearance using Shapely (inches)")
print("Stagger\tCCW (L->R)")
for s in range(0, 121, 5):
    d = check_stagger(s, 1) # 1 = CCW
    print(f"{s} deg\t{d:.3f}")


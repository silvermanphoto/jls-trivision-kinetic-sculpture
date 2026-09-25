# RETIRED: superseded by prism_clearance.py. The February 2026 clearance scripts disagree with one another; kept for history only, do not rely on their results.
import math
def check_clearance(A_starts_first, rot_dir, stagger_deg):
    W = 4.2536 # Face width
    # 2 * R * sin(120/2) = W => R = W / (2 * sin(60))
    R = W / (2 * math.sin(math.radians(60)))  # about 2.4558
    D = 4.45 # Distance between shafts
    
    min_clearance = 999
    
    for t in range(0, 120 + stagger_deg + 1):
        if A_starts_first:
            thetaA = min(max(t, 0), 120) * rot_dir
            thetaB = min(max(t - stagger_deg, 0), 120) * rot_dir
        else:
            thetaB = min(max(t, 0), 120) * rot_dir
            thetaA = min(max(t - stagger_deg, 0), 120) * rot_dir
            
        vA = [(R * math.cos(math.radians(90 + thetaA))),
              (R * math.cos(math.radians(210 + thetaA))),
              (R * math.cos(math.radians(330 + thetaA)))]
        xA = max(vA)
        
        vB = [(D + R * math.cos(math.radians(90 + thetaB))),
              (D + R * math.cos(math.radians(210 + thetaB))),
              (D + R * math.cos(math.radians(330 + thetaB)))]
        xB = min(vB)
        
        clearance = xB - xA
        if clearance < min_clearance:
            min_clearance = clearance
            
    return min_clearance

print("Clearance Matrix (inches, W=4.2536, D=4.45): positive means safe, negative means collision")
print(f"Stagger\tA (left) starts CCW\tB (right) starts CCW\tA starts CW\tB starts CW")
# Test a large range of staggers to find the safe zones
for stagger in range(0, 121, 5):
    l_ccw = check_clearance(True, 1, stagger)
    r_ccw = check_clearance(False, 1, stagger)
    l_cw = check_clearance(True, -1, stagger)
    r_cw = check_clearance(False, -1, stagger)
    print(f"{stagger} deg\t{l_ccw:.3f}\t\t\t{r_ccw:.3f}\t\t\t{l_cw:.3f}\t\t{r_cw:.3f}")


# RETIRED: superseded by prism_clearance.py. The February 2026 clearance scripts disagree with one another; kept for history only, do not rely on their results.
import math
def check_clearance_trace(cascade_dir, rot_dir, stagger_deg):
    R = 2.45
    D = 4.333
    
    for t in range(0, 120 + stagger_deg + 1):
        if cascade_dir == 1:
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
        
        if xB - xA < 0:
            print(f"Collision at t={t}: thetaA={thetaA}, thetaB={thetaB}, xA={xA:.3f}, xB={xB:.3f}")
            return

print("Trace for L->R CCW with 20 deg stagger:")
check_clearance_trace(1, 1, 20)
print("Trace for R->L CCW with 20 deg stagger:")
check_clearance_trace(-1, 1, 20)


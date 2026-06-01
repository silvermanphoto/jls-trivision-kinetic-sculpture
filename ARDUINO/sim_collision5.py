import math

def check_clearance(A_starts_first, rot_dir, stagger_deg):
    W = 4.2536 # Face width
    R = W / (2 * math.sin(math.radians(60)))  # about 2.4558
    D = 4.45 # Distance between shafts
    
    min_clearance = 999
    
    # We are rotating 120 degrees at 0.5 RPM.
    # 0.5 RPM = 180 degrees per minute = 3 degrees per second.
    # We will simulate the movement in 1-degree steps.
    for t in range(0, 120 + stagger_deg + 1):
        if A_starts_first:
            thetaA = min(max(t, 0), 120) * rot_dir
            thetaB = min(max(t - stagger_deg, 0), 120) * rot_dir
        else:
            thetaB = min(max(t, 0), 120) * rot_dir
            thetaA = min(max(t - stagger_deg, 0), 120) * rot_dir
            
        # The prisms are initially oriented such that a flat face is parallel to the front.
        # So vertices are at 90, 210, 330 degrees (where 0 is +X, 90 is +Y)
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

print("BACKWARDS (CCW) ROTATION SIMULATION")
print("Stagger\tL->R (A starts)\tR->L (B starts)")
for stagger in range(0, 121, 5):
    lr = check_clearance(True, 1, stagger)
    rl = check_clearance(False, 1, stagger)
    print(f"{stagger} deg\t{lr:.3f}\t\t{rl:.3f}")


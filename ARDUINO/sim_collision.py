import math
def check_clearance(cascade_dir, rot_dir, stagger_deg):
    # cascade_dir: +1 for L to R (A starts before B), -1 for R to L (B starts before A)
    # rot_dir: +1 for CCW, -1 for CW
    R = 2.45
    D = 4.333
    min_clearance = 999
    # Simulate time from when the first prism starts to when the last finishes
    # Prisms rotate 120 degrees.
    # A is at X=0, B is at X=D
    for t in range(0, 120 + stagger_deg + 1):
        if cascade_dir == 1:
            # A starts at t=0, B starts at t=stagger
            thetaA = min(max(t, 0), 120) * rot_dir
            thetaB = min(max(t - stagger_deg, 0), 120) * rot_dir
        else:
            # B starts at t=0, A starts at t=stagger
            thetaB = min(max(t, 0), 120) * rot_dir
            thetaA = min(max(t - stagger_deg, 0), 120) * rot_dir
        
        # Vertices of A
        vA = [(R * math.cos(math.radians(90 + thetaA))),
              (R * math.cos(math.radians(210 + thetaA))),
              (R * math.cos(math.radians(330 + thetaA)))]
        xA = max(vA)
        
        # Vertices of B
        vB = [(D + R * math.cos(math.radians(90 + thetaB))),
              (D + R * math.cos(math.radians(210 + thetaB))),
              (D + R * math.cos(math.radians(330 + thetaB)))]
        xB = min(vB)
        
        clearance = xB - xA
        if clearance < min_clearance:
            min_clearance = clearance
            
    return min_clearance

print("Clearance Matrix (inches): positive means safe, negative means collision")
print(f"Angle\tL->R CCW\tR->L CCW\tL->R CW\t\tR->L CW")
for stagger in [0, 5, 10, 15, 20, 25, 30, 40, 60, 90, 120]:
    lr_ccw = check_clearance(1, 1, stagger)
    rl_ccw = check_clearance(-1, 1, stagger)
    lr_cw = check_clearance(1, -1, stagger)
    rl_cw = check_clearance(-1, -1, stagger)
    print(f"{stagger} deg\t{lr_ccw:.3f}\t\t{rl_ccw:.3f}\t\t{lr_cw:.3f}\t\t{rl_cw:.3f}")

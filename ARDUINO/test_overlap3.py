# RETIRED: superseded by prism_clearance.py. The February 2026 clearance scripts disagree with one another; kept for history only, do not rely on their results.
import math

def check_clearance(A_starts_first, rot_dir, stagger_deg):
    W = 4.2536 # Face width
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
            
        # The prisms are initially oriented such that a FLAT FACE is flush with the front.
        # This means the vertices are at 90, 210, 330 degrees relative to front (0 is right, 90 is front)
        vA_x = [R * math.cos(math.radians(90 + thetaA)),
                R * math.cos(math.radians(210 + thetaA)),
                R * math.cos(math.radians(330 + thetaA))]
        
        vB_x = [D + R * math.cos(math.radians(90 + thetaB)),
                D + R * math.cos(math.radians(210 + thetaB)),
                D + R * math.cos(math.radians(330 + thetaB))]
                
        # The collision happens when max(vA_x) > min(vB_x) OR if an edge intersects.
        # For equilateral triangles on the same Y axis, just checking the bounding box X overlap 
        # is a safe lower bound (if max X of A > min X of B, they MIGHT collide, but not always).
        # We need actual edge-edge intersection, but bounding box is a safe conservative check.
        
        # Actually, let's just find the minimum distance between the two polygons.
        # A simple check: if the distance between any vertex of A and the center of B is less than R...
        # Let's just output the conservative bounding box clearance:
        clearance = min(vB_x) - max(vA_x)
        if clearance < min_clearance:
            min_clearance = clearance
            
    return min_clearance

print("Bounding Box Clearance Matrix (inches)")
for stagger in range(0, 121, 5):
    lr = check_clearance(True, 1, stagger) # L->R CCW
    rl = check_clearance(False, 1, stagger) # R->L CCW
    print(f"{stagger} deg\t{lr:.3f}\t\t{rl:.3f}")


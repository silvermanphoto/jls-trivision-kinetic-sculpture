# RETIRED: superseded by prism_clearance.py. The February 2026 clearance scripts disagree with one another; kept for history only, do not rely on their results.
import math

def get_vertices(theta, offset_x):
    W = 4.2536
    R = W / (2 * math.sin(math.radians(60)))
    v = []
    # Using specific 90, 210, 330 from manual
    for angle in [90, 210, 330]:
        rad = math.radians(angle + theta)
        x = offset_x + R * math.cos(rad)
        y = R * math.sin(rad)
        v.append((x, y))
    return v

def ccw(A, B, C):
    return (C[1]-A[1]) * (B[0]-A[0]) > (B[1]-A[1]) * (C[0]-A[0])

# Return true if line segments AB and CD intersect
def intersect(A, B, C, D):
    return ccw(A,C,D) != ccw(B,C,D) and ccw(A,B,C) != ccw(A,B,D)

def polys_intersect(polyA, polyB):
    for i in range(3):
        a1 = polyA[i]
        a2 = polyA[(i+1)%3]
        for j in range(3):
            b1 = polyB[j]
            b2 = polyB[(j+1)%3]
            if intersect(a1, a2, b1, b2):
                return True
    return False

def check_stagger(stagger_deg, rot_dir):
    # Simulate step-by-step
    for t in range(0, 120 + stagger_deg + 1):
        thetaA = min(max(t, 0), 120) * rot_dir
        thetaB = min(max(t - stagger_deg, 0), 120) * rot_dir
        
        polyA = get_vertices(thetaA, 0)
        polyB = get_vertices(thetaB, 4.45)
        
        if polys_intersect(polyA, polyB):
            return True, t
            
    return False, -1

# Let's test standard 1-2-3 backward rotation
print("Testing CCW Staggers with precise Line-Intersection...")
for s in range(0, 121, 5):
    hit, time = check_stagger(s, -1) # wait, backwards CCW means positive theta or negative?
    # The manual stated: rotate BACKWARD. In standard math, CCW is positive rotation (+deg).
    # Wait, my previous code used +1 for CCW.
    # Let me test +1
    pass

for s in range(0, 121, 5):
    hit, time = check_stagger(s, 1) # ccw = +
    print(f"CCW Stagger {s} deg:\t{'COLLISION at t='+str(time) if hit else 'SAFE'}")


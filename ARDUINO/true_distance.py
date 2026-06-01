import math
import cmath

def get_vertices(theta, offset_x):
    W = 4.2536
    R = W / (2 * math.sin(math.radians(60)))
    v = []
    # 90 degrees is "front" face
    for angle in [90, 210, 330]:
        rad = math.radians(angle + theta)
        x = offset_x + R * math.cos(rad)
        y = R * math.sin(rad)
        v.append(complex(x, y))
    return v

def dist_point_segment(p, a, b):
    ab = b - a
    ap = p - a
    
    if ab == 0:
        return abs(ap)
        
    t = (ap.real * ab.real + ap.imag * ab.imag) / (ab.real**2 + ab.imag**2)
    t = max(0, min(1, t))
    
    proj = a + t * ab
    return abs(p - proj)

def poly_dist(vA, vB):
    min_d = 999
    # A points to B edges
    for i in range(3):
        pA = vA[i]
        for j in range(3):
            eB1 = vB[j]
            eB2 = vB[(j+1)%3]
            d = dist_point_segment(pA, eB1, eB2)
            if d < min_d: min_d = d
            
    # B points to A edges
    for i in range(3):
        pB = vB[i]
        for j in range(3):
            eA1 = vA[j]
            eA2 = vA[(j+1)%3]
            d = dist_point_segment(pB, eA1, eA2)
            if d < min_d: min_d = d
            
    return min_d

print("True Minimum Distance (inches)")

for stagger in range(0, 121, 5):
    safe_min = 999
    for t in range(0, 120 + stagger + 1):
        # A runs first
        thetaA = min(max(t, 0), 120) * 1  # CCW
        thetaB = min(max(t - stagger, 0), 120) * 1 # CCW
        
        vA = get_vertices(thetaA, 0)
        vB = get_vertices(thetaB, 4.45)
        
        d = poly_dist(vA, vB)
        if d < safe_min: safe_min = d
        
    print(f"Stagger {stagger:3}:\t{safe_min:.3f}")


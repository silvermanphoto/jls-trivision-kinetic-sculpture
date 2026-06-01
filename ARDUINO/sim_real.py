import math

def get_vertices(theta, offset_x):
    W = 4.2536
    R = W / (2 * math.sin(math.radians(60)))
    # For a flat face parallel to the front, the vertices are at 90, 210, 330 degrees (where 0 is +X, 90 is +Y)
    v = []
    for angle in [90, 210, 330]:
        rad = math.radians(angle + theta)
        x = offset_x + R * math.cos(rad)
        y = R * math.sin(rad)
        v.append((x, y))
    return v

def distance(p1, p2):
    return math.hypot(p1[0]-p2[0], p1[1]-p2[1])

def point_to_segment_dist(p, a, b):
    # vector ab
    ab = (b[0]-a[0], b[1]-a[1])
    # vector ap
    ap = (p[0]-a[0], p[1]-a[1])
    # dot prod
    dot_ap_ab = ap[0]*ab[0] + ap[1]*ab[1]
    ab_len_sq = ab[0]**2 + ab[1]**2
    
    if ab_len_sq == 0:
        return distance(p, a)
        
    t = max(0, min(1, dot_ap_ab / ab_len_sq))
    
    # projection point
    proj = (a[0] + t * ab[0], a[1] + t * ab[1])
    return distance(p, proj)

def exact_clearance(thetaA, thetaB):
    D = 4.45
    vA = get_vertices(thetaA, 0)
    vB = get_vertices(thetaB, D)
    
    min_dist = 999
    # Check all vertices of A against all edges of B
    for p in vA:
        for i in range(3):
            d = point_to_segment_dist(p, vB[i], vB[(i+1)%3])
            if d < min_dist: min_dist = d
            
    # Check all vertices of B against all edges of A
    for p in vB:
        for i in range(3):
            d = point_to_segment_dist(p, vA[i], vA[(i+1)%3])
            if d < min_dist: min_dist = d
            
    return min_dist

def check_sequence(stagger_deg):
    min_dist = 999
    for t in range(0, 120 + stagger_deg + 1):
        thetaA = min(max(t, 0), 120) * 1  # CCW
        thetaB = min(max(t - stagger_deg, 0), 120) * 1 # CCW
        
        dist = exact_clearance(thetaA, thetaB)
        if dist < min_dist:
            min_dist = dist
            
    return min_dist

print("Exact Polygon Clearance (inches)")
for s in range(0, 121, 5):
    d = check_sequence(s)
    print(f"Stagger {s} deg:\t{d:.3f}")


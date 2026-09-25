# RETIRED: superseded by prism_clearance.py. The February 2026 clearance scripts disagree with one another; kept for history only, do not rely on their results.
import math

def get_vertices(theta, offset_x):
    W = 4.2536
    R = W / (2 * math.sin(math.radians(60)))
    v = []
    # 90 degrees is "front" face
    for angle in [90, 210, 330]:
        rad = math.radians(angle + theta)
        x = offset_x + R * math.cos(rad)
        y = R * math.sin(rad)
        v.append((x, y))
    return v

# Standard ray casting algorithm for point in polygon
def point_in_poly(x, y, poly):
    n = len(poly)
    inside = False
    p1x, p1y = poly[0]
    for i in range(n+1):
        p2x, p2y = poly[i % n]
        if y > min(p1y, p2y):
            if y <= max(p1y, p2y):
                if x <= max(p1x, p2x):
                    if p1y != p2y:
                        xints = (y-p1y)*(p2x-p1x)/(p2y-p1y)+p1x
                    if p1x == p2x or x <= xints:
                        inside = not inside
        p1x, p1y = p2x, p2y
    return inside

def check_collision(stagger_deg, rot_dir):
    # check from t=0 to 120 + stagger
    for t in range(0, 120 + stagger_deg + 1):
        thetaA = min(max(t, 0), 120) * rot_dir
        thetaB = min(max(t - stagger_deg, 0), 120) * rot_dir
        
        polyA = get_vertices(thetaA, 0)
        polyB = get_vertices(thetaB, 4.45)
        
        # Check if any vertex of A is inside B
        for (x,y) in polyA:
            if point_in_poly(x, y, polyB):
                return True # COLLISION
                
        # Check if any vertex of B is inside A
        for (x,y) in polyB:
            if point_in_poly(x, y, polyA):
                return True # COLLISION
                
        # (Technically polygons can overlap without vertices inside each other, 
        # but for equilateral triangles of the same size, vertex penetration covers almost all cases)
            
    return False

print("Testing Staggers for True 1-12 Waterfall (CCW rotation):")
safe_staggers = []
for s in range(0, 121):
    collision = check_collision(s, 1) # 1 = CCW
    if not collision:
        safe_staggers.append(s)

print(f"Safe staggers: {safe_staggers}")

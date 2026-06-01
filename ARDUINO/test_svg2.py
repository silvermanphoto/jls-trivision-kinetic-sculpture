import math

def get_polygon_svg(theta, offset_x):
    W = 4.2536
    R = W / (2 * math.sin(math.radians(60)))
    v = []
    # Initial state: flat face is front (vertical on Y axis in our simplified 2D)
    # The triangle vertices relative to center:
    for angle in [90, 210, 330]:
        rad = math.radians(angle + theta)
        x = offset_x + R * math.cos(rad)
        y = R * math.sin(rad)
        v.append((x, y))
        
    return f'<polygon points="{v[0][0]},{v[0][1]} {v[1][0]},{v[1][1]} {v[2][0]},{v[2][1]}" fill="none" stroke="black"/>'

def test_stagger(stagger):
    D = 4.45
    collision = False
    
    # We test every 1 degree of rotation
    for t in range(0, 120 + stagger + 1):
        thetaA = min(max(t, 0), 120) * 1
        thetaB = min(max(t - stagger, 0), 120) * 1
        
        # In a real overlap test, we check if they intersect.
        # But we can also just output the bounding box clearance.
        # Let's print the max X of A and min X of B
        W = 4.2536
        R = W / (2 * math.sin(math.radians(60)))
        vA_x = [R * math.cos(math.radians(angle + thetaA)) for angle in [90, 210, 330]]
        vB_x = [D + R * math.cos(math.radians(angle + thetaB)) for angle in [90, 210, 330]]
        
        # Bounding box clearance:
        bb_clearance = min(vB_x) - max(vA_x)
        if bb_clearance < 0:
            collision = True
            break
            
    return not collision # True if safe

print("Testing physical safe staggers...")
for stagger in range(0, 121, 5):
    safe = test_stagger(stagger)
    print(f"Stagger {stagger}:\t{'SAFE (BB)' if safe else 'COLLISION (BB)'}")


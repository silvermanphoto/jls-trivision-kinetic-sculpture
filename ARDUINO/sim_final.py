import math

def get_vertices(theta, offset_x):
    W = 4.2536
    R = W / (2 * math.sin(math.radians(60)))
    v = []
    # If the flat face is physically in front, the vertices are at 90, 210, 330
    for angle in [90, 210, 330]:
        rad = math.radians(angle + theta)
        x = offset_x + R * math.cos(rad)
        y = R * math.sin(rad)
        v.append((x, y))
    return v

def ccw(A, B, C):
    return (C[1]-A[1]) * (B[0]-A[0]) > (B[1]-A[1]) * (C[0]-A[0])

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

# What if BACKWARDS means CW physically?
print("Let's test CW staggers...")
for s in range(0, 121, 5):
    hit = False
    for t in range(0, 120 + s + 1):
        # A goes from 0 to -120 (CW)
        thetaA = -min(max(t, 0), 120)
        # B goes from 0 to -120, offset by s
        thetaB = -min(max(t - s, 0), 120)
        
        # Are they overlapping?
        polyA = get_vertices(thetaA, 0)
        polyB = get_vertices(thetaB, 4.45)
        
        if polys_intersect(polyA, polyB):
            hit = True
            break
            
    print(f"Stagger {s:3} CW:\t{'CRASH' if hit else 'SAFE'}")


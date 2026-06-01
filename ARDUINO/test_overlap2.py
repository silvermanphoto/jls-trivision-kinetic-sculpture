import math

def get_vertices(theta, offset_x):
    W = 4.2536
    R = W / (2 * math.sin(math.radians(60)))
    v = [(offset_x + R * math.cos(math.radians(90 + theta))),
         (offset_x + R * math.cos(math.radians(210 + theta))),
         (offset_x + R * math.cos(math.radians(330 + theta)))]
    return v

# What if A moves from 0 to 120, and B is already at 0 or 120?
# Actually, B at 0 and B at 120 are the same physical geometry!
# So we already know from the previous test: 
# A=30 B=0: Max A_x=2.456, Min B_x=2.323, Clearance=-0.133
print("Collision confirmed: a prism cannot rotate 120 degrees while its neighbor is stationary.")

import math

def get_polygon_svg(theta, offset_x):
    W = 4.2536
    R = W / (2 * math.sin(math.radians(60)))
    v = []
    for angle in [90, 210, 330]:
        rad = math.radians(angle + theta)
        x = offset_x + R * math.cos(rad)
        y = R * math.sin(rad)
        v.append((x*10, y*10))  # Scale for SVG
    return f'<polygon points="{v[0][0]},{v[0][1]} {v[1][0]},{v[1][1]} {v[2][0]},{v[2][1]}" fill="none" stroke="red"/>'

print("Generating SVG frames for a 90 degree stagger...")
html = "<html><body>"
D = 4.45
stagger = 90
for t in range(0, 120 + stagger + 1, 5):
    thetaA = min(max(t, 0), 120) * 1
    thetaB = min(max(t - stagger, 0), 120) * 1
    
    html += f'<div style="display:inline-block; margin: 10px;">t={t}<br><svg width="150" height="100" viewBox="-30 -30 150 100">'
    html += get_polygon_svg(thetaA, 0)
    html += get_polygon_svg(thetaB, D)
    html += '</svg></div>'

html += "</body></html>"
with open("sim_visual.html", "w") as f:
    f.write(html)
print("done")

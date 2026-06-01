print("Testing microstepping counts...")
# 0.9 deg = 400 full steps
print(f"1/8: 400 * 8 = 3200 steps/rev => 120 deg = {3200 * 120 / 360} steps")
print(f"1/16: 400 * 16 = 6400 steps/rev => 120 deg = {6400 * 120 / 360} steps")
print(f"1/64: 400 * 64 = 25600 steps/rev => 120 deg = {25600 * 120 / 360} steps")

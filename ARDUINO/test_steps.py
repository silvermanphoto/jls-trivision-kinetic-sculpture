
# Motor: 17HM19-2004S
# 0.9° step angle = 400 full steps per revolution.
# TMC2209 driver is running in MS1/MS2 floating.
# According to TMC2209 datasheet, floating MS1/MS2 = 1/8 microstepping.
# Wait, let me check the handoff document again.

# 400 steps * 8 = 3200 steps per revolution.
# 3200 / 3 = 1066.666 steps per 120 degrees.
# So exactly 1067 steps.

# Let's verify the step angle from the model spec.
# 17HM19-2004S: "0.9deg"
# Yes, 360 / 0.9 = 400.

# What if the driver default without any MS pins connected is actually 1/16?
# Or what if MS pins on the Adafruit 6121 aren't floating, but pulled to ground or VCC?
# If MS1 and MS2 are tied to GND -> 1/8?
# If they are 1/16, then 3200 is wrong... 400 * 16 = 6400 steps/rev.
# If it's 6400 steps/rev, then 1067 steps is only 1067 / 6400 = 16.6% of a revolution! That's 60 degrees!
print("If it's exactly half a turn off, the driver might be in 1/16.")

# The Arduino sketch takes 40 seconds to complete 120 degrees at 0.5 RPM
# 0.5 RPM = 3 degrees per sec = 120 degrees / 40 sec.
# Our new script should have a stagger >= 40 seconds.
print("To achieve a 120-degree stagger safely, the delay between Prism 1 and Prism 2 must be >= the sweep duration.")

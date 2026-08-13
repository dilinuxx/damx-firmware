"""
PM100D Power Meter Auto-Detect & Measurement Script

This script automatically detects a Thorlabs PM100D USB power meter,
connects to it using PyVISA, checks whether a sensor head is attached,
and reads optical power measurements.

Functionality:
- Scans connected VISA instruments
- Automatically finds the Thorlabs PM100D (USB VID 0x1313 / PID 0x8078)
- Connects and queries device identity
- Detects whether a power sensor head is attached
- Reads power measurements using SCPI commands
- Prevents timeouts if no sensor is present

Requires:
    Python 3.9+
    pyvisa
    NI-VISA or compatible VISA backend
"""
#python -m pip install --no-user pyvisa

import pyvisa
import time

# Initialize VISA resource manager
rm = pyvisa.ResourceManager()

# List connected instruments
resources = rm.list_resources()
print("Connected instruments:", resources)

# Find the PM100D automatically
pm100d_resource = None
for res in resources:
    if '0x1313' in res and '0x8078' in res:
        pm100d_resource = res
        break

if not pm100d_resource:
    raise RuntimeError("No PM100D found.")

print("Connecting to PM100D at:", pm100d_resource)

pm100d = rm.open_resource(pm100d_resource)
pm100d.timeout = 2000  # 2 seconds

# Query device identity
print("Device info:", pm100d.query("*IDN?"))

# -------------------------
# Check sensor
# -------------------------
sensor_info = pm100d.query("SYST:SENS:IDN?")
print("Sensor info:", sensor_info)

if "no sensor" in sensor_info.lower():
    print("⚠ No sensor connected to PM100D.")
    print("Connect a sensor head before reading power.")
    exit()

print("Sensor detected!")

# -------------------------
# Measurement loop
# -------------------------
print("Starting measurements...\n")

try:
    while True:
        power = float(pm100d.query("MEAS:POW?"))
        print(f"Power: {power:.6e} W")
        time.sleep(1)

except KeyboardInterrupt:
    print("Stopped.")
import socket
import re
import time
from datetime import datetime
import matplotlib.pyplot as plt
import pandas as pd

#SERVER = "rishi.shef.ac.uk"
SERVER = "172.20.10.3"
PORT = 51003

MAX_SAFE_CURRENT = 2500      # mA
RAMP_STEP = 200              # mA
RAMP_DELAY = 0.05            # seconds

LASER = 1
RUN_TIME = 1 * 60  # 5 minutes #5 * 60  # 5 minutes

CURRENTS = [2500, 0]
HOLD_TIME = 5  # sampling interval


# -----------------------------
# TIME
# -----------------------------
def timestamp():
    return datetime.now().strftime("%H:%M:%S")


# -----------------------------
# SOCKET CORE
# -----------------------------
def read_exact(sock, n):
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("Socket closed unexpectedly")
        data += chunk
    return data


def read_message(sock):
    header = read_exact(sock, 4).decode()
    length = int(header[1:])
    body = read_exact(sock, length).decode()
    return body


def send_command(sock, command, expect_multiple=False):
    message = f"#{len(command):03d}{command}"

    print(f"[{timestamp()}] TX -> {command}")

    sock.sendall(message.encode())

    responses = []

    while True:
        response = read_message(sock)
        responses.append(response)

        print(f"[{timestamp()}] RX <- {response}")

        if expect_multiple:
            if response == "DONE":
                break
        else:
            break

    return responses


# -----------------------------
# DEVICE DISCOVERY
# -----------------------------
def get_devices(sock, connected_only=False):

    responses = send_command(sock, "HOWCON", True)

    devices = []

    pattern = r"Device (\d+) .* - (CON|NOT)"

    for line in responses:
        m = re.match(pattern, line)

        if not m:
            continue

        device, state = m.groups()

        if connected_only and state != "CON":
            continue

        devices.append(device)

    return devices

# -----------------------------
# GLOBAL LOG
# -----------------------------
log = []

def set_current(sock, device, laser, current, cycle):

    # Safety limits
    current = max(0, min(int(current), MAX_SAFE_CURRENT))

    command = (
        f"SETCURR"
        f"{int(device):04d}"
        f"{int(laser):02d}"
        f"{current:04d}"
    )

    start = time.time()

    response = send_command(sock, command)[0]
    latency = time.time() - start

    success = (response == "OK")

    log.append({
        "time": time.time(),
        "cycle": cycle,
        "device": device,
        "current": current,
        "response": response,
        "success": success,
        "latency": latency
    })

    status = "OK" if success else "FAIL"

    print(
        f"[CYCLE {cycle:03d}] "
        f"DEV {device} | "
        f"I={current} mA | "
        f"{status} | "
        f"{response} | "
        f"{latency:.3f}s"
    )

    return success

def ramp_current(sock, device, laser,
                 start_current,
                 target_current,
                 cycle,
                 step=RAMP_STEP,
                 delay=RAMP_DELAY):

    start_current = max(0, min(start_current, MAX_SAFE_CURRENT))
    target_current = max(0, min(target_current, MAX_SAFE_CURRENT))

    if start_current == target_current:
        return

    if start_current < target_current:
        currents = range(start_current,
                         target_current,
                         step)
    else:
        currents = range(start_current,
                         target_current,
                         -step)

    for current in currents:
        set_current(sock, device, laser, current, cycle)
        time.sleep(delay)

    set_current(sock, device, laser, target_current, cycle)

# -----------------------------
# SAFE SHUTDOWN (FIX)
# -----------------------------
def safe_shutdown(sock, devices, laser, cycle):
    print("\n==============================")
    print("SAFETY SHUTDOWN")
    print("==============================")

    for device in devices:

        try:
            ramp_current(
                sock,
                device,
                laser,
                current_state.get(device, 0),
                0,
                cycle
            )

        except Exception:
            pass

    try:
        send_command(sock, "DISALL", True)
    except Exception:
        pass

    try:
        send_command(sock, "BYE")
    except Exception:
        pass


# -----------------------------
# PLOTTING
# -----------------------------
def plot_results(df):

    df["relative_time"] = df["time"] - df["time"].min()

    window = 10
    df["success_rate"] = df["success"].rolling(window).mean()

    plt.figure()
    plt.plot(df["relative_time"], df["success_rate"])
    plt.title("BLE Success Rate (Rolling Window)")
    plt.xlabel("Time (s)")
    plt.ylabel("Success Rate")
    plt.ylim(0, 1)
    plt.savefig("1_success_rate.png", dpi=300)

    plt.figure()
    plt.plot(df["relative_time"], df["latency"])
    plt.title("BLE Latency Over Time")
    plt.xlabel("Time (s)")
    plt.ylabel("Latency (s)")
    plt.savefig("2_latency.png", dpi=300)

    device_stats = df.groupby("device")["success"].mean()

    plt.figure()
    device_stats.plot(kind="bar")
    plt.title("Per-Device Success Rate")
    plt.ylabel("Success Rate")
    plt.ylim(0, 1)
    plt.savefig("3_device_success.png", dpi=300)

    plt.figure()
    df["response"].value_counts().plot(kind="bar")
    plt.title("Response Breakdown (OK / ERR)")
    plt.ylabel("Count")
    plt.savefig("4_response_breakdown.png", dpi=300)

    plt.close('all')


# -----------------------------
# MAIN
# -----------------------------
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:

    print(f"Connecting to {SERVER}:{PORT}")
    s.connect((SERVER, PORT))

    print("\n[HELLO]")
    print(s.recv(1024).decode())

    send_command(s, "PING")

    print("\n==============================")
    print("Connecting to all devices...")
    print("==============================")

    conall_response = send_command(s, "CONALL", True)

    print("\nRaw CONALL response:")
    for line in conall_response:
        print(f"  {line}")

    # Give the system time to finish updating connection states
    print("\n[INFO] Waiting 5 seconds before querying HOWCON...")
    time.sleep(5)

    #devices = get_devices(s);
    devices = get_devices(s, connected_only=True);

    print("\nParsed connected devices:")
    for d in devices:
        print(f"  -> Device {d}")

    print(f"\n[INFO] Total connected devices: {len(devices)}")

    if not devices:
        print("No devices connected. Exiting.")
        exit()

    current_state = {}
    for device in devices:
        current_state[device] = 0

    start_time = time.time()
    cycle = 0

    try:

        while time.time() - start_time < RUN_TIME:

            cycle += 1

            print(f"\n==============================")
            print(f"START CYCLE {cycle}")
            print(f"==============================")

            for target in CURRENTS:

                for device in devices:
                    ramp_current(
                        s,
                        device,
                        LASER,
                        current_state[device],
                        target,
                        cycle
                    )

                    current_state[device] = target

                time.sleep(HOLD_TIME)

    finally:

        # ALWAYS SAFE SHUTDOWN
        safe_shutdown(s, devices, LASER, cycle)


# -----------------------------
# POST PROCESSING (ALWAYS RUNS)
# -----------------------------
df = pd.DataFrame(log)

print("\nGenerating plots...")

plot_results(df)   # <-- REQUIRED (this creates the figures)

plt.savefig("ble_results.png", dpi=300)

print("Plots saved to ble_results.png")
print("Test complete.")
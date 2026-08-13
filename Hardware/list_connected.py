import socket

SERVER = "rishi.shef.ac.uk"
PORT = 51003


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


def send_command(sock, command, expect_multiple=False, allow_disconnect=False):
    message = f"#{len(command):03d}{command}"
    try:
        sock.sendall(message.encode())
    except OSError:
        print("Socket already closed.")
        return None

    responses = []

    while True:
        try:
            response = read_message(sock)
        except ConnectionError:
            if allow_disconnect and command == "BYE":
                print("[INFO] Socket closed after BYE (expected).")
                break
            else:
                raise
        responses.append(response)
        print(f"[RX] {response}")

        if not expect_multiple or response in ("DONE", "OK", "ERR"):
            break

    return responses

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    print(f"Connecting to {SERVER}:{PORT}...")
    s.connect((SERVER, PORT))

    hello = s.recv(1024).decode()
    print(f"[HELLO] {hello}")

    send_command(s, "PING")

    print("\nConnecting to ALL devices (this may take time)...")
    send_command(s, "CONALL", expect_multiple=True)

    print("\nConnection status after CONALL:")
    send_command(s, "HOWCON", expect_multiple=True)

    send_command(s, "BYE", allow_disconnect=True)

print("Finished.")
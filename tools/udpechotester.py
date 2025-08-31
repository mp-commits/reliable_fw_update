import socket
import sys
import time

def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <min_size> <max_size> <step>")
        sys.exit(1)

    min_size = int(sys.argv[1])
    max_size = int(sys.argv[2])
    step = int(sys.argv[3])

    server_ip = '192.168.1.50'
    server_port = 7
    timeout = 2  # seconds

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(timeout)

        for size in range(min_size, max_size + 1, step):
            message = bytes([x % 256 for x in range(size)])
            try:
                sock.sendto(message, (server_ip, server_port))
                data, _ = sock.recvfrom(2048)

                if data == message:
                    print(f"✓ {size} bytes echoed correctly.")
                else:
                    print(f"✗ {size} bytes: Echo mismatch.")
            except socket.timeout:
                print(f"✗ {size} bytes: No response (timeout).")
            except Exception as e:
                print(f"✗ {size} bytes: Error - {e}")

if __name__ == "__main__":
    main()


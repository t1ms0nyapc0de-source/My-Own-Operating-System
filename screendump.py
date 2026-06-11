import subprocess
import time
import socket
import sys

def main():
    print("Starting QEMU...")
    proc = subprocess.Popen([
        "qemu-system-i386",
        "-cdrom", "myos.iso",
        "-monitor", "telnet:127.0.0.1:4444,server,nowait",
        "-display", "none"
    ])

    time.sleep(3) # Wait 3 seconds for the kernel to boot and run

    print("Connecting to QEMU monitor...")
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(("127.0.0.1", 4444))
        time.sleep(0.5)
        s.recv(4096)
        
        print("Sending screendump command...")
        s.sendall(b"screendump /root/kernel/screen.ppm\n")
        time.sleep(0.5)
        
        print("Sending quit command...")
        s.sendall(b"quit\n")
        time.sleep(0.5)
        s.close()
    except Exception as e:
        print("Error communicating with monitor:", e)
        proc.terminate()
        sys.exit(1)

    proc.wait()
    print("QEMU exited.")

if __name__ == "__main__":
    main()

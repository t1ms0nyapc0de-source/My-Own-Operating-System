import subprocess
import time
import socket
import sys
import os

def run_qemu_and_save():
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
        print("Welcome:", s.recv(4096).decode('utf-8', errors='ignore'))
        
        print("Saving VGA memory...")
        # pmemsave <addr> <size> <file>
        s.sendall(b'pmemsave 0xb8000 4000 "/root/kernel/vga.bin"\n')
        time.sleep(0.5)
        print("pmemsave response:", s.recv(4096).decode('utf-8', errors='ignore'))
        
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

def decode_vga():
    if not os.path.exists("vga.bin"):
        print("vga.bin not found!")
        return

    print("--- VGA Screen Dump ---")
    with open("vga.bin", "rb") as f:
        data = f.read()

    for row in range(25):
        line = []
        for col in range(80):
            idx = (row * 80 + col) * 2
            if idx < len(data):
                char_code = data[idx]
                # VGA characters are standard ASCII or extended ASCII CP437.
                # Convert non-printable/empty characters to spaces or printable characters.
                if 32 <= char_code <= 126:
                    line.append(chr(char_code))
                elif char_code == 0:
                    line.append(' ')
                else:
                    line.append('?')
        print("".join(line).rstrip())
    print("-----------------------")

if __name__ == "__main__":
    run_qemu_and_save()
    decode_vga()

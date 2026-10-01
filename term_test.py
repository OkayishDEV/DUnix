import subprocess, time, os, socket, select

cmd = [
    "qemu-system-x86_64",
    "-kernel", "build/dunix.bin",
    "-serial", "stdio",
    "-monitor", "unix:/tmp/qemu-mon.sock,server,nowait",
    "-display", "none",
    "-nic", "model=e1000"
]

proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
os.set_blocking(proc.stdout.fileno(), False)

output = b""
start = time.time()
while time.time() - start < 3:
    r, _, _ = select.select([proc.stdout], [], [], 0.1)
    if r:
        chunk = proc.stdout.read(4096)
        if chunk:
            output += chunk
            if b"dunix:/#" in output:
                break

print("Shell prompt reached!")

# Connect to monitor
time.sleep(0.5)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect("/tmp/qemu-mon.sock")
time.sleep(0.2)
s.recv(1024)

# Launch dsession
print("Launching dsession...")
proc.stdin.write(b"/bin/dsession &\n")
proc.stdin.flush()
time.sleep(2.0)

# In QEMU, when running with -serial stdio:
# What happens when we type into proc.stdin?
# Or what happens when we use monitor sendkey?
print("Sending keys via proc.stdin...")
proc.stdin.write(b"ls\n")
proc.stdin.flush()
time.sleep(1.0)

print("Sending keys via monitor sendkey...")
for k in ["l", "s", "ret"]:
    s.sendall(f"sendkey {k}\n".encode())
    time.sleep(0.1)

time.sleep(1.5)

while True:
    r, _, _ = select.select([proc.stdout], [], [], 0.3)
    if r:
        chunk = proc.stdout.read(4096)
        if not chunk: break
        output += chunk
    else:
        break

proc.terminate()
s.close()

with open("term_test.log", "wb") as f:
    f.write(output)

print("Test complete. Log size:", len(output))

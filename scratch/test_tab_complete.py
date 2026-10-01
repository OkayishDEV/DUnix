#!/usr/bin/env python3
import subprocess
import time
import os
import sys
import fcntl

def set_nonblocking(fd):
    flags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

def test_interactive():
    print("="*70)
    print("TESTING INTERACTIVE SHELL: TAB AUTOCOMPLETION & ARROW HISTORY")
    print("="*70)

    cmd = [
        'qemu-system-x86_64',
        '-kernel', 'build/dunix.bin',
        '-chardev', 'stdio,id=char0,mux=off',
        '-serial', 'chardev:char0',
        '-display', 'none'
    ]

    p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0)
    set_nonblocking(p.stdout.fileno())

    full_output = b""
    start_time = time.time()

    print("[*] Waiting for DUnix live shell prompt...")
    while time.time() - start_time < 8.0:
        try:
            chunk = p.stdout.read(1024)
            if chunk:
                full_output += chunk
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
                if b"dunix:/#" in full_output:
                    break
        except BlockingIOError:
            pass
        time.sleep(0.05)

    time.sleep(0.3)

    # Test 1: Tab completion of command 'upt' -> 'uptime '
    print("\n[*] Testing Tab autocompletion: typing 'upt' + TAB...")
    p.stdin.write(b"upt\t\n")
    p.stdin.flush()

    out = b""
    t0 = time.time()
    while time.time() - t0 < 2.0:
        try:
            chunk = p.stdout.read(1024)
            if chunk:
                out += chunk
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
                if b"load average:" in out or b"up " in out:
                    break
        except BlockingIOError:
            pass
        time.sleep(0.05)

    tab_ok = (b"load average:" in out or b"up " in out)
    if tab_ok:
        print("[+] TAB AUTOCOMPLETION PASSED: 'upt' successfully expanded to 'uptime' and executed!")
    else:
        print("[-] TAB AUTOCOMPLETION FAILED!")

    # Test 2: History recall with UP arrow
    # We ran 'uptime'. Now press Up arrow + Enter -> should rerun 'uptime'!
    print("\n[*] Testing History recall: sending UP arrow (\x1b[A) + Enter...")
    out = b""
    p.stdin.write(b"\x1b[A\n")
    p.stdin.flush()

    t0 = time.time()
    while time.time() - t0 < 2.0:
        try:
            chunk = p.stdout.read(1024)
            if chunk:
                out += chunk
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
                if b"load average:" in out or b"up " in out:
                    break
        except BlockingIOError:
            pass
        time.sleep(0.05)

    hist_ok = (b"load average:" in out or b"up " in out)
    if hist_ok:
        print("[+] HISTORY RECALL PASSED: UP arrow successfully recalled and executed previous command!")
    else:
        print("[-] HISTORY RECALL FAILED!")

    p.stdin.write(b"poweroff\n")
    p.stdin.flush()
    time.sleep(0.5)

    try:
        p.terminate()
        p.communicate(timeout=1)
    except Exception:
        pass

    return tab_ok and hist_ok

if __name__ == "__main__":
    if not test_interactive():
        sys.exit(1)

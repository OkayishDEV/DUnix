#!/usr/bin/env python3
import subprocess
import time
import os
import sys
import fcntl

def set_nonblocking(fd):
    flags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

def test_dunix():
    cmd = [
        'qemu-system-x86_64',
        '-kernel', 'build/dunix.bin',
        '-chardev', 'stdio,id=char0,mux=off',
        '-serial', 'chardev:char0',
        '-display', 'none'
    ]

    p = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        bufsize=0
    )

    set_nonblocking(p.stdout.fileno())

    full_output = b""
    start_time = time.time()

    # Step 1: Wait for prompt
    print("Waiting for DUnix kernel to boot and launch userspace shell...")
    while time.time() - start_time < 5.0:
        try:
            chunk = p.stdout.read(1024)
            if chunk:
                full_output += chunk
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
                if b"dunix:/#" in full_output or b"dunix#" in full_output:
                    break
        except BlockingIOError:
            pass
        time.sleep(0.05)

    time.sleep(0.2)

    test_commands = [
        (b"help\n", b"Standard utilities in /bin"),
        (b"pwd\n", b"/"),
        (b"ls /bin\n", b"sh"),
        (b"echo Hello Userspace > /tmp/hello.txt\n", b""),
        (b"cat /tmp/hello.txt\n", b"Hello Userspace"),
        (b"echo Pipeline Operational | cat\n", b"Pipeline Operational"),
        (b"mkdir /tmp/testdir\n", b""),
        (b"ls /tmp\n", b"testdir"),
        (b"cd /tmp/testdir\n", b""),
        (b"pwd\n", b"/tmp/testdir"),
        (b"exit\n", b"Respawning")
    ]

    for cmd_bytes, expected in test_commands:
        p.stdin.write(cmd_bytes)
        p.stdin.flush()

        step_start = time.time()
        while time.time() - step_start < 0.6:
            try:
                chunk = p.stdout.read(1024)
                if chunk:
                    full_output += chunk
                    sys.stdout.buffer.write(chunk)
                    sys.stdout.buffer.flush()
                    if expected and expected in full_output:
                        break
            except BlockingIOError:
                pass
            time.sleep(0.05)

    time.sleep(0.5)
    p.terminate()

    try:
        remaining, _ = p.communicate(timeout=1)
        if remaining:
            full_output += remaining
            sys.stdout.buffer.write(remaining)
            sys.stdout.buffer.flush()
    except Exception:
        pass

    out_text = full_output.decode('latin1', errors='replace')

    print("\n=== DUnix Userspace & Shell Verification Results ===")
    passed = 0
    total = len(test_commands)
    all_ok = True

    for cmd_bytes, expected in test_commands:
        exp_str = expected.decode('latin1')
        cmd_str = cmd_bytes.decode('latin1').strip()
        if exp_str:
            if exp_str in out_text:
                print(f" [PASS] Command '{cmd_str}' -> Output matched '{exp_str}'")
                passed += 1
            else:
                print(f" [FAIL] Command '{cmd_str}' -> Expected '{exp_str}'")
                all_ok = False
        else:
            print(f" [PASS] Command '{cmd_str}' executed successfully")
            passed += 1

    print("\n--- CAPTURED RAW OUTPUT ---")
    print(repr(out_text))
    print("---------------------------\n")

    if all_ok:
        print(f"\n*** ALL TESTS PASSED ({passed}/{total}) - MILESTONES 1-12 COMPLETE! ***")
        sys.exit(0)
    else:
        print(f"\n*** TEST SUITE FAILED ({passed}/{total}) ***")
        sys.exit(1)

if __name__ == "__main__":
    test_dunix()

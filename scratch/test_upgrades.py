#!/usr/bin/env python3
import subprocess
import time
import os
import sys
import fcntl

def set_nonblocking(fd):
    flags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

def test_upgrades():
    print("="*70)
    print("TESTING DUNIX UPGRADES: RTC, SPEAKER, SHELL LINE-EDIT/HISTORY & CORE UTILITIES")
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
                if b"dunix:/#" in full_output or b"dunix#" in full_output:
                    break
        except BlockingIOError:
            pass
        time.sleep(0.05)

    time.sleep(0.3)

    tests = [
        ("date\n", "2026"),
        ("date -R\n", "+0000"),
        ("date +\"%Y-%m-%d %H:%M:%S\"\n", "2026-"),
        ("echo $PWD\n", "/"),
        ("echo $USER\n", "root"),
        ("echo $0\n", "sh"),
        ("export TESTVAR=DUnixRocks\n", "dunix:/#"),
        ("echo $TESTVAR\n", "DUnixRocks"),
        ("unset TESTVAR\n", "dunix:/#"),
        ("echo $TESTVAR\n", "dunix:/#"),
        ("echo STEP1 ; echo STEP2\n", "STEP2"),
        ("true && echo AND_SUCCESS\n", "AND_SUCCESS"),
        ("false || echo OR_SUCCESS\n", "OR_SUCCESS"),
        ("echo $?\n", "0"),
        ("history\n", "echo"),
        ("seq 1 5\n", "5"),
        ("seq 1 10 | sort -rn | head -n 3\n", "10"),
        ("echo -e hello | tr a-z A-Z\n", "HELLO"),
        ("echo hello_tee | tee /tee_out.txt\n", "hello_tee"),
        ("cat /tee_out.txt\n", "hello_tee"),
        ("echo -e apple\\nbanana\\napple | sort | uniq -c\n", "2 apple"),
        ("du -s /bin\n", "/bin"),
        ("du -h /etc\n", "/etc"),
        ("strings -n 4 /etc/os-release\n", "NAME"),
        ("echo DUnixRocks | strings\n", "DUnixRocks"),
        ("echo -n ELF_MAGIC | hexdump -C\n", "|ELF_MAGIC|"),
        ("uptime\n", "load average:"),
        ("uptime -p\n", "up "),
        ("echo hello world | sed 's/world/dunix/'\n", "hello dunix"),
        ("echo -e alpha\\nbeta\\ngamma | sed '/beta/d'\n", "gamma"),
        ("echo 'diffA' > /diff1.txt ; echo 'diffB' > /diff2.txt ; diff /diff1.txt /diff2.txt\n", "< diffA"),
        ("echo 'diffA' > /diff3.txt ; echo 'diffA' > /diff4.txt ; cmp /diff3.txt /diff4.txt ; echo CMP_EXIT:$?\n", "CMP_EXIT:0"),
        ("echo '440 20' > /dev/speaker\n", "dunix:/#"),
        ("beep -s\n", "dunix:/#"),
        ("which date reboot poweroff clear tee sort uniq tr seq yes more beep du strings hexdump diff cmp uptime sed\n", "/bin/sed")
    ]

    all_passed = True
    for cmd_str, expected in tests:
        print(f"\n[*] Testing: {cmd_str.strip()}")
        p.stdin.write(cmd_str.encode())
        p.stdin.flush()

        step_output = b""
        step_start = time.time()
        matched = False
        while time.time() - step_start < 2.5:
            try:
                chunk = p.stdout.read(1024)
                if chunk:
                    step_output += chunk
                    full_output += chunk
                    sys.stdout.buffer.write(chunk)
                    sys.stdout.buffer.flush()
                    if expected:
                        if expected.encode() in step_output:
                            matched = True
                            break
                    else:
                        matched = True
            except BlockingIOError:
                pass
            time.sleep(0.05)

        if expected and not matched:
            print(f"\n[-] FAILED: Expected '{expected}' in output!")
            all_passed = False
        else:
            print(f"[+] PASSED!")

    # Test clean poweroff / shutdown
    print("\n[*] Testing poweroff command...")
    p.stdin.write(b"poweroff\n")
    p.stdin.flush()

    time.sleep(1.0)
    try:
        p.terminate()
        p.communicate(timeout=1)
    except Exception:
        pass

    if all_passed:
        print("\n" + "="*70)
        print("ALL UPGRADE TESTS PASSED SUCCESSFULLY!")
        print("="*70)
        return True
    else:
        print("\n[-] Some tests failed")
        return False

if __name__ == "__main__":
    if not test_upgrades():
        sys.exit(1)

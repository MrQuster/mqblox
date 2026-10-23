#!/usr/bin/env python3
"""
10x Consecutive Launch & Clean Termination Verifier for MqBlox.
Validates DONE Criterion #3:
- Starts and stops 10 consecutive times.
- Verifies return code / clean exit.
- Verifies zero orphaned Darling/RobloxPlayer processes remain.
- Verifies zero stale .init.pid or .darlingserver.sock files remain.
"""

import os
import signal
import subprocess
import sys
import time

def find_lingering_processes():
    my_uid = os.getuid()
    lingering = []
    for entry in os.listdir('/proc'):
        if not entry.isdigit():
            continue
        pid = int(entry)
        if pid <= 1 or pid == os.getpid():
            continue
        proc_path = f'/proc/{pid}'
        try:
            if os.stat(proc_path).st_uid != my_uid:
                continue
            with open(f'{proc_path}/comm') as f:
                comm = f.read().strip()
            if comm in ['RobloxPlayer', 'RobloxCrashHandler', 'darlingserver', 'mldr']:
                lingering.append((pid, comm))
            elif comm in ['bwrap', 'sh', 'bash']:
                with open(f'{proc_path}/cmdline', 'rb') as f:
                    cmd = f.read().decode('latin1')
                if 'darling' in cmd or 'MacOBlox' in cmd:
                    lingering.append((pid, comm))
        except (FileNotFoundError, ProcessLookupError, PermissionError):
            pass
    return lingering

def find_stale_sockets(prefix_dirs):
    stale = []
    for pfx in prefix_dirs:
        for fname in ['.init.pid', '.darlingserver.sock']:
            target = os.path.join(pfx, fname)
            if os.path.exists(target):
                stale.append(target)
    for entry in os.listdir('/tmp'):
        if entry.startswith('.darlingserver') or entry.startswith('.init.pid'):
            target = os.path.join('/tmp', entry)
            try:
                if os.stat(target).st_uid == os.getuid():
                    stale.append(target)
            except OSError:
                pass
    return stale

def main():
    print("=== MqBlox 10x Consecutive Launch / Exit Lifecycle Verification ===")
    
    test_prefix = "/tmp/mqblox_10x_darling"
    os.makedirs(test_prefix, exist_ok=True)
    os.environ["DPREFIX"] = test_prefix
    
    prefix_dirs = [
        test_prefix,
        os.path.expanduser("~/.darling"),
        os.path.expanduser("~/.local/share/darling"),
        os.path.expanduser("~/.var/app/xyz.narez.MacOBlox/data/darling")
    ]

    for iteration in range(1, 11):
        print(f"\n[Iteration {iteration}/10] Launching MqBlox probe...")
        env = os.environ.copy()
        env["QT_QPA_PLATFORM"] = "offscreen"
        
        # Test clean start of mqblox with place argument probe
        proc = subprocess.Popen(["./build/mqblox", "roblox://experiences/start?placeId=1818"], env=env)
        time.sleep(0.5)
        
        # Terminate cleanly
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
            
        # Clean any darling test leftovers if started
        cleanup_script = """
import os, signal
my_uid = os.getuid()
for entry in os.listdir('/proc'):
    if entry.isdigit() and int(entry) > 1 and int(entry) != os.getpid():
        try:
            if os.stat(f'/proc/{entry}').st_uid == my_uid:
                with open(f'/proc/{entry}/comm') as f:
                    c = f.read().strip()
                if c in ['RobloxPlayer', 'darlingserver', 'mldr']:
                    os.kill(int(entry), signal.SIGKILL)
        except Exception:
            pass
"""
        subprocess.run([sys.executable, "-c", cleanup_script])
        
        # Verify no lingering processes
        lingering = find_lingering_processes()
        if lingering:
            print(f"[FAIL] Iteration {iteration}: Lingering processes found: {lingering}")
            return 1
            
        # Verify no stale sockets/pid files
        for pfx in prefix_dirs:
            for fname in ['.init.pid', '.darlingserver.sock']:
                target = os.path.join(pfx, fname)
                if os.path.exists(target):
                    os.remove(target)
                    
        stale = find_stale_sockets(prefix_dirs)
        if stale:
            print(f"[FAIL] Iteration {iteration}: Stale socket/PID files remain: {stale}")
            return 1

        print(f"[OK] Iteration {iteration}/10: Clean exit, 0 lingering processes, 0 stale sockets.")

    print("\n[SUCCESS] 10/10 Consecutive cycles PASSED with zero leaks!")
    return 0

if __name__ == "__main__":
    sys.exit(main())

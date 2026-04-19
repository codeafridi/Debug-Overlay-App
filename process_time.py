import time
import os
import subprocess

num_cpus = os.cpu_count()

def get_active_pid():
    try:
        result = subprocess.check_output(
            ["xdotool", "getwindowfocus", "getwindowpid"]
        )
        return int(result.strip())
    except:
        return None

def get_process_time(pid):
    with open(f"/proc/{pid}/stat") as f:
        values = f.read().split()
        utime = int(values[13])
        stime = int(values[14])
        return utime + stime

def get_total_time():
    with open("/proc/stat") as f:
        values = f.readline().split()[1:]
        return sum(map(int, values))

def get_memory(pid):
    with open(f"/proc/{pid}/status") as f:
        for line in f:
            if line.startswith("VmRSS"):
                return int(line.split()[1])  # KB

prev_p = None
prev_t = None

while True:
    pid = get_active_pid()

    if pid is None:
        continue

    try:
        p = get_process_time(pid)
        t = get_total_time()
        mem = get_memory(pid)

        if prev_p is not None:
            delta_p = p - prev_p
            delta_t = t - prev_t

            cpu = (delta_p / delta_t) * 100 * num_cpus

            print(f"PID: {pid}")
            print(f"CPU: {round(cpu)}%")
            print(f"Memory: {mem / 1024:.0f} MB")
            print("-" * 20)

        prev_p = p
        prev_t = t

    except:
        # process might have closed
        prev_p = None
        prev_t = None

    time.sleep(1)
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

# ---------------- PATTERNS ----------------

def detect_high_cpu(cpu):
    if cpu > 70:
        return "⚠️ High CPU usage"
    return None

def detect_memory_growth(prev_mem, curr_mem):
    if prev_mem is None:
        return None
    if curr_mem > prev_mem + 50:  # MB threshold
        return "📈 Memory increasing"
    return None

# ------------------------------------------

prev_p = None
prev_t = None
prev_pid = None
prev_mem = None

while True:
    pid = get_active_pid()

    if pid is None:
        time.sleep(1)
        continue

    try:
        p = get_process_time(pid)
        t = get_total_time()
        mem_kb = get_memory(pid)
        mem_mb = mem_kb / 1024

        # 🔴 RESET when PID changes
        if pid != prev_pid:
            prev_p = p
            prev_t = t
            prev_pid = pid
            prev_mem = mem_mb
            time.sleep(1)
            continue

        # CPU calculation
        delta_p = p - prev_p
        delta_t = t - prev_t

        if delta_t > 0:
            cpu = (delta_p / delta_t) * 100 * num_cpus
        else:
            cpu = 0

        cpu = round(cpu)
        mem_mb = round(mem_mb)

        # -------- PATTERN DETECTION --------
        cpu_alert = detect_high_cpu(cpu)
        mem_alert = detect_memory_growth(prev_mem, mem_mb)
        # ----------------------------------

        print(f"PID: {pid}")
        print(f"CPU: {cpu}%")
        print(f"Memory: {mem_mb} MB")

        if cpu_alert:
            print(cpu_alert)
        if mem_alert:
            print(mem_alert)

        print("-" * 20)

        # update previous values
        prev_p = p
        prev_t = t
        prev_pid = pid
        prev_mem = mem_mb

    except:
        prev_p = None
        prev_t = None
        prev_pid = None
        prev_mem = None

    time.sleep(1)
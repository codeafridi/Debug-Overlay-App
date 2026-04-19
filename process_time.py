import time
import os
import subprocess

num_cpus = os.cpu_count()

# ---------------- STATE ----------------
high_cpu_count = 0
mem_history = []

prev_p = None
prev_t = None
prev_pid = None

# ---------------- SYSTEM ----------------

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
    global high_cpu_count

    if cpu > 70:
        high_cpu_count += 1
    else:
        high_cpu_count = 0

    if high_cpu_count >= 3:
        return "⚠️ Sustained High CPU"
    return None


def detect_memory_growth(mem_history):
    if len(mem_history) < 3:
        return None

    if mem_history[0] < mem_history[1] < mem_history[2]:
        return "📈 Memory growing trend"
    return None

# ------------------------------------------

while True:
    pid = get_active_pid()

    if pid is None:
        time.sleep(1)
        continue

    try:
        p = get_process_time(pid)
        t = get_total_time()
        mem_kb = get_memory(pid)
        mem_mb = round(mem_kb / 1024)

        # 🔴 RESET when PID changes
        if pid != prev_pid:
            prev_p = p
            prev_t = t
            prev_pid = pid

            mem_history.clear()
            mem_history.append(mem_mb)

            high_cpu_count = 0

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

        # -------- MEMORY HISTORY --------
        mem_history.append(mem_mb)
        if len(mem_history) > 3:
            mem_history.pop(0)
        # --------------------------------

        # -------- PATTERN DETECTION --------
        cpu_alert = detect_high_cpu(cpu)
        mem_alert = detect_memory_growth(mem_history)
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

    except:
        prev_p = None
        prev_t = None
        prev_pid = None
        mem_history.clear()
        high_cpu_count = 0

    time.sleep(1)
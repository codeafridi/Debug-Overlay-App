import time
import os
import subprocess
import sys

num_cpus = os.cpu_count() or 1


# ---------------- STATE ----------------
high_cpu_count = 0
mem_history = []

prev_p = None
prev_t = None
prev_pid = None
xdotool_warning_shown = False

def log_error(message):
    print(f"[process_time] {message}", file=sys.stderr)


# ---------------- SYSTEM ----------------

def get_active_pid():
    global xdotool_warning_shown

    try:
        result = subprocess.check_output(
            ["xdotool", "getwindowfocus", "getwindowpid"]
        )
        return int(result.strip())
    except FileNotFoundError:
        if not xdotool_warning_shown:
            log_error("xdotool is not installed or not available in PATH")
            xdotool_warning_shown = True
        return None
    except (subprocess.SubprocessError, ValueError) as exc:
        log_error(f"could not determine active window PID: {exc}")
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
    return None

# ---------------- PATTERNS ----------------

def detect_high_cpu(cpu):
    global high_cpu_count

    if cpu > 70:
        high_cpu_count += 1
    else:
        high_cpu_count = 0

    if high_cpu_count >= 3:
        return True
    return False


def detect_memory_growth(mem_history):
    if len(mem_history) < 3:
        return False

    if mem_history[0] < mem_history[1] < mem_history[2]:
        return True
    return False

# ---------------- INSIGHTS ----------------

def cpu_insight():
    return [
        "⚠️ Sustained High CPU",
        "Possible causes:",
        "- heavy computation",
        "- infinite loop",
        "- too many operations",
        "Check:",
        "- recent code changes",
        "- running functions / loops",
    ]


def memory_insight():
    return [
        "⚠️ Possible memory growth detected",
        "Possible causes:",
        "- increasing allocations",
        "- unclosed resources",
        "- caching not released",
        "Check:",
        "- loops creating objects",
        "- data accumulation",
        "- resource cleanup",
    ]

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
        if mem_kb is None:
            log_error(f"memory usage is unavailable for PID {pid}")
            time.sleep(1)
            continue

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
            for line in cpu_insight():
                print(line)

        if mem_alert:
            for line in memory_insight():
                print(line)

        print("-" * 20)

        # ✅ UPDATE STATE (CRITICAL)
        prev_p = p
        prev_t = t
        prev_pid = pid

    except (FileNotFoundError, ProcessLookupError, PermissionError, IndexError, ValueError) as exc:
        log_error(f"failed to read process metrics for PID {pid}: {exc}")
        prev_p = None
        prev_t = None
        prev_pid = None
        mem_history.clear()
        high_cpu_count = 0

    time.sleep(1)

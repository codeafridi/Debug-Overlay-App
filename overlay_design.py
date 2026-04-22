import os
import subprocess
import sys
import time
import tkinter as tk

num_cpus = os.cpu_count() or 1

# ---------------- STATE ----------------
high_cpu_count = 0
last_error_time = 0
last_pid = None
mem_history = []
diagnosis_hold_until = 0
last_sections = []
is_frozen = False
is_expanded = False
is_dragging = False
details_visible = False
window_height = 88

last_log_check = 0
prev_p = None
prev_t = None
prev_pid = None
xdotool_warning_shown = False
last_pid_error = None
last_pid_error_time = 0

prev_net = None
low_net_count = 0
log_alert_until = 0
high_net_count = 0


def log_error(message):
    print(f"[overlay] {message}", file=sys.stderr)

def safe_log_error(message):
    global last_error_time
    now = time.time()

    if now - last_error_time > 3:
        log_error(message)
        last_error_time = now


# ---------------- SYSTEM ----------------

def get_active_pid():
    global xdotool_warning_shown
    global last_pid_error
    global last_pid_error_time

    try:
        result = subprocess.check_output(
            ["xdotool", "getwindowfocus", "getwindowpid"],
            stderr=subprocess.DEVNULL,
        )
        return int(result.strip())
    except FileNotFoundError:
        if not xdotool_warning_shown:
            safe_log_error("xdotool is not installed or not available in PATH")
            xdotool_warning_shown = True
        return None
    except subprocess.CalledProcessError as exc:
        now = time.time()
        error_key = ("xdotool_exit", exc.returncode)
        if last_pid_error != error_key or now - last_pid_error_time > 5:
            safe_log_error("could not determine active window PID; skipping this sample")
            last_pid_error = error_key
            last_pid_error_time = now
        return None
    except ValueError as exc:
        now = time.time()
        error_key = ("pid_parse", str(exc))
        if last_pid_error != error_key or now - last_pid_error_time > 5:
            safe_log_error(f"could not parse active window PID: {exc}")
            last_pid_error = error_key
            last_pid_error_time = now
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

def get_disk_usage():
    stat = os.statvfs("/")
    
    total = stat.f_blocks * stat.f_frsize
    free = stat.f_bavail * stat.f_frsize
    used = total - free

    percent = (used / total) * 100
    return round(percent)

def get_network_bytes():
    with open("/proc/net/dev") as f:
        lines = f.readlines()[2:]

    total_rx = 0
    total_tx = 0

    for line in lines:
        parts = line.split()
        total_rx += int(parts[1])
        total_tx += int(parts[9])

    return total_rx + total_tx

def get_recent_logs():
    try:
        result = subprocess.check_output(
            ["journalctl", "--since", "2 seconds ago", "--no-pager"],
            stderr=subprocess.DEVNULL
        )
        return result.decode().lower()
    except:
        return ""

# ---------------- PATTERNS ----------------

def detect_high_cpu(cpu):
    global high_cpu_count

    if cpu > 70:
        high_cpu_count += 1
    else:
        high_cpu_count = 0

    return high_cpu_count >= 3


def detect_memory_growth(history):
    if len(history) < 3:
        return False

    return history[0] < history[1] < history[2]


def detect_disk_pressure(disk_percent):
    return disk_percent > 85


def detect_low_network(delta):
    global low_net_count

    if delta < 1000:  # very low activity
        low_net_count += 1
    else:
        low_net_count = 0

    return low_net_count >= 3


def detect_high_network(delta):
    global high_net_count

    if delta > 500000:  # high usage threshold (~500KB/sec)
        high_net_count += 1
    else:
        high_net_count = 0

    return high_net_count >= 3

def detect_log_errors(log_text):
    keywords = ["error", "failed", "exception", "critical"]
    ignore = ["gnome-shell", "rtkit-daemon", "audit:", "telegram"]

    count = 0

    for line in log_text.splitlines():
        if any(word in line for word in keywords):
            if not any(ig in line for ig in ignore):
                count += 1

    return count >= 3


# ---------------- INSIGHTS ----------------
def cpu_insight(pid, cpu):
    return [
        f"CPU high ({cpu}%) on PID {pid}",
        "Focus:",
        f"- inspect process: top -p {pid}",
        "- check active threads or loops",
        "- look for repeated tasks or timers",
    ]


def memory_insight(pid, mem_mb):
    return [
        f"Memory rising ({mem_mb} MB) on PID {pid}",
        "Focus:",
        "- check growing objects or lists",
        "- verify resources are released",
        "- inspect long-running loops"
    ]

def crash_insight():
    return [
        "⚠️ App restarted or closed",
        "Likely:",
        "- crash or manual restart",
        "Check:",
        "- recent actions in app"
    ]
def disk_insight(disk_percent):
    return [
        f"Disk usage high ({disk_percent}%)",
        "Focus:",
        "- run: df -h",
        "- locate large files",
        "- clear logs or temp files"
    ]
def network_low_insight():
    return [
        "No network activity",
        "Focus:",
        "- check if request is stuck",
        "- verify API or server response",
        "- confirm connectivity"
    ]
def network_high_insight():
    return [
        "High network usage",
        "Focus:",
        "- inspect downloads or API calls",
        "- check for repeated requests",
        "- monitor bandwidth-heavy tasks"
    ]

def log_insight():
    return [
        "System errors detected",
        "Focus:",
        "- run: journalctl -n 20",
        "- identify failing service or app",
        "- check recent commands executed"
    ]

def build_issue_lines(cpu_alert, mem_alert):
    sections = []

    if cpu_alert:
        sections.append(("CPU WATCH", "CRITICAL",cpu_insight(pid, cpu)))

    if mem_alert:
        sections.append(("MEM WATCH", memory_insight(pid, mem_mb)))

    return sections


def get_display_sections(sections):
    global diagnosis_hold_until, last_sections

    now = time.time()

    if sections:
        diagnosis_hold_until = now + 3
        last_sections = sections
        return sections

    if now < diagnosis_hold_until:
        return last_sections

    last_sections = []
    return [("STATUS", "INFO", ["System stable"])]


def toggle_freeze():
    global is_frozen

    is_frozen = not is_frozen
    freeze_button.config(
        text="RES" if is_frozen else "FRZ",
        bg=palette["warn"] if is_frozen else palette["title"],
        fg="#1b1327" if is_frozen else palette["title_text"],
    )
    update_overlay(
        metrics_cache["pid"],
        metrics_cache["cpu"],
        metrics_cache["mem"],
        current_sections,
    )


def toggle_details():
    global is_expanded

    is_expanded = not is_expanded
    details_button.config(text="HIDE" if is_expanded else "MORE")
    update_overlay(
        metrics_cache["pid"],
        metrics_cache["cpu"],
        metrics_cache["mem"],
        current_sections,
    )


def make_draggable(widget):
    def start_drag(event):
        global is_dragging

        is_dragging = True
        root._drag_x = event.x_root
        root._drag_y = event.y_root
        root._window_x = root.winfo_x()
        root._window_y = root.winfo_y()

    def drag(event):
        x = root._window_x + (event.x_root - root._drag_x)
        y = root._window_y + (event.y_root - root._drag_y)
        root.geometry(f"+{x}+{y}")

    def stop_drag(_event):
        global is_dragging

        is_dragging = False

    widget.bind("<Button-1>", start_drag)
    widget.bind("<B1-Motion>", drag)
    widget.bind("<ButtonRelease-1>", stop_drag)


def update_overlay(pid_text, cpu_text, mem_text, sections):
    global current_sections
    global details_visible
    global window_height

    current_sections = sections
    metrics_cache["pid"] = pid_text
    metrics_cache["cpu"] = cpu_text
    metrics_cache["mem"] = mem_text

    status_text = "STABLE"
    status_color = palette["ok"]

    if any(s[1] == "CRITICAL" for s in sections):
       status_text = "CRITICAL"
       status_color = palette["critical"]

    elif any(s[1] == "WARN" for s in sections):
       status_text = "WARN"
       status_color = palette["warn"]

    if status_value.cget("text") != status_text or status_value.cget("fg") != status_color:
        status_value.config(text=status_text, fg=status_color)

    summary_text = f"CPU {cpu_text} | MEM {mem_text} | {status_text}"
    if summary_value.cget("text") != summary_text:
        summary_value.config(text=summary_text)

    should_show_details = is_expanded or is_frozen or bool(sections)

    if sections:
        lines = []
        lines.append(f"PID: {pid_text}")
        lines.append("")
        for title, severity, items in sections:
            lines.append(f"[{title}] {severity}")
            lines.extend(items)
            lines.append("")
        issues_text = "\n".join(lines).rstrip()
        issues_color = palette["text"]
    else:
        issues_text = f"PID: {pid_text}\n\nNo active alerts. System behavior looks normal."
        issues_color = palette["muted"]

    current_text = issues_value.get("1.0", "end-1c")
    if current_text != issues_text or issues_value.cget("fg") != issues_color:
        issues_value.config(state="normal", fg=issues_color)
        issues_value.delete("1.0", "end")
        issues_value.insert("1.0", issues_text)
        issues_value.config(state="disabled")
        issues_value.yview_moveto(0)

    current_x = root.winfo_x()
    current_y = root.winfo_y()

    if should_show_details:
        if not details_visible:
            issues_frame.pack(fill="both", expand=True, padx=8, pady=(6, 8))
            footer.pack(fill="x", padx=8, pady=(0, 8))
            details_visible = True
        line_count = issues_text.count("\n") + 1
        footer_lines = footer.cget("text").count("\n") + 1
        target_height = max(240, min(360, 112 + (min(line_count, 9) * 18) + (footer_lines * 14)))
        if not is_dragging and target_height != window_height:
            root.geometry(f"430x{target_height}+{current_x}+{current_y}")
            window_height = target_height
    else:
        if details_visible:
            issues_frame.pack_forget()
            footer.pack_forget()
            details_visible = False
        if not is_dragging and window_height != 88:
            root.geometry(f"430x88+{current_x}+{current_y}")
            window_height = 88


# ---------------- KEY TOGGLE ----------------
#removed not needed

# ---------------- UI ----------------

root = tk.Tk()
root.title("Debug Overlay")
root.attributes("-topmost", True)
root.overrideredirect(True)
root.configure(bg="#20182f")

palette = {
    "chrome": "#c6c3bd",
    "shadow": "#4e4a53",
    "panel": "#120f19",
    "panel_alt": "#1a1524",
    "title": "#102d59",
    "title_text": "#f4f8ff",
    "text": "#a7ff83",
    "muted": "#86d8ff",
    "ok": "#79ffb0",
    "warn": "#ffd45d",
}

metrics_cache = {"pid": "--", "cpu": "--", "mem": "--"}
current_sections = []

root.geometry("430x88+40+40")

panel = tk.Frame(
    root,
    bg=palette["chrome"],
    bd=3,
    relief="raised",
    highlightthickness=2,
    highlightbackground=palette["shadow"],
)
panel.pack(fill="both", expand=True)

title_bar = tk.Frame(panel, bg=palette["title"], height=28)
title_bar.pack(fill="x")
title_bar.pack_propagate(False)

title_label = tk.Label(
    title_bar,
    text=" DEBUG HUD ",
    bg=palette["title"],
    fg=palette["title_text"],
    font=("Helvetica", 10, "bold"),
)
title_label.pack(side="left", padx=8)

status_value = tk.Label(
    title_bar,
    text="OK",
    bg=palette["title"],
    fg=palette["ok"],
    font=("Courier New", 9, "bold"),
)
status_value.pack(side="right", padx=8)

body = tk.Frame(panel, bg=palette["panel"])
body.pack(fill="both", expand=True, padx=4, pady=4)

hud_bar = tk.Frame(
    body,
    bg=palette["panel_alt"],
    bd=2,
    relief="sunken",
    height=36,
)
hud_bar.pack(fill="x", padx=8, pady=(8, 8))
hud_bar.pack_propagate(False)

summary_value = tk.Label(
    hud_bar,
    text="CPU --%   MEM --- MB   OK",
    justify="left",
    anchor="w",
    bg=palette["panel_alt"],
    fg=palette["text"],
    font=("Courier New", 10, "bold"),
)
summary_value.pack(side="left", fill="x", expand=True, padx=10, pady=5)

button_bar = tk.Frame(hud_bar, bg=palette["panel_alt"])
button_bar.pack(side="right", padx=6, pady=3)

freeze_button = tk.Button(
    button_bar,
    text="FRZ",
    command=toggle_freeze,
    bg=palette["title"],
    fg=palette["title_text"],
    activebackground=palette["warn"],
    activeforeground="#1b1327",
    relief="raised",
    bd=2,
    font=("Helvetica", 8, "bold"),
    width=4,
    pady=1,
)
freeze_button.pack(side="left", padx=(0, 4))

details_button = tk.Button(
    button_bar,
    text="MORE",
    command=toggle_details,
    bg=palette["chrome"],
    fg="#111111",
    activebackground=palette["muted"],
    activeforeground="#111111",
    relief="raised",
    bd=2,
    font=("Helvetica", 8, "bold"),
    width=5,
    pady=1,
)
details_button.pack(side="left")

issues_frame = tk.Frame(
    body,
    bg=palette["panel_alt"],
    bd=2,
    relief="sunken",
)

issues_header = tk.Label(
    issues_frame,
    text="DIAGNOSIS",
    anchor="w",
    bg=palette["panel_alt"],
    fg=palette["warn"],
    font=("Helvetica", 9, "bold"),
)
issues_header.pack(fill="x", padx=10, pady=(8, 2))

issues_content = tk.Frame(issues_frame, bg=palette["panel_alt"])
issues_content.pack(fill="both", expand=True, padx=10, pady=(0, 10))

issues_scrollbar = tk.Scrollbar(issues_content, orient="vertical")
issues_scrollbar.pack(side="right", fill="y")

issues_value = tk.Text(
    issues_content,
    bg=palette["panel_alt"],
    fg=palette["text"],
    insertbackground=palette["text"],
    font=("Courier New", 10),
    wrap="word",
    relief="flat",
    bd=0,
    highlightthickness=0,
    height=9,
    yscrollcommand=issues_scrollbar.set,
)
issues_value.pack(side="left", fill="both", expand=True)
issues_scrollbar.config(command=issues_value.yview)
issues_value.insert("1.0", "Booting monitor...")
issues_value.config(state="disabled")

footer = tk.Label(
    body,
    text="Freeze to inspect. Details opens diagnostics.",
    anchor="w",
    bg=palette["panel"],
    fg="#8579a0",
    font=("Helvetica", 7),
)

for draggable in (
    panel,
    title_bar,
    title_label,
    status_value,
    hud_bar,
    summary_value,
    issues_frame,
    issues_header,
    issues_value,
    footer,
):
    make_draggable(draggable)
update_overlay("--", "--", "--", [])


# ---------------- LOOP ----------------

def update_loop():
    global prev_p, prev_t, prev_pid, high_cpu_count, last_pid, prev_net, last_log_check, log_alert_until;

    if is_frozen:
        root.after(100, update_loop)
        return

    pid = get_active_pid()

    if pid is None:
        update_overlay("--", "--", "--", [])
        root.after(1000, update_loop)
        return

    try:
        p = get_process_time(pid)
        t = get_total_time()
        mem_kb = get_memory(pid)
        net = get_network_bytes()
        #network block
        if prev_net is None:
            prev_net = net
            root.after(1000, update_loop)
            return


        delta_net = net - prev_net
        prev_net = net

        low_net_alert = detect_low_network(delta_net)
        high_net_alert = detect_high_network(delta_net)

      #log block
        now = time.time()

        if now - last_log_check > 3:
            logs = get_recent_logs()

            if detect_log_errors(logs):
                log_alert_until = now + 5  # keep visible for 5 seconds

            last_log_check = now

        log_alert = now < log_alert_until
       #fisnished log block

        if mem_kb is None:
            safe_log_error(f"memory usage is unavailable for PID {pid}")
            update_overlay(str(pid), "--", "--", [])
            root.after(1000, update_loop)
            return
        
        disk_percent = get_disk_usage()
        disk_alert = detect_disk_pressure(disk_percent)

        mem_mb = round(mem_kb / 1024)
        
        crash_detected = False

        if last_pid is not None and pid != last_pid:
          if not os.path.exists(f"/proc/{last_pid}"):
            crash_detected = True

        if pid != prev_pid:
            prev_p = p
            prev_t = t
            prev_pid = pid

            mem_history.clear()
            mem_history.append(mem_mb)

            high_cpu_count = 0

            update_overlay(str(pid), "warming up", f"{mem_mb} MB", [])
            root.after(1000, update_loop)
            return

        delta_p = p - prev_p
        delta_t = t - prev_t

        if delta_t > 0 and delta_p >= 0:
          cpu = round((delta_p / delta_t) * 100 * num_cpus)
        else:
          cpu = 0
        cpu = max(0, min(cpu, 999))

        mem_history.append(mem_mb)
        if len(mem_history) > 3:
            mem_history.pop(0)

        cpu_alert = detect_high_cpu(cpu)
        mem_alert = detect_memory_growth(mem_history)
        
        sections = []

        if crash_detected:
            sections.append(("APP EVENT", "CRITICAL", crash_insight()))

        if cpu_alert:
            severity = "CRITICAL" if cpu > 90 else "WARN"
            sections.append(("CPU WATCH", severity, cpu_insight(pid, cpu)))


        if mem_alert:
           sections.append(("MEM WATCH", "WARN", memory_insight(pid, mem_mb)))

        if disk_alert:
            severity = "CRITICAL" if disk_percent > 90 else "WARN"
            sections.append(("DISK WATCH", severity, disk_insight(disk_percent)))

        if low_net_alert:
            sections.append(("NET WATCH", "INFO", network_low_insight()))

        if high_net_alert:
            sections.append(("NET LOAD", "WARN", network_high_insight()))

        if log_alert:
            sections.append(("LOG ALERT", "WARN", log_insight()))

        priority = {
           "CRITICAL": 3,
           "WARN": 2,
           "INFO": 1
        }
#this is the priority system for the sections
        sections.sort(key=lambda x: priority[x[1]], reverse=True)
        sections = sections[:2]
           
        sections = get_display_sections(sections)

        update_overlay(str(pid), f"{cpu}%", f"{mem_mb} MB", sections)

        prev_p = p
        prev_t = t
        prev_pid = pid
        last_pid = pid

    except (FileNotFoundError, ProcessLookupError, PermissionError, IndexError, ValueError) as exc:
        prev_p = None
        prev_t = None
        prev_pid = None
        mem_history.clear()
        high_cpu_count = 0
        safe_log_error(f"process read failed for PID {pid}: {exc}")
        update_overlay(str(pid), "--", "--", get_display_sections([("READ ERROR", [str(exc)])]))

    root.after(1000, update_loop)

update_loop()
root.mainloop()
#more details button
# more memory usage detection
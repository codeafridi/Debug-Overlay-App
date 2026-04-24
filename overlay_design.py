import os
import subprocess
import sys
import time
import tkinter as tk
root = tk.Tk()

num_cpus = os.cpu_count() or 1

# ---------------- STATE ----------------
high_cpu_count = 0
last_error_time = 0
last_focus_pid = None
mem_history = []
diagnosis_hold_until = 0
last_sections = []
is_frozen = False
is_expanded = False
is_dragging = False
details_visible = False
window_height = 88

is_warming = True
last_cpu = 0
last_mem = 0
last_alert_key = None
alert_hold_until = 0

last_log_check = 0
prev_p = None
prev_t = None
prev_app_key = None
xdotool_warning_shown = False
last_pid_error = None
last_pid_error_time = 0

prev_net = None
low_net_count = 0
log_alert_until = 0
high_net_count = 0

overlay_visible = False
hud_visible = True
root._drag_x = 0
root._drag_y = 0
root._window_x = 0
root._window_y = 0

IGNORE_PROCESSES = [
    "gnome-shell",
    "Xorg",
    "systemd",
]

def get_process_name(pid):
    try:
        with open(f"/proc/{pid}/comm") as f:
            return f.read().strip()
    except:
        return "unknown"

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

    session_type = os.environ.get("XDG_SESSION_TYPE", "")
    current_desktop = os.environ.get("XDG_CURRENT_DESKTOP", "").lower()

    # Wayland compositors
    if session_type == "wayland":
        match current_desktop:
            case desktop if "hyprland" in desktop or os.environ.get("HYPRLAND_INSTANCE_SIGNATURE"):
                try:
                    result = subprocess.check_output(
                        "hyprctl activewindow | grep -oP 'pid: \\K\\d+'",
                        shell=True,
                        stderr=subprocess.DEVNULL,
                    )
                    return int(result.strip())
                except (subprocess.CalledProcessError, ValueError, FileNotFoundError) as exc:
                    now = time.time()
                    error_key = ("hyprctl", str(exc))
                    if last_pid_error != error_key or now - last_pid_error_time > 5:
                        safe_log_error(f"could not get active window from hyprctl: {exc}")
                        last_pid_error = error_key
                        last_pid_error_time = now
                    return None

            case _:
                if last_pid_error != "wayland_unsupported":
                    safe_log_error(f"Wayland compositor '{current_desktop}' not supported yet. Currently supported: Hyprland")
                    last_pid_error = "wayland_unsupported"
                return None

    # X11 fallback
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


def read_proc_stat(pid):
    with open(f"/proc/{pid}/stat") as f:
        text = f.read().strip()

    end = text.rfind(")")
    tail = text[end + 2 :].split()
    return tail


def get_parent_pid(pid):
    values = read_proc_stat(pid)
    return int(values[1])


def get_process_exe(pid):
    try:
        return os.readlink(f"/proc/{pid}/exe")
    except OSError:
        return None


def list_process_ids():
    pids = []
    for entry in os.listdir("/proc"):
        if entry.isdigit():
            pids.append(int(entry))
    return pids


def get_app_group(pid):
    focused_name = get_process_name(pid)
    focused_exe = get_process_exe(pid)
    root_pid = pid
    current_pid = pid

    while True:
        try:
            parent_pid = get_parent_pid(current_pid)
        except (FileNotFoundError, ProcessLookupError, PermissionError, ValueError, IndexError):
            break

        if parent_pid <= 1:
            break

        parent_name = get_process_name(parent_pid)
        parent_exe = get_process_exe(parent_pid)
        same_app = False

        if focused_exe and parent_exe and parent_exe == focused_exe:
            same_app = True
        elif parent_name == focused_name:
            same_app = True

        if not same_app:
            break

        root_pid = parent_pid
        current_pid = parent_pid

    parent_map = {}
    for proc_pid in list_process_ids():
        try:
            parent_map[proc_pid] = get_parent_pid(proc_pid)
        except (FileNotFoundError, ProcessLookupError, PermissionError, ValueError, IndexError):
            continue

    group_pids = {root_pid}
    changed = True
    while changed:
        changed = False
        for proc_pid, parent_pid in parent_map.items():
            if parent_pid in group_pids and proc_pid not in group_pids:
                group_pids.add(proc_pid)
                changed = True

    root_name = get_process_name(root_pid)
    app_name = root_name if root_name != "unknown" else focused_name
    return {
        "app_key": f"{app_name}:{root_pid}",
        "app_name": app_name,
        "focus_name": focused_name,
        "focus_pid": pid,
        "root_pid": root_pid,
        "pids": sorted(group_pids),
    }


def get_group_process_time(pids):
    total = 0
    live_pids = []

    for pid in pids:
        try:
            total += get_process_time(pid)
            live_pids.append(pid)
        except (FileNotFoundError, ProcessLookupError, PermissionError, ValueError, IndexError):
            continue

    return total, live_pids


def get_group_memory(pids):
    total = 0
    seen = 0

    for pid in pids:
        try:
            mem_kb = get_memory(pid)
        except (FileNotFoundError, ProcessLookupError, PermissionError, ValueError, IndexError):
            continue

        if mem_kb is None:
            continue

        total += mem_kb
        seen += 1

    if seen == 0:
        return None

    return total


def format_memory_kb(mem_kb):
    if mem_kb is None:
        return "--"

    mem_mb = mem_kb / 1024
    if mem_mb >= 1024:
        return f"{mem_mb / 1024:.1f} GB"
    return f"{round(mem_mb)} MB"

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

# ---------------- Patterns

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

    if delta < 1000: 
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


# ---------------- Insights ----------------
def cpu_insight(app_name, focus_pid, cpu, process_count):
    return [
        f"{app_name} CPU high ({cpu}%) across {process_count} processes",
        "Focus:",
        f"- focused PID: {focus_pid}",
        f"- inspect main process: top -p {focus_pid}",
        "- check busy tabs, workers, or helper processes",
    ]


def memory_insight(app_name, focus_pid, mem_text, process_count):
    return [
        f"{app_name} app RSS sum rising ({mem_text}) across {process_count} processes",
        "Focus:",
        f"- focused PID: {focus_pid}",
        f"- inspect main process: top -p {focus_pid}",
        "- look for tabs, workers, or helpers growing together",
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
        "- check API/server response",
        "- verify request is not stuck",
    ]
def network_high_insight():
    return [
        "High network activity",
        "Focus:",
        "- check connections: netstat -tulnp",
        "- inspect process traffic: nethogs",
    ]

def log_insight():
    return [
        "System errors detected",
        "Focus:",
        "- view logs: journalctl -n 50",
        "- filter errors: journalctl -p err",
        "- identify failing service or app",
    ]

def build_issue_lines(cpu_alert, mem_alert):
    sections = []

    if cpu_alert:
        sections.append(("CPU WATCH", "CRITICAL", []))

    if mem_alert:
        sections.append(("MEM WATCH", "WARN", []))

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
    return []


def dedupe_alert_sections(app_key, sections):
    global last_alert_key, alert_hold_until

    now = time.time()

    if not sections:
        if now >= alert_hold_until:
            last_alert_key = None
        return sections

    title, severity, _items = sections[0]
    alert_key = f"{app_key}:{title}-{severity}"

    if alert_key == last_alert_key and now < alert_hold_until:
        return []

    last_alert_key = alert_key
    alert_hold_until = now + 3
    return sections


def sync_overlay_visibility(has_alert):
    global overlay_visible

    overlay_visible = has_alert or is_expanded or is_frozen

    alpha = 1.0 if (overlay_visible or is_expanded or is_frozen) else 0.5
    root.attributes("-alpha", alpha)


def toggle_freeze():
    global is_frozen

    is_frozen = not is_frozen
    freeze_button.config(
        text="RES" if is_frozen else "FRZ",
        bg=palette["warn"] if is_frozen else palette["title"],
        fg="#1b1327" if is_frozen else palette["title_text"],
    )
    sync_overlay_visibility(bool(current_sections))
    update_overlay(
        metrics_cache["pid"],
        metrics_cache["name"],
        metrics_cache["cpu"],
        metrics_cache["mem"],
        current_sections,
    )


def toggle_details():
    global is_expanded

    is_expanded = not is_expanded
    details_button.config(text="HIDE" if is_expanded else "MORE")
    sync_overlay_visibility(bool(current_sections))
    update_overlay(
        metrics_cache["pid"],
        metrics_cache["name"],
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
        if not is_dragging:
            return

        x = root._window_x + (event.x_root - root._drag_x)
        y = root._window_y + (event.y_root - root._drag_y)
        root.geometry(f"+{x}+{y}")

    def stop_drag(_event):
        global is_dragging

        is_dragging = False

    widget.bind("<Button-1>", start_drag)
    widget.bind("<B1-Motion>", drag)
    widget.bind("<ButtonRelease-1>", stop_drag)


def update_overlay(pid_text, name, cpu_text, mem_text, sections):
    global current_sections
    global details_visible
    global hud_visible
    global window_height

    current_sections = sections
    metrics_cache["pid"] = pid_text
    metrics_cache["name"] = name
    metrics_cache["cpu"] = cpu_text
    metrics_cache["mem"] = mem_text

    should_show_details = bool(sections) or is_expanded or is_frozen

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

    if sections:
      summary_text = f"{name} | PID {pid_text} | CPU {cpu_text} | RSS {mem_text} | {status_text}"
    else:
      summary_text = f"{name} | PID {pid_text} | CPU {cpu_text} | RSS {mem_text}"
    if summary_value.cget("text") != summary_text:
        summary_value.config(text=summary_text)


    is_compact_idle = not overlay_visible and not should_show_details

    if is_compact_idle:
        compact_title = f"{name} | PID {pid_text} | CPU {cpu_text} | RSS {mem_text}"
        if title_label.winfo_manager():
            title_label.pack_forget()
        if compact_value.cget("text") != compact_title:
            compact_value.config(text=compact_title)
        if not compact_value.winfo_manager():
            compact_value.pack(side="left", fill="x", expand=True, padx=(14, 8), pady=7)
        if hud_visible:
            hud_bar.pack_forget()
            hud_visible = False
    else:
        if compact_value.winfo_manager():
            compact_value.pack_forget()
        if not title_label.winfo_manager():
            title_label.pack(side="left", padx=12, pady=7)
        if not hud_visible:
            hud_bar.pack(fill="x", padx=10, pady=(10, 10))
            hud_visible = True

    if sections:
        lines = []
        lines.append(f"App: {name}")
        lines.append(f"Focus PID: {pid_text}")
        lines.append(f"App RSS Sum: {mem_text}")
        lines.append("")
        for title, severity, items in sections:
            lines.append(f"[{title}] {severity}")
            lines.extend(items[:5])
            lines.append("")
        issues_text = "\n".join(lines).rstrip()
        issues_color = palette["text"]
    else:
        issues_text = ""
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




##for design 


    if should_show_details:
        if not details_visible:
            issues_frame.pack(fill="both", expand=True, padx=8, pady=(6, 8))
            footer.pack(fill="x", padx=8, pady=(0, 8))
            details_visible = True

        if not is_dragging:
            line_count = issues_value.get("1.0", "end-1c").count("\n") + 1
            footer_lines = footer.cget("text").count("\n") + 1

            target_height = max(
                240,
                min(360, 112 + (min(line_count, 9) * 18) + (footer_lines * 14))
            )

            if window_height != target_height:
                root.geometry(f"520x{target_height}+{current_x}+{current_y}")
                window_height = target_height

    else:
        if details_visible:
            issues_frame.pack_forget()
            footer.pack_forget()
            details_visible = False

        if not is_dragging:
            target_height = 46 if is_compact_idle else 82

            if window_height != target_height:
                root.geometry(f"520x{target_height}+{current_x}+{current_y}")
                window_height = target_height



#removed not needed

# ---------------- ui


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
    "critical": "#ff6b6b",
}

metrics_cache = {"pid": "--", "name": "unknown", "cpu": "--", "mem": "--"}
current_sections = []

root.geometry("520x96+40+40")

panel = tk.Frame(
    root,
    bg=palette["chrome"],
    bd=3,
    relief="raised",
    highlightthickness=2,
    highlightbackground=palette["shadow"],
)
panel.pack(fill="both", expand=True)

title_bar = tk.Frame(panel, bg=palette["title"], height=34)
title_bar.pack(fill="x")
title_bar.pack_propagate(False)

title_label = tk.Label(
    title_bar,
    text="DEBUG HUD",
    bg=palette["title"],
    fg=palette["title_text"],
    font=("Helvetica", 11, "bold"),
)
title_label.pack(side="left", padx=12, pady=7)

compact_value = tk.Label(
    title_bar,
    text="PID -- | CPU -- | MEM --",
    justify="left",
    anchor="w",
    bg=palette["title"],
    fg="#c9d6ea",
    font=("Courier New", 10, "bold"),
)

status_value = tk.Label(
    title_bar,
    text="OK",
    bg=palette["title"],
    fg=palette["ok"],
    font=("Courier New", 10, "bold"),
)
status_value.pack(side="right", padx=12, pady=7)

body = tk.Frame(panel, bg=palette["panel"])
body.pack(fill="both", expand=True, padx=6, pady=6)

hud_bar = tk.Frame(
    body,
    bg=palette["panel_alt"],
    bd=2,
    relief="sunken",
    height=44,
)
hud_bar.pack(fill="x", padx=10, pady=(10, 10))
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
summary_value.pack(side="left", fill="x", expand=True, padx=14, pady=8)

button_bar = tk.Frame(hud_bar, bg=palette["panel_alt"])
button_bar.pack(side="right", padx=10, pady=6)

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
    pady=2,
)
freeze_button.pack(side="left", padx=(0, 6))

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
    pady=2,
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
    compact_value,
    status_value,
    hud_bar,
    summary_value,
    issues_frame,
    issues_header,
    issues_value,
    footer,
):
    make_draggable(draggable)
update_overlay("--", "unknown", "--", "--", [])



def update_loop():
    global prev_p, prev_t, prev_app_key, high_cpu_count, last_focus_pid, prev_net
    global last_log_check, log_alert_until
    global last_cpu, last_mem
    global overlay_visible
    global is_warming
    global last_alert_key, alert_hold_until

    if is_frozen:
        root.after(100, update_loop)
        return

    pid = get_active_pid()
    if pid is None:
        sync_overlay_visibility(False)
        update_overlay("--", "unknown", "--", "--", [])
        root.after(1000, update_loop)
        return

    focused_name = get_process_name(pid)
    if focused_name in IGNORE_PROCESSES:
        sync_overlay_visibility(False)
        update_overlay(str(pid or "--"), focused_name or "idle", "--", "--", [])
        root.after(1000, update_loop)
        return

    try:
        app_group = get_app_group(pid)
        app_key = app_group["app_key"]
        app_name = app_group["app_name"]
        focus_pid = app_group["focus_pid"]
        root_pid = app_group["root_pid"]
        group_pids = app_group["pids"]

        p, live_group_pids = get_group_process_time(group_pids)
        t = get_total_time()
        mem_kb = get_group_memory(live_group_pids)
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
                log_alert_until = now + 7  # keep visible for 7 seconds

            last_log_check = now

        log_alert = now < log_alert_until
       #fisnished log block
       
        if mem_kb is None:
            safe_log_error(f"memory usage is unavailable for app group rooted at PID {root_pid}")
            sync_overlay_visibility(False)
            update_overlay(str(focus_pid), app_name, "--", "--", [])
            root.after(1000, update_loop)
            return
        
        disk_percent = get_disk_usage()
        disk_alert = detect_disk_pressure(disk_percent)

        mem_mb = round(mem_kb / 1024)
        mem_text = format_memory_kb(mem_kb)
        
        crash_detected = False

        if last_focus_pid is not None and focus_pid != last_focus_pid:
          if not os.path.exists(f"/proc/{last_focus_pid}"):
            crash_detected = True

        if app_key != prev_app_key:
             prev_p = p
             prev_t = t
             prev_app_key = app_key

             mem_history.clear()
             mem_history.append(mem_mb)

             high_cpu_count = 0
             is_warming = True
             last_alert_key = None
             alert_hold_until = 0

             sync_overlay_visibility(False)
             update_overlay(str(focus_pid), app_name, "warming up", mem_text, [])
             root.after(1000, update_loop)
             return

        delta_p = p - prev_p
        delta_t = t - prev_t

        if delta_t > 0 and delta_p >= 0:
          cpu = round((delta_p / delta_t) * 100 * num_cpus)
        else:
          cpu = 0
        
        is_warming = (delta_t == 0)
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
            sections.append(("CPU WATCH", severity, cpu_insight(app_name, focus_pid, cpu, len(live_group_pids))))


        if mem_alert:
           sections.append(("MEM WATCH", "WARN", memory_insight(app_name, focus_pid, mem_text, len(live_group_pids))))

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
#this is the priority system for the sections
        sections.sort(key=lambda x: priority[x[1]], reverse=True)
        if any(s[1] != "INFO" for s in sections):
           sections = [s for s in sections if s[1] != "INFO"]
        sections = sections[:1]

        sections = dedupe_alert_sections(app_key, sections)
        sections = get_display_sections(sections)
        sync_overlay_visibility(bool(sections))

        
            
        
        if is_warming:
          sync_overlay_visibility(False)
          update_overlay(str(focus_pid), app_name, "warming up", mem_text, [])
        else:
          update_overlay(str(focus_pid), app_name, f"{cpu}%", mem_text, sections)

        prev_p = p
        prev_t = t
        prev_app_key = app_key
        last_focus_pid = focus_pid

        


    
    except (FileNotFoundError, ProcessLookupError, PermissionError, IndexError, ValueError) as exc:
        prev_p = None
        prev_t = None
        prev_app_key = None
        mem_history.clear()
        high_cpu_count = 0
        safe_log_error(f"process read failed for PID {pid}: {exc}")
        update_overlay(
            str(pid),
            "error",
            "--",
            "--",
            get_display_sections([("READ ERROR", "CRITICAL", [str(exc)])]),
        )

    root.after(1000, update_loop)

update_loop()
root.mainloop()
#more details button
# more memory usage detection

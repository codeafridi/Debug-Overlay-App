import os
import subprocess
import sys
import time
import tkinter as tk

num_cpus = os.cpu_count() or 1

# ---------------- STATE ----------------
high_cpu_count = 0
mem_history = []
diagnosis_hold_until = 0
last_sections = []
is_frozen = False
is_expanded = False

prev_p = None
prev_t = None
prev_pid = None
xdotool_warning_shown = False


def log_error(message):
    print(f"[overlay] {message}", file=sys.stderr)


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

    return high_cpu_count >= 3


def detect_memory_growth(history):
    if len(history) < 3:
        return False

    return history[0] < history[1] < history[2]


# ---------------- INSIGHTS ----------------

def cpu_insight():
    return [
        "CPU pressure detected",
        "Possible causes:",
        "  * heavy computation",
        "  * runaway loop",
        "  * too many repeated tasks",
        "Check next:",
        "  * hot loops or timers",
        "  * recent logic changes",
    ]


def memory_insight():
    return [
        "Memory is trending upward",
        "Possible causes:",
        "  * growing allocations",
        "  * unreleased resources",
        "  * cache buildup",
        "Check next:",
        "  * objects created in loops",
        "  * cleanup paths",
    ]


def build_issue_lines(cpu_alert, mem_alert):
    sections = []

    if cpu_alert:
        sections.append(("CPU WATCH", cpu_insight()))

    if mem_alert:
        sections.append(("MEM WATCH", memory_insight()))

    return sections


def get_display_sections(sections):
    global diagnosis_hold_until
    global last_sections

    now = time.time()

    if sections:
        diagnosis_hold_until = now + 3
        last_sections = sections
        return sections

    if now < diagnosis_hold_until:
        return last_sections

    last_sections = []
    return []


def toggle_freeze():
    global is_frozen

    is_frozen = not is_frozen
    freeze_button.config(
        text="RESUME" if is_frozen else "FREEZE",
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
    details_button.config(text="HIDE" if is_expanded else "DETAILS")
    update_overlay(
        metrics_cache["pid"],
        metrics_cache["cpu"],
        metrics_cache["mem"],
        current_sections,
    )


def make_draggable(widget):
    def start_drag(event):
        root._drag_x = event.x
        root._drag_y = event.y

    def drag(event):
        x = event.x_root - root._drag_x
        y = event.y_root - root._drag_y
        root.geometry(f"+{x}+{y}")

    widget.bind("<Button-1>", start_drag)
    widget.bind("<B1-Motion>", drag)


def update_overlay(pid_text, cpu_text, mem_text, sections):
    global current_sections

    current_sections = sections
    metrics_cache["pid"] = pid_text
    metrics_cache["cpu"] = cpu_text
    metrics_cache["mem"] = mem_text

    status_text = "STABLE"
    status_color = palette["ok"]

    if sections:
        status_text = "WATCH"
        status_color = palette["warn"]

    status_value.config(text=status_text, fg=status_color)
    summary_value.config(text=f"CPU {cpu_text}   MEM {mem_text}   PID {pid_text}")
    should_show_details = is_expanded or is_frozen or bool(sections)

    if sections:
        lines = []
        for title, items in sections:
            lines.append(f"[{title}]")
            lines.extend(items)
            lines.append("")
        issues_text = "\n".join(lines).rstrip()
        issues_color = palette["text"]
    else:
        issues_text = "No active alerts. System behavior looks normal."
        issues_color = palette["muted"]

    issues_value.config(text=issues_text, fg=issues_color)

    if should_show_details:
        issues_frame.pack(fill="both", expand=True, padx=8, pady=(6, 8))
        footer.pack(fill="x", padx=8, pady=(0, 8))
        line_count = issues_value.cget("text").count("\n") + 1
        target_height = max(180, min(300, 108 + (line_count * 18)))
        root.geometry(f"430x{target_height}+40+40")
    else:
        issues_frame.pack_forget()
        footer.pack_forget()
        root.geometry("430x92+40+40")


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

root.geometry("430x92+40+40")

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
    text=" DEBUG OVERLAY 95 ",
    bg=palette["title"],
    fg=palette["title_text"],
    font=("Helvetica", 10, "bold"),
)
title_label.pack(side="left", padx=8)

status_label = tk.Label(
    title_bar,
    text="STATUS",
    bg=palette["title"],
    fg=palette["muted"],
    font=("Helvetica", 8, "bold"),
)
status_label.pack(side="right", padx=(0, 6))

status_value = tk.Label(
    title_bar,
    text="BOOT",
    bg=palette["title"],
    fg=palette["ok"],
    font=("Courier New", 9, "bold"),
)
status_value.pack(side="right")

body = tk.Frame(panel, bg=palette["panel"])
body.pack(fill="both", expand=True, padx=4, pady=4)

hud_bar = tk.Frame(
    body,
    bg=palette["panel_alt"],
    bd=2,
    relief="sunken",
    height=34,
)
hud_bar.pack(fill="x", padx=8, pady=(8, 6))
hud_bar.pack_propagate(False)

summary_value = tk.Label(
    hud_bar,
    text="CPU --   MEM --   PID --",
    justify="left",
    anchor="w",
    bg=palette["panel_alt"],
    fg=palette["text"],
    font=("Courier New", 10, "bold"),
)
summary_value.pack(side="left", fill="x", expand=True, padx=8, pady=5)

button_bar = tk.Frame(hud_bar, bg=palette["panel_alt"])
button_bar.pack(side="right", padx=6, pady=3)

freeze_button = tk.Button(
    button_bar,
    text="FREEZE",
    command=toggle_freeze,
    bg=palette["title"],
    fg=palette["title_text"],
    activebackground=palette["warn"],
    activeforeground="#1b1327",
    relief="raised",
    bd=2,
    font=("Helvetica", 8, "bold"),
    padx=5,
    pady=0,
)
freeze_button.pack(side="left", padx=(0, 4))

details_button = tk.Button(
    button_bar,
    text="DETAILS",
    command=toggle_details,
    bg=palette["chrome"],
    fg="#111111",
    activebackground=palette["muted"],
    activeforeground="#111111",
    relief="raised",
    bd=2,
    font=("Helvetica", 8, "bold"),
    padx=5,
    pady=0,
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

issues_value = tk.Label(
    issues_frame,
    text="Booting monitor...",
    justify="left",
    anchor="nw",
    bg=palette["panel_alt"],
    fg=palette["text"],
    font=("Courier New", 10),
    wraplength=400,
)
issues_value.pack(fill="both", expand=True, padx=10, pady=(0, 10))

footer = tk.Label(
    body,
    text="Freeze to inspect. Details opens diagnostics.",
    anchor="w",
    bg=palette["panel"],
    fg="#8579a0",
    font=("Helvetica", 8),
)

make_draggable(title_bar)
update_overlay("--", "--", "--", [])


# ---------------- LOOP ----------------

while True:
    if is_frozen:
        root.update()
        time.sleep(0.1)
        continue

    pid = get_active_pid()

    if pid is None:
        update_overlay("--", "--", "--", [])
        root.update()
        time.sleep(1)
        continue

    try:
        p = get_process_time(pid)
        t = get_total_time()
        mem_kb = get_memory(pid)
        if mem_kb is None:
            log_error(f"memory usage is unavailable for PID {pid}")
            update_overlay(
                str(pid),
                "--",
                "--",
                [("MEM WATCH", ["Memory data is unavailable right now"])],
            )
            root.update()
            time.sleep(1)
            continue

        mem_mb = round(mem_kb / 1024)

        if pid != prev_pid:
            prev_p = p
            prev_t = t
            prev_pid = pid

            mem_history.clear()
            mem_history.append(mem_mb)

            high_cpu_count = 0

            update_overlay(str(pid), "warming up", f"{mem_mb} MB", [])
            root.update()
            time.sleep(1)
            continue

        delta_p = p - prev_p
        delta_t = t - prev_t
        cpu = round((delta_p / delta_t) * 100 * num_cpus) if delta_t > 0 else 0

        mem_history.append(mem_mb)
        if len(mem_history) > 3:
            mem_history.pop(0)

        cpu_alert = detect_high_cpu(cpu)
        mem_alert = detect_memory_growth(mem_history)
        sections = get_display_sections(build_issue_lines(cpu_alert, mem_alert))

        update_overlay(str(pid), f"{cpu}%", f"{mem_mb} MB", sections)

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
        update_overlay(str(pid), "--", "--", get_display_sections([("READ ERROR", [str(exc)])]))

    root.update()
    time.sleep(1)

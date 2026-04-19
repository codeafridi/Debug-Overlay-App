import time

pid = int(input("Enter PID: "))

def get_process_time(pid):
    with open(f"/proc/{pid}/stat") as f:
        values = f.read().split()
        utime = int(values[13])  # 14th field
        stime = int(values[14])  # 15th field
        return utime + stime

def get_total_time():
    with open("/proc/stat") as f:
        values = f.readline().split()[1:]
        return sum(map(int, values))

# snapshot 1
p1 = get_process_time(pid)
t1 = get_total_time()

time.sleep(1)

# snapshot 2
p2 = get_process_time(pid)
t2 = get_total_time()

delta_p = p2 - p1
delta_t = t2 - t1

cpu = (delta_p / delta_t) * 100

print(f"CPU Usage: {cpu:.2f}%")
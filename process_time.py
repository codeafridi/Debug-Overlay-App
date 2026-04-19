import time
import os
num_cpus = os.cpu_count()

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

def get_memory(pid):
    with open(f"/proc/{pid}/status") as f:
        for line in f:
            if line.startswith("VmRSS"):
                return int(line.split()[1])  # in KB

# snapshot 1
p1 = get_process_time(pid)
t1 = get_total_time()
mem = get_memory(pid)
time.sleep(1)

# snapshot 2
p2 = get_process_time(pid)
t2 = get_total_time()

delta_p = p2 - p1
delta_t = t2 - t1

cpu = (delta_p / delta_t) * 100 * num_cpus

print(f"CPU Usage: {cpu:.2f}%")
print(f"Memory: {mem / 1024:.2f} MB")   
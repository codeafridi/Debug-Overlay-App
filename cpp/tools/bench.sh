#!/usr/bin/env bash

set -uo pipefail

duration=20
interval=1
label="app"
pid=""
cmd=()

usage() {
  echo "Usage: $0 [--duration N] [--interval S] [--label NAME] [--pid PID] [-- CMD ARGS...]"
  echo "  Measures CPU%, wakeups/s, RSS, PSS, and thread count of a process."
  echo "  With --pid: measures an existing process."
  echo "  With -- CMD: launches CMD, warms up 3s, measures, then stops it."
}

while [ $# -gt 0 ]; do
  case "$1" in
    --duration) duration="$2"; shift 2 ;;
    --interval) interval="$2"; shift 2 ;;
    --label) label="$2"; shift 2 ;;
    --pid) pid="$2"; shift 2 ;;
    --) shift; cmd=("$@"); break ;;
    -h|--help) usage; exit 0 ;;
    *) echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

hz=$(getconf CLK_TCK)
launched=0

if [ -z "$pid" ]; then
  if [ ${#cmd[@]} -eq 0 ]; then
    echo "no --pid or command given" >&2
    usage >&2
    exit 2
  fi
  "${cmd[@]}" >/dev/null 2>&1 &
  pid=$!
  launched=1
  sleep 3
fi

read_stat() { awk '{print $14+$15}' "/proc/$pid/stat" 2>/dev/null || echo 0; }
read_rss() { awk '/VmRSS/{print $2}' "/proc/$pid/status" 2>/dev/null || echo 0; }
read_pss() { awk '/^Pss:/{print $2}' "/proc/$pid/smaps_rollup" 2>/dev/null || echo 0; }
read_threads() { awk '/^Threads:/{print $2}' "/proc/$pid/status" 2>/dev/null || echo 0; }
read_vcsw() { awk '/^voluntary_ctxt_switches:/{print $2}' "/proc/$pid/status" 2>/dev/null || echo 0; }
read_nvcsw() { awk '/^nonvoluntary_ctxt_switches:/{print $2}' "/proc/$pid/status" 2>/dev/null || echo 0; }

p0=$(read_stat); v0=$(read_vcsw); n0=$(read_nvcsw)
sleep "$duration"
p1=$(read_stat); v1=$(read_vcsw); n1=$(read_nvcsw)

rss=$(read_rss); pss=$(read_pss); threads=$(read_threads)

cpu=$(awk -v dp=$((p1 - p0)) -v hz="$hz" -v d="$duration" \
  'BEGIN{printf "%.3f", dp/hz/d*100}')
wake=$(awk -v dv=$((v1 - v0)) -v d="$duration" \
  'BEGIN{printf "%.2f", dv/d}')
nvwake=$(awk -v dv=$((n1 - n0)) -v d="$duration" \
  'BEGIN{printf "%.2f", dv/d}')

echo "label=$label pid=$pid duration_s=$duration"
echo "cpu_avg_pct=$cpu"
echo "voluntary_wakeups_per_s=$wake"
echo "nonvoluntary_wakeups_per_s=$nvwake"
echo "rss_kb=$rss"
echo "pss_kb=$pss"
echo "threads=$threads"

if [ "$launched" -eq 1 ]; then
  kill -TERM "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
fi

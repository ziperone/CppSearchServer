#!/usr/bin/env bash
set -euo pipefail

root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root_dir"

if ! command -v perf >/dev/null 2>&1; then
    echo "perf is not installed. Install linux-tools for the running kernel first." >&2
    exit 1
fi

port=${PORT:-18113}
workers=${WORKERS:-4}
io_loops=${IO_LOOPS:-4}
wrk_threads=${WRK_THREADS:-4}
connections=${CONNECTIONS:-256}
duration=${DURATION:-30}
frequency=${FREQUENCY:-199}
profile_build=${PROFILE_BUILD_DIR:-build-profile}
run_id=$(date +%Y%m%d-%H%M%S)
base="/tmp/cpp-search-perf-${run_id}"
server_pid=""
perf_pid=""

cleanup() {
    if [[ -n "$perf_pid" ]] && kill -0 "$perf_pid" 2>/dev/null; then
        kill "$perf_pid" 2>/dev/null || true
        wait "$perf_pid" 2>/dev/null || true
    fi
    if [[ -n "$server_pid" ]] && kill -0 "$server_pid" 2>/dev/null; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi
}

trap cleanup EXIT

cmake -S . -B "$profile_build" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCPP_SEARCH_ENABLE_PROFILING=ON
cmake --build "$profile_build" -j

"./${profile_build}/cpp_search_server" "$port" data/docs "$workers" "$io_loops" none off \
    >"${base}-server.log" 2>&1 &
server_pid=$!

for _ in $(seq 1 50); do
    if curl -fsS "http://127.0.0.1:${port}/search?q=epoll" >/dev/null; then
        break
    fi
    sleep 0.1
done

if ! kill -0 "$server_pid" 2>/dev/null; then
    cat "${base}-server.log" >&2
    exit 1
fi

perf record -F "$frequency" -g -p "$server_pid" -o "${base}.data" -- sleep "$duration" \
    >"${base}-perf.log" 2>&1 &
perf_pid=$!
sleep 0.2

if ! kill -0 "$perf_pid" 2>/dev/null; then
    wait "$perf_pid" || true
    perf_pid=""
    cat "${base}-perf.log" >&2
    echo "perf did not start. Inspect: cat /proc/sys/kernel/perf_event_paranoid" >&2
    exit 1
fi

wrk -t"$wrk_threads" -c"$connections" -d"${duration}s" --latency \
    "http://127.0.0.1:${port}/search?q=epoll" | tee "${base}-wrk.txt"
wait "$perf_pid"
perf_pid=""

perf report --stdio --no-children -i "${base}.data" >"${base}-report.txt"
echo "results: ${base}.data ${base}-report.txt ${base}-wrk.txt ${base}-server.log"
echo "if perf record reports permission denied, inspect: cat /proc/sys/kernel/perf_event_paranoid"

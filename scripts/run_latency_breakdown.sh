#!/usr/bin/env bash
set -euo pipefail

root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root_dir"

port=${PORT:-18112}
workers=${WORKERS:-4}
io_loops=${IO_LOOPS:-4}
wrk_threads=${WRK_THREADS:-4}
connections=${CONNECTIONS:-256}
duration=${DURATION:-30s}
run_id=$(date +%Y%m%d-%H%M%S)
base="/tmp/cpp-search-latency-${run_id}"
server_pid=""

cleanup() {
    if [[ -n "$server_pid" ]] && kill -0 "$server_pid" 2>/dev/null; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi
}

trap cleanup EXIT

./build-ralease/cpp_search_server "$port" data/docs "$workers" "$io_loops" none on \
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

wrk -t"$wrk_threads" -c"$connections" -d"$duration" --latency \
    "http://127.0.0.1:${port}/search?q=epoll" | tee "${base}-wrk.txt"

curl -fsS "http://127.0.0.1:${port}/metrics" | tee "${base}-metrics.json"
echo
echo "results: ${base}-wrk.txt ${base}-metrics.json ${base}-server.log"
echo "note: metrics mode adds per-request timestamps and atomic aggregation; use it to locate delay stages, not to compare QPS with the baseline benchmark."

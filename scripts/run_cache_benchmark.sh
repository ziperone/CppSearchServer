#!/usr/bin/env bash
set -euo pipefail

root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root_dir"

port=${PORT:-18110}
workers=${WORKERS:-4}
io_loops=${IO_LOOPS:-4}
wrk_threads=${WRK_THREADS:-4}
connections=${CONNECTIONS:-256}
duration=${DURATION:-30s}
run_id=$(date +%Y%m%d-%H%M%S)
server_pid=""

cleanup() {
    if [[ -n "$server_pid" ]] && kill -0 "$server_pid" 2>/dev/null; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi
}

trap cleanup EXIT

if (($# == 0)); then
    modes=(none l1 l1-redis)
else
    modes=("$@")
fi

for mode in "${modes[@]}"; do
    case "$mode" in
        none|l1|l1-redis) ;;
        *)
            echo "unsupported cache mode: $mode" >&2
            exit 2
            ;;
    esac

    base="/tmp/cpp-search-cache-${run_id}-${mode}"
    if [[ "$mode" == "l1-redis" ]]; then
        redis-cli DEL 'search-json:v1:top-k=10:q=epoll' >/dev/null
    fi

    redis-cli INFO commandstats >"${base}-redis-before.txt"
    ./build-ralease/cpp_search_server "$port" data/docs "$workers" "$io_loops" "$mode" \
        >"${base}-server.log" 2>&1 &
    server_pid=$!

    ready=0
    for _ in $(seq 1 50); do
        if curl -fsS "http://127.0.0.1:${port}/search?q=epoll" >/dev/null; then
            ready=1
            break
        fi
        sleep 0.1
    done
    if ((ready == 0)); then
        echo "server was not ready for mode=$mode" >&2
        cat "${base}-server.log" >&2
        exit 1
    fi

    if [[ "$mode" != "none" ]]; then
        curl -fsS "http://127.0.0.1:${port}/search?q=epoll" >/dev/null
    fi

    pidstat -u -r -d -p "$server_pid" 1 35 >"${base}-pidstat.txt" &
    monitor_pid=$!
    wrk -t"$wrk_threads" -c"$connections" -d"$duration" --latency \
        "http://127.0.0.1:${port}/search?q=epoll" | tee "${base}-wrk.txt"
    wait "$monitor_pid"

    redis-cli INFO commandstats >"${base}-redis-after.txt"
    cleanup
    server_pid=""

    echo "results for mode=$mode: ${base}-wrk.txt ${base}-pidstat.txt"
done

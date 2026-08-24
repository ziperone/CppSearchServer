#!/usr/bin/env bash
set -euo pipefail

root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root_dir"

port_a=${PORT_A:-18120}
port_b=${PORT_B:-18121}
workers=${WORKERS:-4}
io_loops=${IO_LOOPS:-4}
cache_key='search-json:v1:top-k=10:q=epoll'
pid_a=""
pid_b=""

cleanup() {
    for pid in "$pid_a" "$pid_b"; do
        if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
        fi
    done
}

trap cleanup EXIT

command_calls() {
    local command=$1
    redis-cli INFO commandstats | awk -F'[:,=]' -v name="cmdstat_${command}" '
        $1 == name { print $3; found = 1 }
        END { if (!found) print 0 }
    '
}

wait_ready() {
    local port=$1
    for _ in $(seq 1 50); do
        if curl -fsS "http://127.0.0.1:${port}/search?q=epoll" >/dev/null; then
            return 0
        fi
        sleep 0.1
    done
    return 1
}

start_instance() {
    local port=$1
    local log_file=$2
    ./build-ralease/cpp_search_server "$port" data/docs "$workers" "$io_loops" l1-redis \
        >"$log_file" 2>&1 &
    echo $!
}

redis-cli DEL "$cache_key" >/dev/null
get_before=$(command_calls get)
set_before=$(command_calls set)

pid_a=$(start_instance "$port_a" /tmp/cpp-search-redis-a.log)
wait_ready "$port_a"
curl -fsS "http://127.0.0.1:${port_a}/search?q=epoll" >/dev/null
get_after_a=$(command_calls get)
set_after_a=$(command_calls set)

pid_b=$(start_instance "$port_b" /tmp/cpp-search-redis-b.log)
wait_ready "$port_b"
curl -fsS "http://127.0.0.1:${port_b}/search?q=epoll" >/dev/null
get_after_b=$(command_calls get)
set_after_b=$(command_calls set)

echo "instance_a: GET +$((get_after_a - get_before)), SET +$((set_after_a - set_before))"
echo "instance_b: GET +$((get_after_b - get_after_a)), SET +$((set_after_b - set_after_a))"
echo "expected: instance A has at least one GET and one SET; instance B has GET but no SET"

#include "observability/RequestLatency.h"

#include <algorithm>
#include <sstream>

namespace observability {

RequestLatency::RequestLatency(bool enabled)
    : enabled_(enabled),
      completed_requests_(0),
      worker_queue_us_(0),
      worker_compute_us_(0),
      result_return_us_(0),
      response_write_us_(0),
      total_us_(0),
      max_total_us_(0) {}

bool RequestLatency::enabled() const {
    return enabled_;
}

std::shared_ptr<RequestTiming> RequestLatency::beginRequest() {
    if (!enabled_) {
        return nullptr;
    }

    auto timing = std::make_shared<RequestTiming>();
    timing->request_ready_at = RequestTiming::Clock::now();
    return timing;
}

void RequestLatency::markQueued(const std::shared_ptr<RequestTiming>& timing) {
    if (timing) {
        timing->queued_at = RequestTiming::Clock::now();
        timing->queued = true;
    }
}

void RequestLatency::markWorkerStarted(const std::shared_ptr<RequestTiming>& timing) {
    if (timing) {
        timing->worker_started_at = RequestTiming::Clock::now();
        timing->worker_started = true;
    }
}

void RequestLatency::markWorkerFinished(const std::shared_ptr<RequestTiming>& timing) {
    if (timing) {
        timing->worker_finished_at = RequestTiming::Clock::now();
        timing->worker_finished = true;
    }
}

void RequestLatency::markReturnedToIo(const std::shared_ptr<RequestTiming>& timing) {
    if (timing) {
        timing->returned_to_io_at = RequestTiming::Clock::now();
        timing->returned_to_io = true;
    }
}

void RequestLatency::markWriteStarted(const std::shared_ptr<RequestTiming>& timing) {
    if (timing && !timing->write_started) {
        timing->write_started = true;
        timing->write_started_at = RequestTiming::Clock::now();
    }
}

void RequestLatency::complete(const std::shared_ptr<RequestTiming>& timing) {
    if (!timing || !timing->queued || !timing->worker_started || !timing->worker_finished
        || !timing->returned_to_io || !timing->write_started) {
        return;
    }

    const RequestTiming::Clock::time_point completed_at = RequestTiming::Clock::now();
    const std::uint64_t worker_queue = elapsedMicros(timing->queued_at, timing->worker_started_at);
    const std::uint64_t worker_compute = elapsedMicros(timing->worker_started_at,
                                                        timing->worker_finished_at);
    const std::uint64_t result_return = elapsedMicros(timing->worker_finished_at,
                                                       timing->returned_to_io_at);
    const std::uint64_t response_write = elapsedMicros(timing->returned_to_io_at, completed_at);
    const std::uint64_t total = elapsedMicros(timing->request_ready_at, completed_at);

    worker_queue_us_.fetch_add(worker_queue, std::memory_order_relaxed);
    worker_compute_us_.fetch_add(worker_compute, std::memory_order_relaxed);
    result_return_us_.fetch_add(result_return, std::memory_order_relaxed);
    response_write_us_.fetch_add(response_write, std::memory_order_relaxed);
    total_us_.fetch_add(total, std::memory_order_relaxed);
    completed_requests_.fetch_add(1, std::memory_order_relaxed);
    updateMax(max_total_us_, total);
}

std::string RequestLatency::snapshotJson() const {
    const std::uint64_t completed = completed_requests_.load(std::memory_order_relaxed);
    const auto average = [completed](std::uint64_t total) {
        return completed == 0 ? 0ULL : total / completed;
    };

    std::ostringstream output;
    output << "{\"enabled\":" << (enabled_ ? "true" : "false")
           << ",\"completed_requests\":" << completed
           << ",\"avg_worker_queue_us\":"
           << average(worker_queue_us_.load(std::memory_order_relaxed))
           << ",\"avg_worker_compute_us\":"
           << average(worker_compute_us_.load(std::memory_order_relaxed))
           << ",\"avg_result_return_us\":"
           << average(result_return_us_.load(std::memory_order_relaxed))
           << ",\"avg_response_write_us\":"
           << average(response_write_us_.load(std::memory_order_relaxed))
           << ",\"avg_total_us\":"
           << average(total_us_.load(std::memory_order_relaxed))
           << ",\"max_total_us\":" << max_total_us_.load(std::memory_order_relaxed)
           << '}';
    return output.str();
}

std::uint64_t RequestLatency::elapsedMicros(RequestTiming::Clock::time_point start,
                                             RequestTiming::Clock::time_point end) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
}

void RequestLatency::updateMax(std::atomic<std::uint64_t>& maximum, std::uint64_t value) {
    std::uint64_t current = maximum.load(std::memory_order_relaxed);
    while (current < value
           && !maximum.compare_exchange_weak(current,
                                               value,
                                               std::memory_order_relaxed,
                                               std::memory_order_relaxed)) {
    }
}

}  // namespace observability

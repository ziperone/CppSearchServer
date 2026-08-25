#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace observability {

struct RequestTiming {
    using Clock = std::chrono::steady_clock;

    Clock::time_point request_ready_at;
    Clock::time_point queued_at;
    Clock::time_point worker_started_at;
    Clock::time_point worker_finished_at;
    Clock::time_point returned_to_io_at;
    Clock::time_point write_started_at;
    bool queued = false;
    bool worker_started = false;
    bool worker_finished = false;
    bool returned_to_io = false;
    bool write_started = false;
};

class RequestLatency {
public:
    explicit RequestLatency(bool enabled);

    bool enabled() const;
    std::shared_ptr<RequestTiming> beginRequest();
    void markQueued(const std::shared_ptr<RequestTiming>& timing);
    void markWorkerStarted(const std::shared_ptr<RequestTiming>& timing);
    void markWorkerFinished(const std::shared_ptr<RequestTiming>& timing);
    void markReturnedToIo(const std::shared_ptr<RequestTiming>& timing);
    void markWriteStarted(const std::shared_ptr<RequestTiming>& timing);
    void complete(const std::shared_ptr<RequestTiming>& timing);

    std::string snapshotJson() const;

private:
    static std::uint64_t elapsedMicros(RequestTiming::Clock::time_point start,
                                       RequestTiming::Clock::time_point end);
    static void updateMax(std::atomic<std::uint64_t>& maximum, std::uint64_t value);

    const bool enabled_;
    std::atomic<std::uint64_t> completed_requests_;
    std::atomic<std::uint64_t> worker_queue_us_;
    std::atomic<std::uint64_t> worker_compute_us_;
    std::atomic<std::uint64_t> result_return_us_;
    std::atomic<std::uint64_t> response_write_us_;
    std::atomic<std::uint64_t> total_us_;
    std::atomic<std::uint64_t> max_total_us_;
};

}  // namespace observability

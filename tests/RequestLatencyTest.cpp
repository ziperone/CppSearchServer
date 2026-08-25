#include "observability/RequestLatency.h"

#include <iostream>
#include <string>

int main() {
    observability::RequestLatency disabled(false);
    if (disabled.beginRequest()) {
        std::cerr << "disabled metrics unexpectedly created timing data\n";
        return 1;
    }

    observability::RequestLatency metrics(true);
    auto timing = metrics.beginRequest();
    metrics.markQueued(timing);
    metrics.markWorkerStarted(timing);
    metrics.markWorkerFinished(timing);
    metrics.markReturnedToIo(timing);
    metrics.markWriteStarted(timing);
    metrics.complete(timing);

    const std::string snapshot = metrics.snapshotJson();
    if (snapshot.find("\"enabled\":true") == std::string::npos
        || snapshot.find("\"completed_requests\":1") == std::string::npos) {
        std::cerr << "unexpected metrics snapshot: " << snapshot << '\n';
        return 1;
    }

    std::cout << "Request latency test passed\n";
    return 0;
}

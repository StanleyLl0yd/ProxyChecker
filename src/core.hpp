#pragma once

#include <algorithm>
#include <chrono>
#include <optional>

namespace proxychecker {

enum class Status { Unknown, Up, Suspect, Down };

inline const wchar_t* StatusName(Status status) noexcept {
    switch (status) {
    case Status::Up: return L"UP";
    case Status::Suspect: return L"SUSPECT";
    case Status::Down: return L"DOWN";
    default: return L"UNKNOWN";
    }
}

struct Transition {
    Status before;
    Status after;
    bool changed() const noexcept { return before != after; }
};

class StateTracker {
public:
    using Clock = std::chrono::steady_clock;
    using Time = Clock::time_point;

    explicit StateTracker(int downSeconds = 90, int recoverySuccesses = 2)
        : downPeriod_(std::chrono::seconds(std::clamp(downSeconds, 10, 3600))),
          recoveryCount_(std::clamp(recoverySuccesses, 1, 10)) {}

    Transition Probe(bool success, Time now) noexcept {
        const Status before = status_;

        if (success) {
            failureStart_.reset();
            if (status_ == Status::Up) {
                successes_ = recoveryCount_;
            } else if (++successes_ >= recoveryCount_) {
                status_ = Status::Up;
            }
        } else {
            successes_ = 0;
            if (!failureStart_) failureStart_ = now;
            // An outage remains DOWN until recovery has been confirmed.
            if (status_ == Status::Down || now - *failureStart_ >= downPeriod_) {
                status_ = Status::Down;
            } else {
                status_ = Status::Suspect;
            }
        }
        return {before, status_};
    }

    Status state() const noexcept { return status_; }
    void Reset() noexcept {
        status_ = Status::Unknown;
        successes_ = 0;
        failureStart_.reset();
    }
private:
    Status status_ = Status::Unknown;
    int successes_ = 0;
    std::optional<Time> failureStart_;
    std::chrono::seconds downPeriod_;
    int recoveryCount_;
};

inline bool ShouldSendHeartbeat(Status state, bool enabled, bool hasUrl,
    StateTracker::Time now, StateTracker::Time last, int intervalSeconds) noexcept {
    if (state != Status::Up || !enabled || !hasUrl) return false;
    if (last == StateTracker::Time::min()) return true;
    return now - last >= std::chrono::seconds(std::clamp(intervalSeconds, 30, 3600));
}

}  // namespace proxychecker

#include "../src/core.hpp"
#include <cassert>
#include <chrono>
#include <iostream>

using namespace proxychecker;
using namespace std::chrono_literals;

int main() {
    const auto t = StateTracker::Clock::now();
    StateTracker s(90, 2);
    assert(s.state() == Status::Unknown);
    assert(s.Probe(true, t).after == Status::Unknown);
    assert(s.Probe(true, t + 10s).after == Status::Up);
    assert(s.Probe(false, t + 20s).after == Status::Suspect);
    assert(s.Probe(true, t + 30s).after == Status::Suspect);
    assert(s.Probe(true, t + 40s).after == Status::Up);
    assert(s.Probe(false, t + 50s).after == Status::Suspect);
    assert(s.Probe(false, t + 100s).after == Status::Suspect);
    assert(s.Probe(false, t + 139s).after == Status::Suspect);
    assert(s.Probe(false, t + 140s).after == Status::Down);
    assert(s.Probe(false, t + 200s).after == Status::Down);
    assert(s.Probe(true, t + 210s).after == Status::Down);
    assert(s.Probe(true, t + 220s).after == Status::Up);
    s.Reset();
    assert(s.state() == Status::Unknown);

    StateTracker firstFailure(90, 2);
    assert(firstFailure.Probe(false, t).after == Status::Suspect);
    assert(firstFailure.Probe(false, t + 90s).after == Status::Down);
    assert(firstFailure.Probe(true, t + 100s).after == Status::Down);
    assert(firstFailure.Probe(false, t + 110s).after == Status::Suspect);
    // Recovery success streak broken; a new 90-second failure window is required.
    assert(firstFailure.Probe(false, t + 199s).after == Status::Suspect);
    assert(firstFailure.Probe(false, t + 200s).after == Status::Down);

    std::cout << "state tests passed\n";
}

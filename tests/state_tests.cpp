#include "../src/core.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>

using namespace proxychecker;
using namespace std::chrono_literals;

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

int main() {
    const auto t = StateTracker::Clock::now();
    StateTracker s(90, 2);
    Require(s.state() == Status::Unknown, "initial state");
    Require(s.Probe(true, t).after == Status::Unknown, "one success");
    Require(s.Probe(true, t + 10s).after == Status::Up, "two successes");
    Require(s.Probe(false, t + 20s).after == Status::Suspect, "first failure");
    Require(s.Probe(true, t + 30s).after == Status::Suspect, "one recovery");
    Require(s.Probe(true, t + 40s).after == Status::Up, "confirmed recovery");
    Require(s.Probe(false, t + 50s).after == Status::Suspect, "new failure");
    Require(s.Probe(false, t + 100s).after == Status::Suspect, "failure at 50s");
    Require(s.Probe(false, t + 139s).after == Status::Suspect, "failure at 89s");
    Require(s.Probe(false, t + 140s).after == Status::Down, "confirmed down");
    Require(s.Probe(false, t + 200s).after == Status::Down, "stay down");
    Require(s.Probe(true, t + 210s).after == Status::Down, "one success while down");
    Require(s.Probe(false, t + 215s).after == Status::Down, "interrupted recovery stays down");
    Require(s.Probe(true, t + 220s).after == Status::Down, "recovery first");
    Require(s.Probe(true, t + 230s).after == Status::Up, "recovery second");
    s.Reset();
    Require(s.state() == Status::Unknown, "reset");

    StateTracker firstFailure(90, 2);
    Require(firstFailure.Probe(false, t).after == Status::Suspect, "unknown first failure");
    Require(firstFailure.Probe(false, t + 90s).after == Status::Down, "unknown down");

    const auto never = StateTracker::Time::min();
    Require(!ShouldSendHeartbeat(Status::Unknown, true, true, t, never, 60), "unknown no ping");
    Require(!ShouldSendHeartbeat(Status::Suspect, true, true, t, never, 60), "suspect no ping");
    Require(!ShouldSendHeartbeat(Status::Down, true, true, t, never, 60), "down no ping");
    Require(!ShouldSendHeartbeat(Status::Up, false, true, t, never, 60), "disabled no ping");
    Require(!ShouldSendHeartbeat(Status::Up, true, false, t, never, 60), "no URL no ping");
    Require(ShouldSendHeartbeat(Status::Up, true, true, t, never, 60), "initial ping");
    Require(!ShouldSendHeartbeat(Status::Up, true, true, t + 59s, t, 60), "no premature ping");
    Require(ShouldSendHeartbeat(Status::Up, true, true, t + 60s, t, 60), "due heartbeat");
    std::cout << "state and heartbeat tests passed\n";
}

#pragma once
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <string>

// A test's checks, each counted, a failure said on stderr after the test's name.  `summary` is the last line.
struct TestChecks {
    const char* name;
    int count = 0, failed = 0;
    void operator()(bool ok, const std::string& what) {
        ++count;
        if (ok) return;
        ++failed;
        std::fprintf(stderr, "%s: %s\n", name, what.c_str());
    }
    int summary(const std::string& also = {}) const {   // `also`, said after the count
        std::printf("%s: %d/%d checks hold%s%s\n", name, count - failed, count, also.empty() ? "" : "; ", also.c_str());
        return failed ? 1 : 0;
    }
    // What `run` gives, if it comes back within `limit`.  If it doesn't, `what` is said as a failure and the program
    // exits 1 at once: a call that doesn't come back can't be stopped, and its future would wait for it.
    template <class Run>
    auto within(std::chrono::seconds limit, const std::string& what, Run run) {
        auto result = std::async(std::launch::async, run);
        if (result.wait_for(limit) != std::future_status::ready) {
            std::fprintf(stderr, "%s: %s\n", name, what.c_str());
            std::fflush(stderr);
            std::_Exit(1);
        }
        return result.get();
    }
};

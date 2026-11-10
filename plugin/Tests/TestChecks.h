#pragma once
#include <cstdio>
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
    int summary() const {
        std::printf("%s: %d/%d checks hold\n", name, count - failed, count);
        return failed ? 1 : 0;
    }
};

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
    int summary(const std::string& also = {}) const {   // `also`, said after the count
        std::printf("%s: %d/%d checks hold%s%s\n", name, count - failed, count, also.empty() ? "" : "; ", also.c_str());
        return failed ? 1 : 0;
    }
};

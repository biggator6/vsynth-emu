"""fwsteps_count.py <Sh4Interp.cpp> <out.cpp>: a copy of the interpreter whose Sh4::call tells fwsteps.cpp's
fwstepsCall the steps and seconds of each call, for fwsteps.sh.  The source is read, never written."""
import pathlib
import sys

LIMIT = "    const uint64_t limit = nsteps + maxSteps;\n"
AFTER = "    std::fesetround(savedRound);\n    hostRound = -1;\n    if (r0out) *r0out = r[0];\n"
HEAD = "#include <chrono>\n#include <cstdint>\nvoid fwstepsCall(std::uint64_t steps, double seconds);\n"
START = "    const uint64_t fwstepsFrom = nsteps;\n    const auto fwstepsT0 = std::chrono::steady_clock::now();\n"
END = ("    fwstepsCall(nsteps - fwstepsFrom,\n"
       "                std::chrono::duration<double>(std::chrono::steady_clock::now() - fwstepsT0).count());\n")


def main():
    src, out = sys.argv[1], sys.argv[2]
    text = pathlib.Path(src).read_text()
    for part in (LIMIT, AFTER):
        if text.count(part) != 1:
            sys.exit(f"fwsteps_count: {src} no longer has its call's limit and end as this expects")
    text = HEAD + text.replace(LIMIT, LIMIT + START).replace(AFTER, END + AFTER)
    pathlib.Path(out).write_text(text)


if __name__ == "__main__":
    main()

// rd <rom> <addr> <words>: rom.2 words after the interpreter's C startup (.data copied, .bss zeroed),
// e.g. the RAM tables of routine pointers.  Built by build.sh.
#include "Sh4Interp.h"
#include <cstdio>
#include <cstdlib>
int main(int argc, char** argv) {
    Sh4 c; if (!c.loadRom(argv[1])) return 1;
    uint32_t a = (uint32_t)strtoul(argv[2], 0, 16); int n = atoi(argv[3]);
    for (int i = 0; i < n; ++i) printf("%08x %08x\n", a + 4 * i, c.rd32(a + 4 * i));
}

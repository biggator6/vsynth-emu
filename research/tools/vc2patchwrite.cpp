// vc2patchwrite <rom.4> <out.p02> [ID=value[@zone] ...]: a VC-2 patch file made by rom.4's own routines, for the
// captures' runs (vc2capture.py; research/VC2_MODE_PLAN.md, step 5, the user's "Patch files"): rom.4's default patch
// (what its reader lays under a file with no data: v450's vc2PatchRead on a header alone), each value set by rom.4's
// setters (vc2PatchSet) in the order given, the store written by its writer (vc2PatchWrite; both v505, held by
// xt_vc2save).  An ID is hex (0x194) or decimal, a value as rom.4's read gives it (the descriptor's value less its
// centre), a zone 1…8 (1 if none).  The file is read back before it is kept, each value as it was set, or it is
// refused.  The firmware is the user's, read at run time; the file it writes holds rom.4's defaults, so it stays out
// of git.  build.sh builds it into build/tools/bin/, beside the harnesses.
#include "Vc2Params.h"
#include "Vc2PatchFile.h"
#include "Vc2PatchSave.h"
#include "Vc2PatchStore.h"
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

// [FW] a .p02's two headers as rom.4's writer (0x893411b0) makes them, with no data: 00 1e, VC20, Rev008; PATa, VC_2,
// the data's place 0x20 and its length 0, both big-endian
std::vector<uint8_t> headersAlone() {
    std::vector<uint8_t> f(32, 0);
    const std::string tags[] = { "VC20", "Rev008", "PATa", "VC_2" };
    const size_t at[] = { 2, 6, 0x10, 0x14 };
    f[1] = 0x1e;
    for (size_t k = 0; k < 4; ++k) std::copy(tags[k].begin(), tags[k].end(), f.begin() + (long)at[k]);
    f[0x1b] = 0x20;
    return f;
}

// "ID=value[@zone]": false when it isn't one
bool parseSet(const char* text, XtPatchSet& s) {
    char* end = nullptr;
    s.id = (int)std::strtol(text, &end, 0);
    if (!end || *end != '=') return false;
    s.value = (int)std::strtol(end + 1, &end, 0);
    s.zone = 0;
    if (end && *end == '@') s.zone = (int)std::strtol(end + 1, &end, 0) - 1;
    return end && *end == 0 && s.zone >= 0;
}

int failed(const std::string& what) {
    std::fprintf(stderr, "vc2patchwrite: %s\n", what.c_str());
    return 1;
}

// Whether the file, read back by v450's reader, gives each value as it was set (by the port's reads, xt_vc2patch's):
// a value past its range, which rom.4's setter holds to it, is said so
bool readsBack(const std::vector<uint8_t>& file, const std::vector<XtPatchSet>& sets, std::string& why) {
    Vc2PatchFile back;
    if (!vc2PatchRead(file.data(), file.size(), back, &why)) return false;
    for (const XtPatchSet& s : sets) {
        const int read = s.id >= kVc2FirstTyped ? vc2StoreTyped(back.store, s.id, s.zone, s.cosm)
                                                : xtStoreValue(vc2Store(), back.store, s.id, s.zone, s.cosm);
        if (read == s.value) continue;
        why = "ID " + std::to_string(s.id) + " reads back " + std::to_string(read) + ", not " + std::to_string(s.value);
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) return failed("usage: vc2patchwrite <rom.4> <out.p02> [ID=value[@zone] ...]");
    if (!vc2LoadParams(argv[1]) || !vc2LoadPatchFiles(argv[1])) return failed("rom.4's tables can't be read");
    std::vector<XtPatchSet> sets;
    for (int k = 3; k < argc; ++k) {
        XtPatchSet s;
        if (!parseSet(argv[k], s)) return failed(std::string("not ID=value[@zone]: ") + argv[k]);
        sets.push_back(s);
    }
    const std::vector<uint8_t> none = headersAlone();
    Vc2PatchFile patch;
    std::vector<uint8_t> file;
    std::string why;
    if (!vc2PatchRead(none.data(), none.size(), patch, &why)) return failed("rom.4's default patch: " + why);
    if (!vc2PatchSet(argv[1], patch.store, sets, &why)) return failed(why);
    if (!vc2PatchWrite(argv[1], patch.store, file, &why)) return failed(why);
    if (!readsBack(file, sets, why)) return failed(std::string(argv[2]) + ": " + why);
    FILE* out = std::fopen(argv[2], "wb");
    if (!out) return failed(std::string("can't write ") + argv[2]);
    const bool all = std::fwrite(file.data(), 1, file.size(), out) == file.size();
    return std::fclose(out) == 0 && all ? 0 : failed(std::string("couldn't write all of ") + argv[2]);
}

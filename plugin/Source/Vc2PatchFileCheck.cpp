// xt_vc2patch's files (v450; Vc2PatchCheck.h): each of the disk image's patches through vc2PatchRead against rom.4's
// routines on its data straight (the default patch 0x89325300, laid out as a file 0x89324b10, the data over it, the
// conversion 0x89324b20), and its name against rom.4's own reading of it (0x89320810); then copies of the first made
// otherwise, each against what rom.4's reader (0x893419a0) takes of it by its code: which it refuses, and which it reads,
// from where and how much.  XT_VC2PATCH_MAIN only.
#ifdef XT_VC2PATCH_MAIN
#include "Vc2PatchCheck.h"
#include "Vc2PatchFile.h"
#include "Vc2PatchStore.h"
#include <algorithm>

namespace {

constexpr uint32_t kDefaultPatch = 0x89325300, kAsFile = 0x89324b10, kToStore = 0x89324b20;   // [FW]
constexpr uint32_t kAimCommon = 0x893207b0;   // [FW] (obj, part): the common's place in part's store
constexpr uint32_t kNameOf = 0x89320810;      // [FW] (obj, to): Patch Name 1-12, then a 0
constexpr uint32_t kName = 0x8C048000;
constexpr size_t kDataAt = 0x18, kDataBytes = 0x1c, kHeaders = 0x20, kTag = 2, kRev = 6, kPat = 0x10, kKind = 0x14;
constexpr size_t kNameChars = 12;

uint32_t be32(const std::vector<uint8_t>& f, size_t at) {
    return (uint32_t)f[at] << 24 | (uint32_t)f[at + 1] << 16 | (uint32_t)f[at + 2] << 8 | f[at + 3];
}
void setBe32(std::vector<uint8_t>& f, size_t at, uint32_t v) {
    for (int k = 0; k < 4; ++k) f[at + (size_t)k] = (uint8_t)(v >> (24 - 8 * k));
}

// rom.4's routines on `bytes` of data straight: the store they leave; empty on a fault
std::vector<uint8_t> straight(Vc2Fw& fw, const uint8_t* data, size_t bytes) {
    if (!fw.call(kDefaultPatch, { 1 }) || !fw.call(kAsFile, { kVc2FileImage })) return {};
    if (bytes) fw.cpu.write(kVc2FileImage, data, bytes);
    if (!fw.call(kToStore, { kVc2FileStore, kVc2FileImage })) return {};
    std::vector<uint8_t> store(kVc2StoreBytes);
    fw.cpu.read(kVc2FileStore, store.data(), store.size());
    return store;
}

// rom.4's reading of the name of the store at A, its spaces at the end gone
std::string romName(Vc2Fw& fw, const std::vector<uint8_t>& store) {
    fw.cpu.write(kVc2StoreA, store.data(), store.size());
    if (!fw.call(kAimCommon, { kVc2Object, 0 }) || !fw.call(kNameOf, { kVc2Object, kName })) return "(it faults)";
    std::string name;
    for (size_t k = 0; k < kNameChars; ++k) name += (char)fw.cpu.rd8(kName + (uint32_t)k);
    while (!name.empty() && name.back() == ' ') name.pop_back();
    return name;
}

// A file, and what rom.4's reader takes of it: refused, or its data from `at`, `bytes` of it; upgraded, as a Rev006 or
// Rev007 file is, which may write the store's upgradable bytes
struct Case {
    std::string what;
    std::vector<uint8_t> file;
    bool read;
    size_t at, bytes;
    bool upgraded = false;
};

// [FW] the store's bytes the reader's upgrades may write (0x89341bda…0x89341c8e, 0x89341ce2…0x89341d3c): zone 2's tone
// and envelope blocks (its 0x8932b1d0 calls on the file's +0xe6 + 78 and +0x356 + 112: TVA Switch, Env 13's Attack and
// Velocity Sens), and wherever the conversion takes the file's bytes 0x1370 and 0x1371 (Rev006's), by running it
std::vector<bool> upgradable(Vc2Fw& fw) {
    std::vector<bool> may(kVc2StoreBytes);
    for (const int first : { 0x2d3, 0x390 }) {   // the tone's first ID and the envelopes'
        const int from = xtStoreSection(vc2Store(), first, 1), to = xtStoreSection(vc2Store(), first, 2);
        std::fill(may.begin() + from, may.begin() + to, true);
    }
    std::vector<uint8_t> marked(kVc2StoreBytes), store(kVc2StoreBytes);
    marked[0x1370] = marked[0x1371] = 0xff;
    fw.cpu.write(kVc2FileImage, marked.data(), marked.size());
    if (!fw.call(kToStore, { kVc2FileStore, kVc2FileImage })) return {};
    fw.cpu.read(kVc2FileStore, store.data(), store.size());
    for (size_t k = 0; k < store.size(); ++k) may[k] = may[k] || store[k] != 0;
    return may;
}

bool sameBut(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, const std::vector<bool>& may) {
    if (a.size() != b.size() || may.size() != a.size()) return false;
    for (size_t k = 0; k < a.size(); ++k)
        if (a[k] != b[k] && !may[k]) return false;
    return true;
}

// The file through vc2PatchRead against what the case says of it
void judge(Vc2Fw& fw, const Case& c, const std::vector<bool>& may, Vc2PatchTally& t) {
    ++t.compared;
    Vc2PatchFile mine;
    const bool read = vc2PatchRead(c.file.data(), c.file.size(), mine);
    if (read != c.read)
        return t.differs(c.what + (read ? ": read, where rom.4 refuses it" : ": refused, where rom.4 reads it"));
    if (!read) return;
    const std::vector<uint8_t> want = straight(fw, c.file.data() + c.at, c.bytes);
    if (c.upgraded ? !sameBut(mine.store, want, may) : mine.store != want) return t.differs(c.what + ": another store");
    if (const std::string name = romName(fw, mine.store); mine.name != name)
        t.differs(c.what + ": named \"" + mine.name + "\", rom.4's \"" + name + "\"");
}

Case as(const std::vector<uint8_t>& file, const char* what, bool read) {
    return { what, file, read, be32(file, kDataAt), std::min<size_t>(be32(file, kDataBytes), kVc2StoreBytes) };
}
Case tagged(const std::vector<uint8_t>& file, size_t at, const char* tag, const char* what, bool read) {
    std::vector<uint8_t> f = file;
    std::copy(tag, tag + std::strlen(tag) + (at == kRev ? 1 : 0), f.begin() + (long)at);
    return as(f, what, read);
}

// [FW] 0x893419a0's tests, in its order: a V-Synth patch's tag, the first word, VC20, PATa, VC_2, the data's place, the
// version (Rev000-Rev005 refused, Rev006 and Rev007 upgraded, any other read), the data read whole up to the store's
std::vector<Case> madeOtherwise(const std::vector<uint8_t>& file) {
    std::vector<Case> out = {
        tagged(file, kTag, "SVD0", "a V-Synth patch's tag", false),
        tagged(file, kTag, "VC21", "VC21 for VC20", false),
        tagged(file, kPat, "PATb", "PATb for PATa", false),
        tagged(file, kKind, "VC_3", "VC_3 for VC_2", false),
        tagged(file, kRev, "Rev009", "Rev009, a later version", true),
    };
    for (const char* rev : { "Rev006", "Rev007" }) {
        out.push_back(tagged(file, kRev, rev, "an upgraded version", true));
        out.back().upgraded = true;
    }
    static const char* const kOld[] = { "Rev000", "Rev001", "Rev002", "Rev003", "Rev004", "Rev005" };
    for (const char* rev : kOld) out.push_back(tagged(file, kRev, rev, "an older version", false));
    std::vector<uint8_t> f = file;
    f[1] = 0x1f;
    out.push_back(as(f, "a first word of 0x1f", false));
    f = file;
    setBe32(f, kDataBytes, 0x1000);
    out.push_back(as(f, "a data length of 0x1000", true));
    f = file;
    setBe32(f, kDataBytes, 0x2000);
    f.resize(kHeaders + 0x2000, 0xa5);
    out.push_back(as(f, "a data length past the store's", true));
    f = file;
    f.resize(kHeaders + 0x1000);
    out.push_back(as(f, "the data cut short of its length", false));
    f = file;
    f.insert(f.begin() + kHeaders, kHeaders, 0x5a);
    setBe32(f, kDataAt, 2 * kHeaders);
    out.push_back(as(f, "the data 32 bytes further on", true));
    f = file;
    setBe32(f, kDataAt, 0x10000);
    setBe32(f, kDataBytes, 0);
    out.push_back(as(f, "no data, its place past the end", true));
    f.assign(file.begin(), file.begin() + kHeaders);
    out.push_back(as(f, "the headers alone", false));
    f.assign(file.begin(), file.begin() + kHeaders - 1);
    out.push_back({ "a byte short of the headers", f, false, 0, 0 });
    return out;
}

} // namespace

long vc2PatchFiles(Vc2Fw& fw, const std::vector<Vc2PatchOnDisk>& files) {
    Vc2PatchTally t { "files" };
    const std::vector<bool> may = upgradable(fw);
    for (const Vc2PatchOnDisk& f : files) judge(fw, as(f.bytes, f.name.c_str(), true), may, t);
    const long onDisk = t.compared;
    if (!files.empty())
        for (const Case& c : madeOtherwise(files.front().bytes)) judge(fw, c, may, t);
    return t.say("read, " + std::to_string(onDisk) + " of them the disk image's, the rest made otherwise");
}
#endif

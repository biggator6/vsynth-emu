#pragma once
// xt_vc2waves's parts (v500): the VC-2's wave loaders against rom.4's own.  The files both sides' file calls are
// answered from (rom.4's through hooks on its four calls, the port's through Vc2Files), each side's calls logged and
// compared, each call with what the side's watch sees; and each loader's part: the index's parse
// (Vc2WaveIndexCheck.cpp).  XT_VC2WAVES_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2Files.h"
#include "Vc2WaveBoot.h"
#include "Vc2WavePoolRig.h"   // the pool's rig, which the pool's and the records' parts share (v502)
#include <functional>
#include <string>
#include <vector>

inline constexpr uint64_t kVc2LoadSteps = 200000000;   // a loader's call: a parse of 500 records a set runs about 9e6

// A file call as either side made it: open ('o', its path), read ('r', the count asked), seek ('s', the offset and
// whence) or close ('c'), its handle and what it answered, and what the side's watch saw as it was made (the loaders'
// progress, which the XT's display reads while they run)
struct Vc2FileCall {
    char op = 0;
    int32_t handle = 0;
    uint32_t a = 0, b = 0;
    int32_t answer = 0;
    std::string path;
    uint64_t watched = 0;
    bool operator==(const Vc2FileCall&) const = default;
};

// Files by path, answering the calls as the XT's OS does as far as the loaders reach it: an open by its path (a file's
// place in the list from 1, -1 for none) at its first byte, a read up to its end, a seek from its start (whence 0) or
// from where it is (the new place), a close; every call logged
class Vc2FileList : public Vc2Files {
public:
    struct File {
        std::string path;
        std::vector<uint8_t> bytes;
        size_t at = 0;
    };
    std::vector<File> files;
    std::vector<Vc2FileCall> calls;
    std::function<uint64_t()> watch;   // what each call records beside it, when set
    int32_t open(const std::string& path) override;
    uint32_t read(int32_t handle, uint8_t* into, uint32_t n) override;
    int32_t seek(int32_t handle, int32_t offset, int32_t whence) override;
    void close(int32_t handle) override;
private:
    File* of(int32_t handle);
    void log(Vc2FileCall call);
};

// rom.4's four file calls (0x89336c60, 0x89336cf0, 0x89336db0, 0x89336ca0) answered by `files`, which must outlive
// the machine's runs
void vc2HookFiles(Vc2Fw& fw, Vc2FileList& files);
// The first call that differs between the port's log and rom.4's, as text; empty when none does
std::string vc2FileCallsDifference(const std::vector<Vc2FileCall>& mine, const std::vector<Vc2FileCall>& theirs);
// A call made on both sides, as text: that rom.4's faulted (not `ok`, on its machine `c`), that the answers differ
// (the port's, rom.4's), or else what `memory()` finds different; empty when nothing does
std::string vc2CallOutcome(Sh4& c, bool ok, std::pair<int64_t, int64_t> answers,
                           const std::function<std::string()>& memory);
// The first byte of `bytes` at `at` in rom.4's memory (its machine's, `c`) that differs from the port's `mine`, as
// text; empty when none does
std::string vc2BytesDifference(Sh4& c, uint32_t at, const void* mine, size_t bytes, const char* what);

// A part's tally: what it ran, and the cases that differ, the first said under XT_VC2WAVES_FIRST and the run stopped
// there under XT_VC2WAVES_STOP
struct Vc2WavesTally {
    const char* what;
    long runs = 0, different = 0;
    bool stopped = false;
    void outcome(const std::string& difference, const std::string& where);
    long say(const std::string& how) const;
};

// The index's parse: the disk image's index (`index`, empty when there is none), then `run.cases` cases of drawn
// indexes, one to three parses each on the sets the last left
long vc2WaveIndexPart(Vc2Fw& fw, const Vc2Run& run, const std::vector<uint8_t>& index);
// The store's pool (v501, Vc2WavePoolCheck.cpp): `run.cases` cases of a set-up and up to 160 drawn calls on a small pool
long vc2WavePoolPart(Vc2Fw& fw, const Vc2Run& run);
// The store's records and chunks (v502, Vc2WaveRecordsCheck.cpp): `run.cases` cases of up to 120 drawn calls on a store
long vc2WaveRecordsPart(Vc2Fw& fw, const Vc2Run& run);
// The waves loaded by rom.4 itself (v503, Vc2WaveBootCheck.cpp): Vc2WaveBoot on `files`, what it left against them and
// against rom.4's memory, the lookups, and the load again after every static initializer
long vc2WaveBootPart(const std::string& rom4, const Vc2Run& run, const Vc2DiskBootFiles& files);
// The port's lookups on `w`, what the load on `c` left, against rom.4's on `c` (Vc2WaveLookupCheck.cpp), each call an
// outcome in `tally`; how many calls
long vc2WaveLookups(Sh4& c, Vc2LoadedWaves& w, const Vc2Run& run, Vc2WavesTally& tally);
// Its draws (Vc2WavePoolDraws.cpp): a request's size (up to an eighth or a sixty-fourth of the pool; a fourth of the
// time one of three sizes, so free blocks tie for the largest; now and then 0, exactly what's free, or past it); a
// pool's (2 KB to the most the check's OS gives, now and then 0, more, or negative); and a realloc's that fills
// `block` exactly, the block and its free next joined or a larger free block elsewhere, when the size law can ask for
// it (else a request's size)
uint32_t vc2PoolDrawBytes(std::mt19937& rng, const Vc2WavePool& pool);
uint32_t vc2PoolDrawSize(std::mt19937& rng);
uint32_t vc2PoolFittingBytes(std::mt19937& rng, const Vc2WavePool& pool, uint32_t block);

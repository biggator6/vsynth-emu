// xt_files' check of a plain .wav imported and encoded as the XT does (v488, v490; XtFiles.h), XT_FILES_MAIN only.  Each
// plain file beside a project's Wave folder (the user's raw files, which the XT imported) put in the store by the XT's
// loader and written, then encoded SOLO by its Encode EXECUTE and written: each of the projects' waves with its
// samples that the XT made from it (its LITE imports, its SOLO encodes) must be the file written, but for what the XT's
// own run left that this one can't know (the import's two unwritten vpsc bytes; the app's version in an encode's LIST).
#ifdef XT_FILES_MAIN
#include "XtFwWaveFile.h"
#include <algorithm>
#include <cstdio>
#include <string_view>

namespace {
namespace fs = std::filesystem;
// [FW] the vpsc's bytes the import never writes ([0xc], [0xd]: whatever the store's heap held, 0x894a27c0)
constexpr size_t kUnwrittenAt = 0xc, kUnwrittenEnd = 0xe;
// [FW] the encode's LIST: "V-Synth Ver." and the app's version, 4 bytes (0x894ab554): this rom.2's "0.20", the XT's of
// the project's time "2.01" ([HW], {#wave-encode-read})
constexpr std::string_view kSoftware = "V-Synth Ver.";
constexpr size_t kVersionBytes = 4;
// [HW] the depth the user's XT encoded 003 and 004 at isn't in their files; any keeping no attack gives their 2 events
constexpr int kSoloDepth = 64;
constexpr int kLite = 0, kSolo = 1, kBacking = 3, kEnsemble = 5;

struct ImportTally {
    int files = 0, imported = 0, encoded = 0, lite = 0, liteSame = 0, solo = 0, soloSame = 0;
    int added = 0, addedBack = 0;   // an event added halfway: one more; added and deleted again: as with none
    int bands = 0, bandsCut = 0, bandsBack = 0;   // cut then band-encoded (v492): as asked; saved again the same
    int bandsMost = 0;                            // and of those not, the most a sample moved
};

// A wave cut to its second quarter by rom.2's Truncate, then encoded BACKING and ENSEMBLE by its Encode EXECUTE, as
// the XT's screens make them in that order (v492; no file of the XT's to hold them to): each of that type with the
// kept frames; and saved again from what was written, the same (a band wave's samples come out of its bands, which a
// load makes again from them: Synth006's a step off, v483)
void cutsThenBands(const std::vector<uint8_t>& raw, long frames, ImportTally& t, const fs::path& path, bool first) {
    const XtWaveCut cut { frames / 4, frames / 2 };
    for (const int type : { kBacking, kEnsemble }) {
        std::vector<uint8_t> file, again;
        std::string why;
        XtWaveFile read;
        ++t.bands;
        const XtWaveEdits edits { cut, std::nullopt, XtWaveEncode { type, kSoloDepth, {} } };
        if (!xtFwWaveFile(raw, file, &why, edits) || !xtWaveRead(file.data(), file.size(), read)) {
            if (first) std::printf("%s: not cut and encoded %d: %s\n", path.filename().c_str(), type, why.c_str());
            continue;
        }
        t.bandsCut += read.encodeType == type && read.frames == cut.to - cut.from + 1;
        const bool back = xtFwWaveFile(file, again) && again == file;
        t.bandsBack += back;
        if (back || again.empty()) continue;
        const XtFileDiff d = xtFileDiff(again, file);
        t.bandsMost = std::max(t.bandsMost, d.most);
        if (first)
            std::printf("%s: cut, encoded %d, saved again: %zu bytes differ, words by up to %d\n",
                        path.filename().c_str(), type, d.bytes, d.most);
    }
}

// [FW] the vpno's count of events, at +4 (0x89515b40)
constexpr size_t kVpnoCountAt = 4;

uint32_t eventsOf(const std::vector<uint8_t>& file) {
    XtWaveBlocks w;
    if (!xtWaveBlocksOf(file.data(), file.size(), w) || !w.bytes[XtWaveBlocks::kVpno]) return 0;
    const auto& vpno = *w.bytes[XtWaveBlocks::kVpno];
    if (vpno.size() < kVpnoCountAt + 4) return 0;
    return vpno[kVpnoCountAt] | vpno[kVpnoCountAt + 1] << 8 | vpno[kVpnoCountAt + 2] << 16 |
           (uint32_t)vpno[kVpnoCountAt + 3] << 24;
}

// The encode's event edits (v490): an event added halfway by the Encode screen's add gives one more event, and the
// same added then deleted the file the encode gives with none
void editsEvents(const std::vector<uint8_t>& raw, const std::vector<uint8_t>& encoded, ImportTally& t) {
    XtWaveFile read;
    if (!xtWaveRead(encoded.data(), encoded.size(), read)) return;
    const auto half = (uint32_t)(read.frames / 2);
    std::vector<uint8_t> added, back;
    const XtWaveEncode add { kSolo, kSoloDepth, { { true, half } } };
    const XtWaveEncode addDelete { kSolo, kSoloDepth, { { true, half }, { false, half } } };
    ++t.added;
    t.addedBack += xtFwWaveFile(raw, added, nullptr, XtWaveEdits { std::nullopt, std::nullopt, add }) &&
                   eventsOf(added) == eventsOf(encoded) + 1 &&
                   xtFwWaveFile(raw, back, nullptr, XtWaveEdits { std::nullopt, std::nullopt, addDelete }) &&
                   back == encoded;
}

// What the XT's own run left that this one can't know, copied from its file `w` into `got`: the import's unwritten
// vpsc bytes, an encode's version in its LIST
void asTheXtLeftIt(XtWaveBlocks& got, const XtWaveBlocks& w, int type) {
    auto& vpsc = *got.bytes[XtWaveBlocks::kVpsc];
    const auto& theirs = *w.bytes[XtWaveBlocks::kVpsc];
    if (type == kLite)
        std::copy_n(theirs.begin() + kUnwrittenAt, kUnwrittenEnd - kUnwrittenAt, vpsc.begin() + kUnwrittenAt);
    auto& list = got.bytes[XtWaveBlocks::kList];
    const auto& theirList = w.bytes[XtWaveBlocks::kList];
    if (type == kLite || !list || !theirList || list->size() != theirList->size()) return;
    const auto at = std::search(list->begin(), list->end(), kSoftware.begin(), kSoftware.end());
    if (at == list->end() || list->end() - at < (long)(kSoftware.size() + kVersionBytes)) return;
    const long from = (at - list->begin()) + (long)kSoftware.size();
    std::copy_n(theirList->begin() + from, kVersionBytes, list->begin() + from);
}

// The file written held to every project wave of `type` with its samples; `same` and `of` counted
void holdTo(const std::vector<uint8_t>& file, int type, const std::vector<fs::path>& waves, int& of, int& same) {
    XtWaveBlocks got;
    if (!xtWaveBlocksOf(file.data(), file.size(), got)) return;
    for (const fs::path& p : waves) {
        const std::vector<uint8_t> bytes = xtFileBytes(p);
        XtWaveBlocks w;
        if (!xtWaveBlocksOf(bytes.data(), bytes.size(), w) || (*w.bytes[XtWaveBlocks::kVpsc])[1] != type) continue;
        if (w.bytes[XtWaveBlocks::kData] != got.bytes[XtWaveBlocks::kData]) continue;
        ++of;
        XtWaveBlocks as = got;
        asTheXtLeftIt(as, w, type);
        same += as.bytes == w.bytes && file.size() == bytes.size();
    }
}

void importOne(const fs::path& raw, const std::vector<fs::path>& waves, ImportTally& t, bool first) {
    ++t.files;
    const std::vector<uint8_t> bytes = xtFileBytes(raw);
    std::vector<uint8_t> file;
    std::string why;
    if (XtWaveFile imported; xtFwWaveFile(bytes, file, &why) && xtWaveRead(file.data(), file.size(), imported)) {
        ++t.imported;
        holdTo(file, kLite, waves, t.lite, t.liteSame);
        cutsThenBands(bytes, imported.frames, t, raw, first);
    } else if (first) {
        std::printf("%s: not imported: %s\n", raw.filename().c_str(), why.c_str());
    }
    const XtWaveEdits solo { std::nullopt, std::nullopt, XtWaveEncode { kSolo, kSoloDepth, {} } };
    if (xtFwWaveFile(bytes, file, &why, solo)) {
        ++t.encoded;
        holdTo(file, kSolo, waves, t.solo, t.soloSame);
        editsEvents(bytes, file, t);
    } else if (first) {
        std::printf("%s: not encoded: %s\n", raw.filename().c_str(), why.c_str());
    }
}
} // namespace

int xtWaveImportCheck(const std::string& rom, bool first) {
    ImportTally t;
    const std::vector<fs::path> waves = xtProjectWaves(rom, "Wave");
    for (const fs::path& p : xtProjectWaves(rom, ".")) importOne(p, waves, t, first);
    std::printf("waves imported: %d plain files beside the projects' waves, %d imported by the XT's loader, written as "
                "the projects' %d LITE imports of them: %d the same but the 2 bytes the import never writes; %d "
                "encoded SOLO by the XT's Encode, as its %d SOLO encodes of them: %d the same but the app's version; "
                "an event added halfway on %d, one more and added then deleted as none on %d; cut then encoded "
                "BACKING and ENSEMBLE %d times, %d of the type with the kept frames, %d saved again the same, the "
                "others' samples moved by up to %d\n",
                t.files, t.imported, t.lite, t.liteSame, t.encoded, t.solo, t.soloSame, t.added, t.addedBack, t.bands,
                t.bandsCut, t.bandsBack, t.bandsMost);
    return t.lite - t.liteSame + t.solo - t.soloSame + t.added - t.addedBack + t.bands - t.bandsCut;
}
#endif

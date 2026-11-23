// The kernel's part of rom.4's boot load (Vc2WaveBootKernel.h, v503).
#include "Vc2WaveBootKernel.h"
#include <algorithm>
#include <cctype>
#include <limits>

namespace {
using Kernel = Vc2WaveBoot::Kernel;
// −1 for no file or entry; the mode the loaders open a file in, [FW] 0 (0x89347f58, 0x8934d120); a page of memory
constexpr uint32_t kNone = 0xffffffffu, kReading = 0, kPage = 0x1000;
constexpr size_t kPathMost = 260, kNameMost = 255, kShortMost = 12;
// [FW] a folder's entry's place, name's length, name and 8.3 name; the stats' size and attributes
constexpr uint32_t kPlaceAt = 0, kLengthAt = 8, kNameAt = 10, kShortAt = 0x10a, kSizeAt = 0, kAttributesAt = 8;

// A kernel entry the load answers: its routine, or with none the word it gives
struct Way {
    uint32_t at;
    void (Kernel::*routine)(Sh4&);
    uint32_t gives;
};
// [FW] each entry by rom.4's stub that reaches it
constexpr Way kWays[] = {
    { 0x88008000, nullptr, 0 },                   // the OS's lock (0x893018b0: the store's pool's)
    { 0x88008010, nullptr, 0 },                   // its unlock (0x893018c0)
    { 0x88008110, nullptr, 0 },                   // the allocator's lock (0x89301810)
    { 0x88008120, nullptr, 0 },                   // its unlock (0x89301820)
    { 0x88008550, nullptr, 0 },                   // the boot's key test (0x894da150)
    { 0x88008a90, nullptr, 0 },                   // the display's text (0x8933a890)
    { 0x880089b0, &Kernel::compareSome, 0 },      // (0x8933a7b0)
    { 0x880089e0, &Kernel::compare, 0 },          // (0x8933a7e0)
    { 0x880089f0, &Kernel::compareSome, 0 },      // (0x8933a7f0)
    { 0x88008c90, &Kernel::fileName, 0 },         // (0x89338c60)
    { 0x88008cf0, nullptr, 1 },                   // the drive's queries (0x89338e50, 0x89338e90, 0x89338ed0,
    { 0x88008d00, nullptr, 0 },                   // 0x89338f10, 0x89339040)
    { 0x88008d10, nullptr, 1 },
    { 0x88008d20, nullptr, 1 },
    { 0x88008da0, nullptr, 0 },
    { 0x88008dd0, &Kernel::statEntry, 0 },        // (0x89339120)
    { 0x88008f00, &Kernel::openFolder, 0 },       // (0x893369c0)
    { 0x88008f10, &Kernel::closeFolder, 0 },      // (0x893369f0)
    { 0x88008f20, &Kernel::readFolder, 0 },       // (0x89336a40)
    { 0x88008f30, &Kernel::rewindFolder, 0 },     // (0x89336a90)
    { 0x88008f40, &Kernel::seekFolder, 0 },       // (0x89336ae0)
    { 0x88008f70, &Kernel::fstat, 0 },            // (0x89336c10)
    { 0x88008f80, &Kernel::open, 0 },             // (0x89336c60)
    { 0x88008fa0, &Kernel::close, 0 },            // (0x89336ca0)
    { 0x88008fb0, &Kernel::read, 0 },             // (0x89336cf0)
    { 0x88008fc0, &Kernel::mapped, 0 },           // (0x89336d60)
    { 0x88008fe0, &Kernel::seek, 0 },             // (0x89336db0)
};

std::string text(Sh4& c, uint32_t at, size_t most) {
    std::string s;
    for (char ch = (char)c.rd8(at); ch != 0 && s.size() < most; ch = (char)c.rd8(++at)) s += ch;
    return s;
}

// Two names compared without case over at most `most` characters, as strncasecmp gives it
int32_t caseless(const std::string& a, const std::string& b, size_t most) {
    for (size_t i = 0; i < most; ++i) {
        const int x = i < a.size() ? std::tolower((unsigned char)a[i]) : 0;
        const int y = i < b.size() ? std::tolower((unsigned char)b[i]) : 0;
        if (x != y || x == 0) return x - y;
    }
    return 0;
}

// The open file or folder r4 names, given to `use`; `none` given back when r4 names none
template <class Open, class Use>
void withOpen(Open& open, Sh4& c, uint32_t none, Use use) {
    const auto f = open.find(c.r[4]);
    if (f == open.end()) return c.ret(none);
    use(f->second);
}

// A file or folder found, kept open under the next handle, which is given back; `failed` when none was found
template <class Open, class Item>
void keep(Open& open, uint32_t& next, Sh4& c, bool found, Item item, uint32_t failed) {
    if (!found) return c.ret(failed);
    open[next] = std::move(item);
    c.ret(next++);
}

// r4's file or folder closed
template <class Open>
void shut(Open& open, Sh4& c) {
    open.erase(c.r[4]);
    c.ret(0);
}

// r4's folder's next entry the one at `place`
template <class Open>
void placeAt(Open& open, Sh4& c, uint32_t place) {
    withOpen(open, c, 0, [&c, place](auto& f) {
        f.at = place;
        c.ret(0);
    });
}
} // namespace

void Kernel::serve(Sh4& c) {
    for (const Way& w : kWays)
        c.hook(w.at, [this, w](Sh4& s) {
            if (w.routine) (this->*w.routine)(s);
            else s.ret(w.gives);
        });
}

bool Kernel::answers(uint32_t at) {
    return std::ranges::any_of(kWays, [at](const Way& w) { return w.at == at; });
}

void Kernel::open(Sh4& c) {
    const std::string path = text(c, c.r[4], kPathMost);
    if (c.r[5] != kReading) refused = "a write to " + path;
    File f;
    const bool found = c.r[5] == kReading && files.file(path, f.bytes);
    keep(openFiles, nextFile, c, found, std::move(f), kNone);
}

void Kernel::close(Sh4& c) { shut(openFiles, c); }

void Kernel::read(Sh4& c) {
    withOpen(openFiles, c, kNone, [&c](File& file) {
        const size_t from = std::min<size_t>(file.at, file.bytes.size());
        const auto n = (uint32_t)std::min<size_t>(c.r[6], file.bytes.size() - from);
        c.write(c.r[5], file.bytes.data() + from, n);
        file.at = (uint32_t)from + n;
        c.ret(n);
    });
}

void Kernel::seek(Sh4& c) {   // whence 0 from the start, 1 from here, 2 from the end; any other refused
    withOpen(openFiles, c, kNone, [&c](File& file) {
        const uint32_t from[] = { 0, file.at, (uint32_t)file.bytes.size(), file.at };
        file.at = from[std::min<uint32_t>(c.r[6], 3)] + (c.r[6] > 2 ? 0 : c.r[5]);
        c.ret(c.r[6] > 2 ? kNone : 0);
    });
}

void Kernel::fstat(Sh4& c) {
    withOpen(openFiles, c, kNone, [&c](File& file) {
        c.wr32(c.r[5] + kSizeAt, (uint32_t)file.bytes.size());
        c.ret(0);
    });
}

void Kernel::mapped(Sh4& c) {
    withOpen(openFiles, c, 0, [this, &c](File& file) {
        // one-off: a file's bytes put where rom.4 reads it as memory, at a place the kernel gives, not a machine's set-up
        if (file.mapped == 0) {
            const auto room = (uint32_t)((file.bytes.size() + kPage) & ~(size_t)(kPage - 1));
            c.mapRam(nextMapped, room);
            c.write(nextMapped, file.bytes.data(), file.bytes.size());
            file.mapped = nextMapped;
            nextMapped += room;
        }
        c.ret(file.mapped);
    });
}

void Kernel::openFolder(Sh4& c) {
    Folder f;
    const bool found = files.folder(text(c, c.r[4], kPathMost), f.entries);
    keep(folders, nextFolder, c, found, std::move(f), 0);
}

void Kernel::closeFolder(Sh4& c) { shut(folders, c); }

void Kernel::readFolder(Sh4& c) {
    withOpen(folders, c, 0, [&c](Folder& f) {
        if (f.at >= f.entries.size()) return c.ret(0);
        const Vc2BootEntry& e = f.entries[f.at];
        for (uint32_t k = 0; k < kEntryBytes; ++k) c.wr8(kEntryAt + k, 0);
        c.wr32(kEntryAt + kPlaceAt, f.at++);
        const std::string name = e.name.substr(0, kNameMost), shortName = e.shortName.substr(0, kShortMost);
        c.wr16(kEntryAt + kLengthAt, (uint16_t)name.size());
        c.write(kEntryAt + kNameAt, name.c_str(), name.size() + 1);
        c.write(kEntryAt + kShortAt, shortName.c_str(), shortName.size() + 1);
        c.ret(kEntryAt);
    });
}

void Kernel::rewindFolder(Sh4& c) { placeAt(folders, c, 0); }

void Kernel::seekFolder(Sh4& c) { placeAt(folders, c, c.r[5]); }

void Kernel::statEntry(Sh4& c) {
    withOpen(folders, c, kNone, [&c](Folder& f) {
        if (c.r[5] >= f.entries.size()) return c.ret(kNone);
        const Vc2BootEntry& e = f.entries[c.r[5]];
        c.wr32(c.r[6] + kSizeAt, e.size);
        c.wr8(c.r[6] + kAttributesAt, e.attributes);
        c.ret(0);
    });
}

void Kernel::compare(Sh4& c) {
    c.ret((uint32_t)caseless(text(c, c.r[4], kPathMost), text(c, c.r[5], kPathMost),
                             std::numeric_limits<size_t>::max()));
}

void Kernel::compareSome(Sh4& c) {
    c.ret((uint32_t)caseless(text(c, c.r[4], kPathMost), text(c, c.r[5], kPathMost), c.r[6]));
}

void Kernel::fileName(Sh4& c) {
    const std::string path = text(c, c.r[4], kPathMost);
    const size_t slash = path.rfind('\\');
    c.ret(c.r[4] + (uint32_t)(slash == std::string::npos ? 0 : slash + 1));
}

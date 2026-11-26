// vc2card's card and its kernel answers: see vc2cardfiles.h.  Every address is rom.4's.
#include "vc2cardfiles.h"
#include "Vc2WaveBoot.h"
#include <cctype>
#include <cstdio>
#include <cstring>

namespace vc2card {

namespace {

// [FW] the kernel's line read (0x89338ca0's: handle, to, most): the project's parse (0x89342cb0) compares each read
// with a whole line, "[current project]\r\n" first, so it gives a line at a time
constexpr uint32_t kLineRead = 0x88008cb0;
// [FW] the kernel's name of an open handle (0x89338d60's; rom.4's own Z: drive gives its object's, 0x8933ce10), and
// where the run puts it, in the boot's RAM below its stack
constexpr uint32_t kHandleName = 0x88008cd0, kNameAt = 0x8C0A2000;
// [FW] the seek wrapper's return (0x89336db0: handle r14, place r12, whence r13 kept): the boot's kernel answers a
// seek with 0 (its wave loaders never test it), but rom.4's patch reader takes a file only when the seek answers the
// place it sought from the start (0x893419a0; v450's reads of real files)
constexpr uint32_t kSeekReturn = 0x89336e0a, kFromStart = 0;
constexpr uint32_t kMappedFrom = 0x90000000;

std::string upper(std::string s) {
    for (char& ch : s) ch = (char)std::toupper((unsigned char)ch);
    return s;
}

} // namespace

void CardFiles::put(const std::string& path, std::vector<uint8_t> bytes) {
    const size_t slash = path.rfind('\\');
    const std::string name = path.substr(slash + 1), folder = path.substr(0, slash + 1);
    folders_[upper(folder)].push_back({name, name.substr(0, 8), 0, (uint32_t)bytes.size()});
    files_[upper(path)] = std::move(bytes);
}

void CardFiles::putFolder(const std::string& path) {
    folders_[upper(path)];
    const std::string parent = path.substr(0, path.rfind('\\', path.size() - 2) + 1);
    const std::string name = path.substr(parent.size(), path.size() - parent.size() - 1);
    folders_[upper(parent)].push_back({name, name.substr(0, 8), 0x10, 0});
}

bool CardFiles::file(const std::string& path, std::vector<uint8_t>& out) const {
    asked.push_back("file " + path);
    const auto it = files_.find(upper(path));
    if (it == files_.end()) return false;
    out = it->second;
    openedFiles.push_back(path);
    return true;
}

bool CardFiles::folder(const std::string& path, std::vector<Vc2BootEntry>& out) const {
    asked.push_back("folder " + path);
    const std::string key = upper(path.empty() || path.back() == '\\' ? path : path + "\\");
    const auto it = folders_.find(key);
    if (it == folders_.end()) return false;
    out = it->second;
    openedFolders.push_back(path);
    return true;
}

uint32_t Lines::next(Sh4& s, uint32_t to, uint32_t most) {
    size_t n = 0;
    while (at + n < text.size() && n < most && (n == 0 || text[at + n - 1] != '\n')) ++n;
    s.write(to, text.data() + at, n);
    at += n;
    return (uint32_t)n;
}

std::string heldText(Sh4& c, uint32_t at) {
    char buffer[kPathMost + 1] = {};
    c.read(at, buffer, kPathMost);
    return std::string(buffer, strnlen(buffer, kPathMost));
}

void answerTheRest(Sh4& c, std::vector<std::string>& reached, Lines& lines) {
    c.hook(kLineRead, [&lines](Sh4& s) { s.ret(lines.next(s, s.r[5], s.r[6])); });
    for (uint32_t pc = kImageFrom; pc + 2 < c.image().dataFrom; pc += 2) {
        if (c.rd16(pc) >> 12 != 0xd) continue;
        const uint32_t literal = c.rd32(((pc + 4) & ~3u) + 4 * (c.rd16(pc) & 0xff));
        if (literal < kKernelFrom || literal >= kKernelTo || literal == kLineRead || literal == kHandleName
            || Vc2WaveBoot::answers(literal))
            continue;
        c.hook(literal, [&reached, literal](Sh4& s) {
            char line[kPathMost + 16];
            const bool text = s.r[4] >= kImageFrom && s.r[4] < kMappedFrom;
            std::snprintf(line, sizeof line, "%08x(%s)", literal, text ? heldText(s, s.r[4]).c_str() : "");
            reached.push_back(line);
            s.ret(0);
        });
    }
}

void answerHandleName(Sh4& c, const CardFiles& files, std::vector<std::string>& reached) {
    c.hook(kHandleName, [&files, &reached](Sh4& s) {
        const uint32_t h = s.r[4];
        const std::string folder = h >= 1 && h <= files.openedFolders.size() ? files.openedFolders[h - 1] : "";
        const std::string file = h >= 1 && h <= files.openedFiles.size() ? files.openedFiles[h - 1] : "";
        reached.push_back("88008cd0(handle " + std::to_string(h) + ": folder " + folder + ", file " + file + ")");
        s.write(kNameAt, folder.c_str(), folder.size() + 1);
        s.ret(kNameAt);
    });
}

void answerSeekPlace(Sh4& c) {
    c.hook(kSeekReturn, [](Sh4& s) {
        if (s.r[13] == kFromStart && s.r[0] == 0) s.r[0] = s.r[12];
    });
}

} // namespace vc2card

#pragma once
// vc2card's made-up card and the kernel calls its run answers past Vc2WaveBoot's own (vc2cardkernel.cpp).  The card
// is files and folders by rom.4's paths, matched without case; the run's answers are the kernel's line read, a handle's
// name and a seek's place, which a patch's load reaches and the wave loaders don't, each said where it's made.
#include "Sh4Interp.h"
#include "Vc2BootFiles.h"
#include <map>
#include <string>
#include <vector>

namespace vc2card {

constexpr uint32_t kKernelFrom = 0x88000000, kKernelTo = 0x89000000, kImageFrom = 0x89300000;
constexpr size_t kPathMost = 256;

// A card's files and folders by rom.4's paths, without case; what rom.4 asked for, and what opened, in order
class CardFiles : public Vc2BootFiles {
public:
    void put(const std::string& path, std::vector<uint8_t> bytes);
    void putFolder(const std::string& path);   // with its '\' at the end
    bool file(const std::string& path, std::vector<uint8_t>& out) const override;
    bool folder(const std::string& path, std::vector<Vc2BootEntry>& out) const override;
    mutable std::vector<std::string> asked;
    // The boot's kernel numbers each kind's handles from 1 as opens succeed (Vc2WaveBootKernel), so these name them
    mutable std::vector<std::string> openedFiles, openedFolders;

private:
    std::map<std::string, std::vector<uint8_t>> files_;
    std::map<std::string, std::vector<Vc2BootEntry>> folders_;
};

// The ini's text, a line a call, as the kernel's line read gives the only file read so
struct Lines {
    std::string text;
    size_t at = 0;
    uint32_t next(Sh4& s, uint32_t to, uint32_t most);
};

// The text rom.4 holds at `at`, read whole and cut at its 0
std::string heldText(Sh4& c, uint32_t at);

// Every kernel entry rom.4's stubs reach that the boot's table doesn't answer: 0, each call named with its first
// argument's text where it is one (a path); the line read from `lines`, and the two below
void answerTheRest(Sh4& c, std::vector<std::string>& reached, Lines& lines);
// A handle's name from the card's opens (a folder's: the listings ask it of their folder), logged with both kinds'
void answerHandleName(Sh4& c, const CardFiles& files, std::vector<std::string>& reached);
// A seek from the start answers its place (the boot's kernel answers 0)
void answerSeekPlace(Sh4& c);

} // namespace vc2card

// The firmware's disk image (XtFiles.h): its FAT16 partition's folders and files, and the preset waves after it.
// Moved whole from XtFiles.cpp in v177, by the split rule.
#include "XtFiles.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

using xtfile::le32;

bool XtDiskImage::open(const std::string& path) {
    d.clear();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) {
        d.resize((size_t)n);
        if (std::fread(d.data(), 1, d.size(), f) != d.size()) d.clear();
    }
    std::fclose(f);
    if (d.size() < 0x10000) { d.clear(); return false; }
    const auto u16 = [this](size_t at) { return (uint32_t)(d[at] | d[at + 1] << 8); };
    const auto u32 = [this](size_t at) { return le32(&d[at]); };
    const size_t part = 512 * (size_t)u32(446 + 8), boot = part;
    if (boot + 64 > d.size() || u16(boot + 11) != 512 || std::memcmp(&d[boot + 54], "FAT16", 5)) {
        d.clear();
        return false;
    }
    partitionEnd = part + 512 * (size_t)u32(446 + 12);
    const size_t fatBytes = 512 * (size_t)u16(boot + 22);
    fat = boot + 512 * (size_t)u16(boot + 14);
    root = fat + fatBytes * d[boot + 16];
    rootBytes = 32 * u16(boot + 17);
    data = root + rootBytes;
    cluster = 512u * d[boot + 13];
    // Damaged (v177): clusters of no sectors, or a root directory running past the image's end, and so its FATs,
    // which come before it.  Read, the first never ended a chain and the second read past the image.
    if (cluster == 0 || data > d.size()) {
        d.clear();
        return false;
    }
    // A cluster's number below this has its entry in the FAT and its data in the image
    clusterEnd = (uint32_t)std::min({ (d.size() - data) / cluster + 2, std::min(fatBytes, d.size() - fat) / 2,
                                      size_t { 0xfff8 } });
    return true;
}

// Through the FAT from cluster `c`, to its end mark or `limit` bytes.  A cluster past clusterEnd ends it, and so does
// one it has been through (v177): a FAT that loops gives each of its clusters once.
std::vector<uint8_t> XtDiskImage::chain(uint32_t c, uint32_t limit) const {
    std::vector<uint8_t> out;
    std::vector<bool> seen(clusterEnd);
    while (c >= 2 && c < clusterEnd && !seen[c] && out.size() < limit) {
        seen[c] = true;
        const size_t at = data + (size_t)(c - 2) * cluster;
        out.insert(out.end(), d.begin() + (long)at, d.begin() + (long)(at + cluster));
        c = (uint32_t)(d[fat + 2 * (size_t)c] | d[fat + 2 * (size_t)c + 1] << 8);
    }
    if (out.size() > limit) out.resize(limit);
    return out;
}

std::vector<XtDiskImage::Entry> XtDiskImage::entries(const std::vector<uint8_t>& raw) const {
    std::vector<Entry> out;
    std::string longName;
    for (size_t i = 0; i + 32 <= raw.size(); i += 32) {
        const uint8_t* x = &raw[i];
        if (x[0] == 0) break;
        if (x[0] == 0xe5) { longName.clear(); continue; }
        if (x[11] == 0x0f) {   // a long name's piece, last first; these names are ASCII
            std::string part;
            for (int k : { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 }) {
                if (x[k] == 0 || x[k] == 0xff) break;
                part += (char)x[k];
            }
            longName = part + longName;
            continue;
        }
        if (!(x[11] & 8)) {
            std::string name = longName;
            if (name.empty()) {
                for (int k = 0; k < 8 && x[k] != ' '; ++k)
                    name += (char)x[k];
                if (x[8] != ' ') {
                    name += '.';
                    for (int k = 8; k < 11 && x[k] != ' '; ++k)
                        name += (char)x[k];
                }
            }
            if (name != "." && name != "..")
                out.push_back({ name, (x[11] & 0x10) != 0, (uint32_t)(x[26] | x[27] << 8), le32(x + 28) });
        }
        longName.clear();
    }
    return out;
}

std::vector<XtDiskImage::Entry> XtDiskImage::list(const std::string& folder) const {
    if (d.empty()) return {};
    std::vector<Entry> at = entries(std::vector<uint8_t>(d.begin() + (long)root, d.begin() + (long)(root + rootBytes)));
    size_t from = 0;
    while (from < folder.size()) {
        size_t to = folder.find('/', from);
        if (to == std::string::npos) to = folder.size();
        const std::string step = folder.substr(from, to - from);
        const Entry* next = nullptr;
        for (const Entry& e : at) if (e.isDir && e.name == step) next = &e;
        if (!next) return {};
        at = entries(chain(next->cluster, 0x100000));
        from = to + 1;
    }
    return at;
}

std::vector<uint8_t> XtDiskImage::read(const Entry& file) const { return chain(file.cluster, file.size); }

std::vector<size_t> XtDiskImage::presetWaves() const {
    std::vector<size_t> at;
    for (size_t i = partitionEnd; i + 4 <= d.size(); ++i)
        if (d[i] == 'v' && !std::memcmp(&d[i], "vpfl", 4)) at.push_back(i);
    return at;
}

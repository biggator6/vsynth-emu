// An OSC's per-note input as rom.4 makes it (vc2oscinput.h).
#include "vc2oscinput.h"

namespace vc2 {

Bytes bytesAt(Machine& m, uint32_t at, uint32_t n) {
    Bytes b(n);
    m.cpu.read(at, b.data(), n);
    return b;
}

void zeroBytes(Machine& m, uint32_t at, uint32_t n) {
    const Bytes z(n, 0);
    m.cpu.write(at, z.data(), n);
}

bool useDefaultPatch(Machine& m) {
    const uint32_t entry = m.cpu.rd32(kPartTable);
    const bool ok = loadStore(m, {});
    m.cpu.wr8(entry, 1);
    m.cpu.wr32(entry + 8, kPatchStore);
    return ok;
}

bool setToneValue(Machine& m, uint32_t id, int value) {
    return setStoreValue(m, kPatchStore + kToneBase, id, value);   // zone 0's tone block
}

bool setOscType(Machine& m, uint32_t type) {
    for (uint32_t osc = 0; osc < kOscs; ++osc)
        if (!setToneValue(m, kSwitchId + kOscIdStride * osc, 1) ||
            !setToneValue(m, kTypeId + kOscIdStride * osc, (int)type))
            return false;
    return true;
}

bool buildRecord(Machine& m, uint32_t out, uint32_t osc, Bytes& record) {
    uint32_t built = 0;
    if (!m.run(kBuilder, {out, 0, 0, osc}, &built) || !built) return false;
    record = bytesAt(m, out + kOutHeader + kOscStride * osc, kRecordBytes);
    return true;
}

void writeOscNote(Machine& m, const OscNote& at, uint32_t osc, uint8_t key, uint8_t velocity, const Bytes& record) {
    zeroBytes(m, at.input, kInputBytes);
    zeroBytes(m, at.block, kBlockBytes);
    zeroBytes(m, at.note, kNoteBytes);
    m.cpu.wr8(at.input + 2, (uint8_t)osc);
    m.cpu.wr8(at.input + 3, key);
    m.cpu.wr8(at.input + 4, velocity);
    m.cpu.write(at.input + kNoteHeader, record.data(), kRecordBytes);
    m.cpu.wr32(at.input + kNotePointer, at.note);
}

} // namespace vc2

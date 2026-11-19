// xt_vc2sets' events (v280): each of C's routines and M's on both sides, rom.4's then the port's; what differs after:
// C, M, W, the part's M, the calls and the answer.  XT_VC2SETS_MAIN only.
#ifdef XT_VC2SETS_MAIN
#include "Vc2SetsCheck.h"

namespace {
constexpr uint32_t kEnded = 3;   // [FW] what an end is given beside its object

// An event's set, note and call struct (in rom.4's memory, and as the port is given it)
struct SetDrawn {
    uint32_t set, note, at;
    Vc2SetCall call;
};

SetDrawn setDrawn(SetSides& s, std::mt19937& rng) {
    SetDrawn d { setsSet(rng), setsNote(rng), setsCall(s, rng), {} };
    s.fw.cpu.read(d.at, &d.call, sizeof d.call);
    return d;
}

// A kind as M's routines are given it: 0 half the time, else a few or any word
uint32_t matrixKind(std::mt19937& rng) {
    if (rng() % 2) return 0;
    return rng() % 2 ? 1 + rng() % 3 : (uint32_t)rng();
}

uint32_t anyMatrix(std::mt19937& rng) {
    return Vc2NoteMatrices::kAt + Vc2NoteMatrices::kBytes * (uint32_t)(rng() % Vc2NoteMatrices::kCount);
}

// Both placements, of any number
template <size_t I>
std::string setPlaceEvent(SetSides& s, std::mt19937& rng) {
    const uint32_t n = rng() % 256;
    const bool sets = I == 0;
    const uint32_t bytes = sets ? Vc2Sets::kBytes : Vc2NoteMatrices::kBytes;
    ++s.n.byEvent[sets ? 0 : 8];
    const Vc2CheckRoutine r { sets ? 0x893a76a0u : 0x893a5d80u, sets ? "C's placement" : "M's placement", vc2R0 };
    return setsOnBoth(s, r, { bytes, n }, [&] {
        return sets ? s.port.addressOf(n) : s.port.matrices.addressOf(n);
    });
}

std::string setMakeEvent(SetSides& s, std::mt19937& rng) {
    const SetDrawn d = setDrawn(s, rng);
    ++s.n.byEvent[1];
    s.n.noCommon += s.notes.at(d.note).kind != 0;
    return setsOnBoth(s, { 0x893a76d0, "the make" }, { d.set, d.at, d.note }, [&] {
        s.port.make(d.set, d.call, d.note, s.notes, s.in(), s.parts);
        return 0u;
    });
}

std::string setEndEvent(SetSides& s, std::mt19937& rng) {
    const uint32_t set = rng() % 8 ? setsSet(rng) : 0;
    ++s.n.byEvent[2];
    return setsOnBoth(s, { 0x893a7a20, "the end" }, { set, kEnded }, [&] {
        s.port.end(set, s.parts);
        return 0u;
    });
}

std::string setConvertEvent(SetSides& s, std::mt19937& rng) {
    const SetDrawn d = setDrawn(s, rng);
    ++s.n.byEvent[3];
    return setsOnBoth(s, { 0x893a7b20, "the conversion" }, { d.set, d.at, d.note }, [&] {
        s.port.convert(d.set, d.call, d.note, s.notes, s.parts);
        return 0u;
    });
}

std::string setReleaseEvent(SetSides& s, std::mt19937& rng) {
    const uint32_t set = setsSet(rng), note = setsNote(rng), velocity = (uint32_t)rng();
    ++s.n.byEvent[4];
    return setsOnBoth(s, { 0x893a7c50, "the release" }, { set, velocity, note }, [&] {
        s.port.release(set, velocity, note, s.parts);
        return 0u;
    });
}

std::string setUpdateEvent(SetSides& s, std::mt19937& rng) {
    const SetDrawn d = setDrawn(s, rng);
    ++s.n.byEvent[5];
    return setsOnBoth(s, { 0x893a7cc0, "the update" }, { d.set, d.at }, [&] {
        s.port.update(d.set, d.call, s.in(), s.parts);
        return 0u;
    });
}

std::string setCopyEvent(SetSides& s, std::mt19937& rng) {
    const uint32_t set = setsSet(rng), note = setsNote(rng);
    ++s.n.byEvent[6];
    return setsOnBoth(s, { 0x893a7df0, "the copy" }, { set, note }, [&] {
        s.port.copy(set, note, s.notes, s.parts);
        return 0u;
    });
}

std::string setRefreshEvent(SetSides& s, std::mt19937& rng) {
    const SetDrawn d = setDrawn(s, rng);
    ++s.n.byEvent[7];
    return setsOnBoth(s, { 0x893a7f10, "the refresh", vc2R0 }, { d.set, d.at },
                      [&] { return s.port.refresh(d.set, d.call, s.parts); });
}

std::string matrixMakeEvent(SetSides& s, std::mt19937& rng) {
    const SetDrawn d = setDrawn(s, rng);
    const uint32_t m = anyMatrix(rng), kind = matrixKind(rng);
    ++s.n.byEvent[9];
    return setsOnBoth(s, { 0x893a5db0, "M's constructor", vc2R0 }, { m, d.at, kind }, [&] {
        s.port.matrices.construct(m, d.call, kind, s.in(), s.parts);
        return m;
    });
}

std::string matrixUpdateEvent(SetSides& s, std::mt19937& rng) {
    const SetDrawn d = setDrawn(s, rng);
    const uint32_t m = anyMatrix(rng), kind = matrixKind(rng);
    ++s.n.byEvent[10];
    return setsOnBoth(s, { 0x893a6010, "M's update" }, { m, d.at, kind }, [&] {
        s.port.matrices.update(m, d.call, kind, s.in(), s.parts);
        return 0u;
    });
}

const SetEvent kEvents[] = {
    setPlaceEvent<0>, setMakeEvent, setMakeEvent, setEndEvent, setConvertEvent, setReleaseEvent, setUpdateEvent,
    setCopyEvent, setRefreshEvent, setPlaceEvent<1>, matrixMakeEvent, matrixMakeEvent, matrixUpdateEvent,
};
} // namespace

std::span<const SetEvent> setEvents() { return kEvents; }
#endif

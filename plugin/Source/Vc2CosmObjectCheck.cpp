// xt_vc2cosm's fifth part (v249): rom.4's COSM object's frame against Vc2CosmObject, on an object in the interpreter's
// RAM, nothing hooked: the constructor in place (0x893aa030), the set-up (0x893aa590) on a drawn struct and flag, the
// binding (0x893aa7b0) of four drawn words, and the install (0x893aafe0) on a drawn flag, code byte, variant byte and
// Filter Type.  Each case the object noise, then 60 events; after each the object word for word and, where rom.4 gives
// one, the answer.  XT_VC2COSM_MAIN only.
#ifdef XT_VC2COSM_MAIN
#include "Vc2CosmCheck.h"
#include "Vc2CosmObject.h"

namespace {
constexpr uint32_t kConstruct = 0x893aa030, kSetUp = 0x893aa590, kBind = 0x893aa7b0, kInstall = 0x893aafe0;   // [FW]
constexpr uint32_t kObjectAt = 0x8C074000, kSetUpAt = 0x8C074200;   // in Vc2Fw's RAM: the object, the set-up's struct
constexpr int kEvents = 60;

struct ObjectCounts {
    long events = 0, constructs = 0, setUps = 0, binds = 0, installs = 0, stereo = 0, draws = 0;
};

struct ObjectRig {
    explicit ObjectRig(Vc2Fw& f) : fw(f) {}
    Vc2Fw& fw;
    Vc2CosmObjectWords o {};
    ObjectCounts n;
    // vc2CosmPart's: rom.4's side hooked (nothing), a case's start, an event, the counts said
    void hook() {}
    void start(std::mt19937& rng);
    std::string event(std::mt19937& rng);
    void say(const Vc2Run& run) const;
};

// What the install reads, drawn: the code byte mostly a type's (0…20), now and then any; the variant mostly a Type's
// (0…2), now and then 3 (where the Wave Shaper's float steps), on into the next types' floats or any; the Filter Type
// mostly 0…14, now and then anywhere the floats reach (−84…1,209)
void drawInstallWords(Vc2CosmObjectWords& o, std::mt19937& rng) {
    const std::array<uint8_t, 3> codes { (uint8_t)(rng() % 21), (uint8_t)(rng() % 21), (uint8_t)rng() };
    o.dspCode = codes[rng() % codes.size()];
    const std::array<uint8_t, 4> variants { (uint8_t)(rng() % 3), 3, (uint8_t)(rng() % 23), (uint8_t)rng() };
    o.variant = variants[rng() % variants.size()];
    const std::array<int32_t, 3> types { (int32_t)(rng() % 15), (int32_t)(rng() % 15), (int32_t)(rng() % 1294) - 84 };
    o.filterType = types[rng() % types.size()];
}

// A flag as the set-up's and the install's callers give it: 0, 1, or any word
uint32_t drawCallerFlag(std::mt19937& rng) {
    const std::array<uint32_t, 3> flags { 0, 1, (uint32_t)rng() };
    return flags[rng() % flags.size()];
}

// rom.4's `routine` on the object and `args`, then the port's `mine`: the object and, when asked, the answer
template <class Mine>
std::string objectBoth(ObjectRig& r, uint32_t routine, std::initializer_list<uint32_t> args, Vc2CosmAsk ask,
                       Mine mine) {
    r.fw.cpu.write(kObjectAt, &r.o, sizeof r.o);
    return vc2CosmBoth(r.fw, routine, args, ask, mine, [&r] {
        return vc2RegionsDifference(r.fw, { { kObjectAt, &r.o, sizeof r.o, "the COSM object" } });
    });
}

using ObjectEvent = std::string (*)(ObjectRig&, std::mt19937&);

std::string objConstruct(ObjectRig& r, std::mt19937&) {
    ++r.n.constructs;
    return objectBoth(r, kConstruct, { kObjectAt }, { "the constructor" }, [&r] {
        vc2CosmConstruct(r.o);
        return kObjectAt;
    });
}

std::string objSetUp(ObjectRig& r, std::mt19937& rng) {
    ++r.n.setUps;
    Vc2CosmSetUpWords s {};
    vc2CosmNoise(s, rng);
    const uint32_t flag = drawCallerFlag(rng);
    r.fw.cpu.write(kSetUpAt, &s, sizeof s);
    return objectBoth(r, kSetUp, { kObjectAt, kSetUpAt, flag }, { "the set-up", false }, [&] {
        vc2CosmSetUp(r.o, s, flag);
        return 0u;
    });
}

std::string objBind(ObjectRig& r, std::mt19937& rng) {
    ++r.n.binds;
    const std::array<uint32_t, 4> b { (uint32_t)rng(), (uint32_t)rng(), (uint32_t)rng(), (uint32_t)rng() };
    r.fw.cpu.wr32(kVc2Sp, b[3]);   // the fifth argument, on the stack as a caller leaves it (Sh4::call sets r4…r7)
    return objectBoth(r, kBind, { kObjectAt, b[0], b[1], b[2] }, { "the binding", false }, [&] {
        vc2CosmBind(r.o, b);
        return 0u;
    });
}

std::string objInstall(ObjectRig& r, std::mt19937& rng) {
    ++r.n.installs;
    const uint32_t flag = drawCallerFlag(rng);
    r.n.stereo += flag != 0;
    drawInstallWords(r.o, rng);
    return objectBoth(r, kInstall, { kObjectAt, flag }, { "the install" }, [&] { return vc2CosmInstall(r.o, flag); });
}

std::string objDraw(ObjectRig& r, std::mt19937& rng) {
    ++r.n.draws;
    vc2CosmNoise(r.o, rng);
    return "";
}

// Each event's share of eight
constexpr std::array<ObjectEvent, 8> kObjectEvents { objConstruct, objSetUp, objBind, objInstall, objInstall,
                                                     objInstall, objInstall, objDraw };

void ObjectRig::start(std::mt19937& rng) {
    vc2CosmNoise(o, rng);
    drawInstallWords(o, rng);
}

// one-off: each part's own event table, through vc2CosmEvent
std::string ObjectRig::event(std::mt19937& rng) { return vc2CosmEvent(*this, kObjectEvents, rng); }

// one-off: each part's own report line, in fcheck's baseline in its own words; one printer would rewrite the earlier
// parts' lines
void ObjectRig::say(const Vc2Run& run) const {
    std::printf("object: %d cases, %ld events: %ld constructs, %ld set-ups, %ld bindings, %ld installs (%ld stereo), "
                "%ld draws\n", run.cases, n.events, n.constructs, n.setUps, n.binds, n.installs, n.stereo, n.draws);
}
} // namespace

long vc2CosmObjectCases(Vc2Fw& fw, const Vc2Run& run) { return vc2CosmPart<ObjectRig>(fw, run, 4, kEvents); }
#endif

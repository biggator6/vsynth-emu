// The VC-2 part's settings by ID, the dispatchers 0x8939b170, 0x8939b0d0, 0x8939b110 and 0x8939b140, and G's channel
// word 0x893a5050 (v292): see Vc2PartSettings.h.  Each reader in rom.4's order; the systems' readers each answer 1,
// so their dispatchers' tests never stop them.
#include "Vc2PartSettings.h"

namespace {
using Block = Vc2ParamBlock;
constexpr int16_t kReceiveSwitch = 71, kReceiveChannel = 72;   // [FW] Part 1 Receive Switch and Channel, system B's
constexpr uint32_t kStateAt = 2;                               // [FW] G's word: the table's word + 2
} // namespace

bool Vc2PartSettings::readPart(Vc2PartRows& t, Vc2MatrixSettings& g, Vc2MatrixSetup& setup, int part,
                               const Vc2PartNodes& nodes, Vc2SettingsParts& out) {
    return modes(t, part, nodes, out) && bytes(t, part) && common(t, part) && oscs(t, part) && mixes(t, part) &&
           envelopes(t, part) && lfos(t, part) && cosms(t, part) && tvas(t, part) && setup.settings(g, part) &&
           vocoder(t, part, out);
}

void Vc2PartSettings::readSystem(Vc2PartRows& t, Vc2MatrixSettings& g, Vc2ChannelStates& states) {
    oscTrailer(t);
    lfoSystem(t);
    tvaRoute(t);
    matrixChannel(g, states);
}

void Vc2PartSettings::readAssigns(Vc2PartRows& t, Vc2MatrixSettings& g, Vc2MatrixSetup& setup) {
    setup.assigns(g);
    tvaControls(t);
}

void Vc2PartSettings::readCalibration(Vc2PartRows& t) {
    oscCalibration(t);
    tvaTrailer(t);
}

void Vc2PartSettings::matrixChannel(Vc2MatrixSettings& g, Vc2ChannelStates& states) {
    if (p_.value(Block::kSystemB, 0, 0, kReceiveSwitch) == 0) {
        g.channel = states.unreceived();
        return;
    }
    g.channel = states.receiving(p_.value(Block::kSystemB, 0, 0, kReceiveChannel)) + kStateAt;
}

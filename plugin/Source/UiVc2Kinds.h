#pragma once
// The VC-2 mode's blocks (v453; research/UI_DESIGN_VC2.md, "SYNTH" and "A zone, a COSM and a face as the selected one"):
// its kinds' table, as D's (UiBlockKinds.h), and a model for each zone, so its path, lower panel and overviews are D's
// parts made from it.  A row is rom.4's ID for zone 1, OSC 1 and COSM 1, in hex ("2e7"); an envelope's or an LFO's
// parameter its first ID and its role after a '+' ("390+Attack", "439+Rate"), as D's kinds give a prefix and a name.
// The instance's ID is made from it by rom.4's layout (vc2InstanceId) and then the host's (vc2HostParamId).
#include "UiBlockKinds.h"

namespace Vc2Blocks {

inline constexpr int kZones = 2;                      // a patch's zones the VC-2's parameters hold (v451)
inline constexpr int kOscFaces = 4, kCosmFaces = 17;  // OSC Type's values (ANALOG, PCM, EXTIN, VOCODER), COSM Type's

// The model and the path for a zone (0 or 1)
const Blocks::Model& model(int zone);
Blocks::Path path(int zone);
// rom.4's ID of an instance's parameter, from its first instance's: OSC 2's by each run's distance, COSM 2's envelopes and
// LFO by theirs (its own IDs are the host's COSM 2, by vc2HostParamId's `cosm`)
int instanceId(Blocks::Kind kind, int index, int id);
// Which instance of a kind a rom.4 ID is (its host's COSM given): OSC 1 or 2 by the runs, a COSM's own IDs by `cosm`, its
// envelopes and LFO by theirs; -1 if it isn't the kind's (the editor's test holds each instance's controls to it)
int whichInstance(Blocks::Kind kind, int id, int cosm);
// The host's ID of a row for a zone's instance (a role resolved first); empty if the row names nothing
juce::String param(int zone, Blocks::Kind kind, int index, const juce::String& row);
// The rom.4 ID a row names for the first instance, its role resolved; -1 if none
int rowId(const juce::String& row);
// A Matrix Control's parameter by field (Blocks::Model's): Control n's Source is 0x16a + 5n, then its destinations and
// their Sens
juce::String matrixId(int control, int field);
// d-6: what rom.4 holds and nothing found reads (the Growl's five, Harm Formant's LFO and Envelope Depths, Wave start
// offset, the second Wave Number), by any instance's rom.4 ID
bool unread(int id);

}

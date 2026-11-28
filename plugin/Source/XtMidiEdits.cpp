// rom.2's edits of the patch from a control change (XtMidiEdits.h, v462): the port's numbers as 0x89474a10 moves them,
// then the part object's RPN slot for Data Entry and its control change slot for every number.
#include "XtMidiEdits.h"

namespace {
// The part object's RPN slot (0x89475f80), by the RPN's number (a table at 0x89475fdc): none past 2, which a number
// with the other kind chosen last (bit 15) is
void rpnEdits(uint16_t rpn, uint16_t data, XtStoredEdits& e) {
    const int msb = data >> 7;
    switch (rpn) {
        case 0:   // 0x89476050: both Bend Ranges, the MSB, to 24
            if (msb > 24) return;
            e.add(xtedit::BendUp, msb);
            e.add(xtedit::BendDown, msb);
            return;
        case 1: {   // 0x89476000: the Fine Tune in cents, ±50
            const int cents = ((int)data * 100 >> 13) - 100;
            if (cents >= -50 && cents <= 50) e.add(xtedit::FineTune, cents);
            return;
        }
        case 2:   // 0x89475fe2: the Coarse Tune in semitones, ±48
            if (msb - 64 >= -48 && msb - 64 <= 48) e.add(xtedit::CoarseTune, msb - 64);
            return;
        default: return;
    }
}

// The part object's control change slot (0x89475ee0), after its base's (0x894754e0, the bank it keeps for a program
// change, which isn't taken)
void ccEdits(int cc, int value, XtStoredEdits& e) {
    switch (cc) {
        case 5: e.add(xtedit::PortaTime, value); return;
        case 65: e.add(xtedit::PortaSwitch, value >= 64 ? 1 : 0); return;
        case 126: e.add(xtedit::MonoPoly, 0); return;   // MONO
        case 127: e.add(xtedit::MonoPoly, 1); return;   // POLY
        default: return;
    }
}
}  // namespace

XtStoredEdits XtMidiPort::controlChange(int cc, int value) {
    XtStoredEdits e;
    const auto v = (uint16_t)value;
    switch (cc) {
        case 6:    // Data Entry's MSB (0x89474a80), then the listeners' RPN and NRPN slots
            data = (uint16_t)(v << 7);
            rpnEdits(rpn, data, e);
            break;
        case 38:   // its LSB (0x89474ad0)
            data = (uint16_t)((data & 0x3f80) | v);
            rpnEdits(rpn, data, e);
            break;
        case 98: nrpn = (uint16_t)((nrpn & 0x7f80) | v); rpn |= kUnselected; break;           // 0x89474b30
        case 99: nrpn = (uint16_t)((nrpn & 0x00ff) | v << 8); rpn |= kUnselected; break;      // 0x89474b50
        case 100: rpn = (uint16_t)((rpn & 0x7f80) | v); nrpn |= kUnselected; break;           // 0x89474b70
        case 101: rpn = (uint16_t)((rpn & 0x00ff) | v << 8); nrpn |= kUnselected; break;      // 0x89474b90
        case 121:  // Reset All Controllers at 0 (0x89474be0): the numbers too
            if (value == 0) rpn = nrpn = kNone;
            break;
        default: break;
    }
    ccEdits(cc, value, e);   // every message goes on to the store and the control change slot (0x89474bf4)
    return e;
}

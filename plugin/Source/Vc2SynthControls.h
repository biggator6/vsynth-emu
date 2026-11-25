#pragma once
// The part's controllers' parts, the part's own (v345, Vc2Synth whole's sixth version): what the controller (0x89392f90)
// and the part's controller (0x89394440), the controller numbers' handlers and Time Trip reach beyond themselves, each
// to the part's port: a number's object's handler (Vc2PartNumbers, its world the part's), a node's word's first byte,
// a part's trip record by the part table, the trip's set and move, the listeners' run and the release path over a key
// table's voices (Vc2Assigner), the vocoder's pedal, and the sounding tones' Time Trip steps.  Bit for bit:
// xt_vc2synth's load part (v345) and notes part (v346).  HARDWARE_FIRMWARE.md, "The part's settings and voices wired".
// The notes' task.
#include "Vc2PartControllers.h"
#include "Vc2PartNumbers.h"
#include "Vc2SynthVoices.h"
#include "Vc2TimeTrip.h"

class Vc2SynthControls final : public Vc2ControllerParts, public Vc2NumberParts, public Vc2TripParts {
public:
    explicit Vc2SynthControls(Vc2Synth& s) : s_(s) {}
    // 0x89392f90 and 0x89394440 on the part
    void controller(int source, int32_t channel, int32_t number, int32_t value) {
        Vc2PartControllers(s_.rows, s_.nodes, *this).controller(source, channel, number, value);
    }
    void partController(uint8_t part, int32_t number, int32_t value) {
        Vc2PartControllers(s_.rows, s_.nodes, *this).partController(part, number, value);
    }
    // The controllers' reach: a number's object's handler, a word's first byte, a part's trip and its routines
    void numberChanged(int32_t number, int source, int32_t channel, int32_t value) override {
        Vc2NumberWorld w { s_.rows, s_.nodes, s_.keys, *this };
        s_.numbers.changed(number, source, channel, value, w);
    }
    uint8_t firstByte(uint32_t word) override { return s_.reads->firstByte(word); }
    uint32_t tripAt(uint8_t part) override { return s_.partTable[part]; }
    Vc2TripFront& trip(uint32_t address) override {
        return s_.trips.records[(address - Vc2TripRecords::kAt) / sizeof(Vc2TripRecord)].front;
    }
    void tripSet(uint32_t at, int32_t on, int8_t x, int8_t y) override { vc2TripSet(trip(at).trip, on, x, y); }
    uint8_t tripMove(uint32_t at, int8_t x, int8_t y) override {
        return (uint8_t)vc2TripMove(trip(at).trip, x, y, s_.trips.scratch, *this);
    }
    // The handlers' reach: the listeners of a channel's state, the vocoder's pedal, the release path by key table
    void notify(uint32_t state) override { byTable<&Vc2Assigner::notify>(state); }
    void vocoderHold(int32_t value) override { s_.vocoder.hold(value); }
    void holdOff(uint32_t table) override { byTable<&Vc2Assigner::holdOff>(table); }
    void sostenutoOff(uint32_t table) override { byTable<&Vc2Assigner::sostenutoOff>(table); }
    void truncateTable(uint32_t table) override { s_.assigner.truncateTable(table, s_.voices, *s_.voiceParts); }
    void releaseBut(uint32_t table) override { byTable<&Vc2Assigner::releaseBut>(table); }
    void releaseAll(uint32_t table) override { byTable<&Vc2Assigner::releaseAll>(table); }
    // Time Trip's: the sounding tones (the part's list's first, each one's +0), a tone's +0x48 virtual
    uint32_t firstTone() override { return s_.lists.sounding; }
    uint32_t nextTone(uint32_t tone) override { return s_.tones.at(tone).head.next; }
    void stepTone(uint32_t tone, int32_t count, uint32_t part, uint32_t mask) override {
        s_.tones.at(tone).countMatched((uint32_t)count, (int32_t)part, mask, s_.hostOf(tone));
    }

private:
    // The assigner's `Member` on a key table's (or a state's) word, with the keys, the voices and their parts
    template <auto Member>
    void byTable(uint32_t word) {
        (s_.assigner.*Member)(word, s_.keys, s_.voices, *s_.voiceParts);
    }
    Vc2Synth& s_;
};

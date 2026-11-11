// The Multi Step Modulator's settings as a patch's store holds them (XtMsm.h): each ID the loaders read, through the store's
// reads (xtPatchValue), which give what the firmware's getter gives on the block at +0x14ac (xt_files' "pointed" line).
#include "XtFiles.h"
#include "XtMsm.h"

XtMsmSettings xtMsmSettingsOf(const std::vector<uint8_t>& store) {
    XtMsmSettings s;
    for (int id = kXtMsmFirstId; id <= kXtMsmLastId; ++id) *xtMsmField(s, id) = xtPatchValue(store, id);
    return s;
}

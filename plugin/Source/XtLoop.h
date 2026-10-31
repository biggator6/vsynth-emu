#pragma once
// Loop playback as the XT's players take it (XtSoloVoice, XtMbPlayer): the
// OSC's Loop switch with the wave's loop points, in samples, and the loop
// modes at the forward end and the backward start (0 none, 1 One Way,
// 2 Alternate, 3 jump to the start and hold).  Forward play loops between
// fwdStart and fwdEnd, backward play between bwdStart and bwdEnd.
struct XtLoop {
    bool on = false;                    // player byte 0 bit 0
    int fwdMode = 0, bwdMode = 0;
    int fwdStart = 0, fwdEnd = 0, bwdStart = 0, bwdEnd = 0;
};

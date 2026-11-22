// The routines the VC-2's ports share, for the plugin: see Vc2ToneMath.h (v320).
#include "Vc2ToneMath.h"
#include "Vc2LibMath.h"
#include "XtOsc.h"

int Vc2ToneMath::velocity(int velocity, int sens, int curve) const { return xtVelCurveValue(velocity, sens, curve); }
float Vc2ToneMath::exp2(float x) const { return xtOscExp2(x); }
float Vc2ToneMath::log(float x) const { return vc2LibLog(x); }
float Vc2ToneMath::pow(float x, float y) const { return vc2LibPow(x, y); }

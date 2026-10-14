# `vpenc.dll` — live oracle, ABI, and open questions

Contributed 2026-10-14 from a separate session that got **V-Trainer running natively
on Apple Silicon**. Complements `HARDWARE_FIRMWARE.md` §encoder; does not restate it.

## 1. The encoder is now executable, not just static

`~/Applications/V-Trainer.app` — self-contained Wine bundle (722 MB, CrossOver 23.7.1
engine, WoW64, needs Rosetta 2). Double-click or:

```sh
open ~/Applications/V-Trainer.app                      # GUI
WINEDEBUG=+relay ~/Applications/V-Trainer.app/Contents/MacOS/V-Trainer   # traced
```

Deployed by file copy, not InstallShield: `VTrainer.exe` + `vpenc.dll` + app-local
`mfc42.dll`. No serial prompt, no `ikernel`. Files: `S:` → `~/V-Trainer Samples`,
`Z:` → `/`. Verified encoding `Drum Sample.wav` → `VP9SAMDrum Sample-DRM.WAV`
(783,592 B) with chunks `fmt `(16) `vpfl`(44, ver "0101") `vpsc`(32) `vmdl`(48)
`vpno`(44) `vpdt`(34,864) `LIST`(84) `rdmd`(332) `data`(748,044).

**Why this matters for `encodeGrainCutsVTrainer` (64.2 vs 65.6):** the real encoder
can now be queried for ground truth on arbitrary input, so the port's divergence is
measurable rather than inferred:

```
Vpenc_Init → SetOsPointers → InitNewDocument → SetWavePrm → SetAlgo(1)
  → SetTemplate(...) → SetSens(127) → EventDetect
  → GetEventCount / GetEventAt(LEvent*, i)   <-- ground-truth decisions
  → Start → Encode → End → Riff_Image / GetChunkInfo
```

Diff those `LEvent` lists against the port's grain cuts on the same vocal input. If
they match and the score still differs, the gap is downstream of cut placement.
`WINEDEBUG=+relay` also logs VTrainer's own calls with arguments — the usage protocol
for free, including real `SetTemplate` values.

## 2. Not yet in the project docs

- **`void Vpenc_SetOsPointers(void*, void* x8)`** — eight injected OS function
  pointers: a portable-core seam. Corroborates "a port of the VP-9000's own encoder"
  and implies the SH-4 firmware and this DLL compile from **one source**, OS services
  swapped. The two binaries cross-validate.
- **Named structs** the mangling reveals: `SChunkInfo`, `LEvent`, `ALGORITHMCAPS`,
  `SEncPrm` (the last exported as global `m_encPrm`, alongside `m_EncoderState`,
  `m_UserDoc`). Layouts recoverable from the accessors below.
- **Chunk construction is exported**, so the RIFF layout needn't be reversed by
  diffing files: `Vpenc_GetChunkInfo(SChunkInfo*)`, `Vpenc_Riff_Image(void*)`,
  `encAllocateChunk(ulong)` / `encFreeChunk(ushort*)`.
- **`.rdata` constants** (small, but usable as firmware fingerprints — search both
  endiannesses, SH-4 is bi-endian): off `0x000f8`, 24 doubles decaying
  `0.10921, 0.106899, 0.100161, 0.0895643, 0.0759792…` (window or FIR kernel);
  off `0x008c0`, 12 doubles alternating-sign `10, 0.315721, -0.0985503, 0.0517546,
  -0.0301213…` (series/polynomial approximation); off `0x00610`, 27 doubles, mixed.
- Imports are **pure MSVC CRT** — no DSP library, no audio API. The whole algorithm
  is the 101,099 bytes of `.text`.

## 3. Correction, logged deliberately

An earlier claim in that session that `vpenc.dll` is **x87 floating point** was
**wrong**. It rested on counting bytes in `0xD8`–`0xDF`, which cannot separate opcodes
from operands and displacements; 1,453 hits in 102,400 bytes is *below* the ~3,200 a
random byte stream would give, so it is no evidence at all. The documented
integer/fixed-point finding stands. Float is confined to parameter math —
`Vpenc_BeatsFromTempo`, `GetBeats`, `SetBeats` all use `double`, and the `.rdata`
tables above are real doubles — not the grain-cut decision path.

Consequence: **bit-exact porting of the fixed-point core is feasible**, so a scoring
gap is algorithmic, not a precision wall.


## 4. Export table (83 named, mechanically extracted)

Signatures from MSVC mangling, so arity and types are authoritative.
Image base `0x10000000`; `.text` 101,099 B, `.rdata` 8,413 B, `.data` 62,024 B virtual / 8,192 raw (rest is BSS).

| VA | Signature | Mangled |
|---|---|---|
| `0x10001000` | `—` | `??4CVpenc@@QAEAAV0@ABV0@@Z` |
| `0x100122d0` | `—` | `??0CVpenc@@QAE@XZ` |
| `0x10012300` | `int Vpenc_GetEncoderState(void)` | `?Vpenc_GetEncoderState@@YAHXZ` |
| `0x10012310` | `int Vpenc_GetCurrentDoc(void)` | `?Vpenc_GetCurrentDoc@@YAHXZ` |
| `0x10012320` | `void Vpenc_SetCurrentDoc(int)` | `?Vpenc_SetCurrentDoc@@YAXH@Z` |
| `0x10012330` | `short Vpenc_GetAlgoCaps(ALGORITHMCAPS*, long)` | `?Vpenc_GetAlgoCaps@@YAFPAUALGORITHMCAPS@@J@Z` |
| `0x10012450` | `short Vpenc_GetAlgoNumber(void)` | `?Vpenc_GetAlgoNumber@@YAFXZ` |
| `0x10012460` | `void Vpenc_Abort(void)` | `?Vpenc_Abort@@YAXXZ` |
| `0x10012470` | `int Vpenc_GetAbort(void)` | `?Vpenc_GetAbort@@YAHXZ` |
| `0x10012480` | `void Vpenc_ClrAbort(void)` | `?Vpenc_ClrAbort@@YAXXZ` |
| `0x10012490` | `void Vpenc_SetSens(char)` | `?Vpenc_SetSens@@YAXD@Z` |
| `0x100124a0` | `char Vpenc_GetSens(void)` | `?Vpenc_GetSens@@YADXZ` |
| `0x100124b0` | `void Vpenc_SetCopyFlag(char)` | `?Vpenc_SetCopyFlag@@YAXD@Z` |
| `0x100124c0` | `char Vpenc_GetCopyFlag(void)` | `?Vpenc_GetCopyFlag@@YADXZ` |
| `0x10012560` | `int Vpenc_Start(void)` | `?Vpenc_Start@@YAHXZ` |
| `0x10012620` | `int Vpenc_End(void)` | `?Vpenc_End@@YAHXZ` |
| `0x100126a0` | `int Vpenc_EventDetect(void)` | `?Vpenc_EventDetect@@YAHXZ` |
| `0x100126f0` | `long Vpenc_GetEventCount(void)` | `?Vpenc_GetEventCount@@YAJXZ` |
| `0x10012890` | `int Vpenc_GetEventAt(LEvent*, int)` | `?Vpenc_GetEventAt@@YAHPAULEvent@@H@Z` |
| `0x100128e0` | `int Vpenc_EventIns(long, long, int, int)` | `?Vpenc_EventIns@@YAHJJHH@Z` |
| `0x10012900` | `int Vpenc_EventDel(long, long, int)` | `?Vpenc_EventDel@@YAHJJH@Z` |
| `0x10012930` | `int Vpenc_EventMov(long, long, int, int)` | `?Vpenc_EventMov@@YAHJJHH@Z` |
| `0x10012970` | `int Vpenc_Encode(void)` | `?Vpenc_Encode@@YAHXZ` |
| `0x10012eb0` | `?P encAllocateChunk(?A, ushort, ulong)` | `?encAllocateChunk@@YAPAGK@Z` |
| `0x10012ec0` | `int encFreeChunk(?P, ?A, ushort)` | `?encFreeChunk@@YAHPAG@Z` |
| `0x10012ed0` | `int Vpenc_Riff_Stream_Read(int)` | `?Vpenc_Riff_Stream_Read@@YAHH@Z` |
| `0x10012ee0` | `int Vpenc_Aiff_Stream_Read(int)` | `?Vpenc_Aiff_Stream_Read@@YAHH@Z` |
| `0x10012ef0` | `int Vpenc_Aiff_Stream_Write(int)` | `?Vpenc_Aiff_Stream_Write@@YAHH@Z` |
| `0x10012f00` | `int Vpenc_Riff_Stream_Write(int)` | `?Vpenc_Riff_Stream_Write@@YAHH@Z` |
| `0x10012f10` | `int Vpenc_GetChunkInfo(SChunkInfo*)` | `?Vpenc_GetChunkInfo@@YAHPAUSChunkInfo@@@Z` |
| `0x10012f20` | `int Vpenc_Riff_Image(void*)` | `?Vpenc_Riff_Image@@YAHPAX@Z` |
| `0x10012f30` | `int Vpenc_Smf_Stream_Write(int)` | `?Vpenc_Smf_Stream_Write@@YAHH@Z` |
| `0x10012f50` | `void Vpenc_Init(void)` | `?Vpenc_Init@@YAXXZ` |
| `0x10012fe0` | `int Vpenc_Tempo(void)` | `?Vpenc_Tempo@@YAHXZ` |
| `0x10012ff0` | `long Vpenc_SamplesPerBeat(void)` | `?Vpenc_SamplesPerBeat@@YAJXZ` |
| `0x10013000` | `double Vpenc_BeatsFromTempo(ulong)` | `?Vpenc_BeatsFromTempo@@YANK@Z` |
| `0x10013010` | `long GetRawMark(int, long)` | `?GetRawMark@@YAJHJ@Z` |
| `0x10013090` | `double Vpenc_GetBeats(void)` | `?Vpenc_GetBeats@@YANXZ` |
| `0x100130a0` | `void Vpenc_SetBeats(double)` | `?Vpenc_SetBeats@@YAXN@Z` |
| `0x100130c0` | `ulong Vpenc_GetTempo(void)` | `?Vpenc_GetTempo@@YAKXZ` |
| `0x100130d0` | `void Vpenc_SetTempo(ulong)` | `?Vpenc_SetTempo@@YAXK@Z` |
| `0x100130e0` | `ushort Vpenc_GetTune(void)` | `?Vpenc_GetTune@@YAGXZ` |
| `0x100130f0` | `void Vpenc_SetTune(ushort)` | `?Vpenc_SetTune@@YAXG@Z` |
| `0x10013110` | `long Vpenc_GetFineTune(void)` | `?Vpenc_GetFineTune@@YAJXZ` |
| `0x10013120` | `void Vpenc_SetFineTune(long)` | `?Vpenc_SetFineTune@@YAXJ@Z` |
| `0x10013130` | `long Vpenc_GetLoopStart(void)` | `?Vpenc_GetLoopStart@@YAJXZ` |
| `0x10013140` | `long Vpenc_GetLoopEnd(void)` | `?Vpenc_GetLoopEnd@@YAJXZ` |
| `0x10013150` | `void Vpenc_SetLoopStart(long)` | `?Vpenc_SetLoopStart@@YAXJ@Z` |
| `0x10013160` | `void Vpenc_SetLoopEnd(long)` | `?Vpenc_SetLoopEnd@@YAXJ@Z` |
| `0x10013170` | `void Vpenc_SetWavePrm(ulong, ulong, int, ulong)` | `?Vpenc_SetWavePrm@@YAXKKHK@Z` |
| `0x10013200` | `ulong Vpenc_GetCPU1SampleRate(void)` | `?Vpenc_GetCPU1SampleRate@@YAKXZ` |
| `0x10013210` | `ulong Vpenc_GetSampleRate(void)` | `?Vpenc_GetSampleRate@@YAKXZ` |
| `0x10013220` | `int Vpenc_GetChannels(void)` | `?Vpenc_GetChannels@@YAHXZ` |
| `0x10013230` | `ulong Vpenc_GetWaveSize(void)` | `?Vpenc_GetWaveSize@@YAKXZ` |
| `0x10013240` | `long Vpenc_GetWaveAdr(void)` | `?Vpenc_GetWaveAdr@@YAJXZ` |
| `0x10013250` | `void Vpenc_SetListAdrSize(ulong, long)` | `?Vpenc_SetListAdrSize@@YAXKJ@Z` |
| `0x10013270` | `void Vpenc_SetModelAdrSize(ulong, long)` | `?Vpenc_SetModelAdrSize@@YAXKJ@Z` |
| `0x10013290` | `long Vpenc_GetListSize(void)` | `?Vpenc_GetListSize@@YAJXZ` |
| `0x100132a0` | `void Vpenc_SetListSize(long)` | `?Vpenc_SetListSize@@YAXJ@Z` |
| `0x100132b0` | `char Vpenc_GetSigNum(void)` | `?Vpenc_GetSigNum@@YADXZ` |
| `0x100132c0` | `char Vpenc_GetSigDeno(void)` | `?Vpenc_GetSigDeno@@YADXZ` |
| `0x100132d0` | `void Vpenc_SetSig(char, char)` | `?Vpenc_SetSig@@YAXDD@Z` |
| `0x10013310` | `void Vpenc_SetMIDIOption(int)` | `?Vpenc_SetMIDIOption@@YAXH@Z` |
| `0x10013320` | `void Vpenc_SetOrgNote(char)` | `?Vpenc_SetOrgNote@@YAXD@Z` |
| `0x10013330` | `char Vpenc_GetOrgNote(void)` | `?Vpenc_GetOrgNote@@YADXZ` |
| `0x10013340` | `void Vpenc_SetKey(char)` | `?Vpenc_SetKey@@YAXD@Z` |
| `0x10013350` | `char Vpenc_GetKey(void)` | `?Vpenc_GetKey@@YADXZ` |
| `0x10013360` | `void Vpenc_SetMediaFormatType(char)` | `?Vpenc_SetMediaFormatType@@YAXD@Z` |
| `0x10013370` | `char Vpenc_GetMediaFormatType(void)` | `?Vpenc_GetMediaFormatType@@YADXZ` |
| `0x10013380` | `int Vpenc_GetVersion(void)` | `?Vpenc_GetVersion@@YAHXZ` |
| `0x10013390` | `void Vpenc_SetAlgo(char)` | `?Vpenc_SetAlgo@@YAXD@Z` |
| `0x100133a0` | `char Vpenc_GetAlgo(void)` | `?Vpenc_GetAlgo@@YADXZ` |
| `0x100133b0` | `ulong Vpenc_GetBuildNumber(void)` | `?Vpenc_GetBuildNumber@@YAKXZ` |
| `0x10013430` | `void Vpenc_SetOsPointers(void*, ?0, ?0, ?0, ?0, ?0, ?0, ?0, ?0)` | `?Vpenc_SetOsPointers@@YAXPAX00000000@Z` |
| `0x10013440` | `int Vpenc_ReBuildMidi(void)` | `?Vpenc_ReBuildMidi@@YAHXZ` |
| `0x10013530` | `int Vpenc_TypeConvertInit(void)` | `?Vpenc_TypeConvertInit@@YAHXZ` |
| `0x10013580` | `int Vpenc_ReEncodeInit(void)` | `?Vpenc_ReEncodeInit@@YAHXZ` |
| `0x10013730` | `void Vpenc_SetTemplate(char, long, long, long, long, long)` | `?Vpenc_SetTemplate@@YAXDJJJJJ@Z` |
| `0x10013770` | `void Vpenc_SetTemplateNumber(char)` | `?Vpenc_SetTemplateNumber@@YAXD@Z` |
| `0x10013780` | `void Vpenc_InitNewDocument(void)` | `?Vpenc_InitNewDocument@@YAXXZ` |
| `0x1002b9d0` | `—` | `?m_UserDoc@@3HA` |
| `0x1002b9d8` | `—` | `?m_encPrm@@3USEncPrm@@A` |
| `0x1002bb6c` | `—` | `?m_EncoderState@@3HA` |

## 5. Oracle results (Session 16 main thread, 2026-10-14)

**Correction to §1:** `Vpenc_EventDetect` / `GetEventAt` return the attack
*events* (Encode Depth / STEP-EVENT markers), not grain marks. The grain
records are the output file's **`vpdt` chunk** (also walked by
`GetRawMark` after `Vpenc_Encode`). Parser: `research/vpdt_parse.py`.

**SOLO `vpdt`:** 116-byte header, then `{u16 length (1/16 sample), u16
info}` records that tile the whole sample — the exact stream the V-Synth
firmware player reads. On `vocal_aah` (44.1 kHz mono, template defaults):
600 records = 463 voiced (median period 338.31 samples = 130.4 Hz; onset
ramp ¼, ½, then full periods) + 137 unvoiced (701/16 ≈ 1 ms, info 0x80).
Voiced `info & 7 == 7` always; `info >> 3` ranges 11–20.

**Where real marks sit:** ≈ 9 samples before the upward zero crossing (IQR
±5); 114 after the positive peak; 80 after the P/4 energy centre.
Our `encodeGrainCutsVTrainer` matches the period track exactly (median
338.31) with a near-constant phase offset (−25.6 samples) → tracker
correct, first-mark rule incomplete. The default peak-snap encoder is
−114 samples (a third of a period) off.

**Rendering with the real marks** (`VSE_CUTSFILE`, firmware SOLO player,
same 44.1 k input): vocal mean **65.63** vs **66.26** for the default
peak-snap encoder and 64.27 for the port. Real marks win time 2× (73.1 vs
68.5) and time ×½ (69.7 vs 68.5), lose formant downmax (57.9 vs 63.5) and
pitch +7 (63.5 vs 66.7). **Conclusion: grain-mark placement is not the
remaining vocal gap — it is downstream** (player details, e.g. the
unimplemented per-record `info >> 3` repeat/hold field, Energy, or
reference/metric factors).

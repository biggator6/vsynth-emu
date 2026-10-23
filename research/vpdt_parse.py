"""Parse VP-9000 encoded sample files (V-Trainer output, RIFF 'VP9SAM...').

SOLO 'vpdt' layout (Session 16, verified on a 44.1 kHz vocal encode):
  116-byte header (29 u32; total chunk = 116 + 4·nRecords), then records
  {u16 length in 1/16-sample units (0xFFFF = continue), u16 info}.
  info & 7 == 7 → voiced (patent path, pitch + formant gates);
  info >> 3     → per-record field 11..20 (firmware: (info & 0x1F8) << 6,
                  used by the SOLO scheduler as a repeat/hold count);
  info 0x80 / 0 → unvoiced (~1 ms records, 701/16 samples).
Record lengths tile the whole sample. BACKING/ENSEMBLE 'vpdt' differ
(multi-band); not yet decoded.

V-Synth XT SOLO 'vpdt' (vpfl version "0102", written by the XT's own
encoder; Session 16 on the user's XT project Emu_test.prj):
  88-byte header — u32[10] = sample count × 16, u32[21] (byte 84) = record
  count; records from byte 88, same {u16 length, u16 info} format, ending in
  a (256, 1) terminator (16 samples).  Verified: records minus the
  terminator sum exactly to the sample count (sine 1761, vocal 461 records).
  LITE waves (vpsc byte 1 = 0) carry no vpdt at all.
"""
import struct, os
def chunks(p):
    d=open(os.path.expanduser(p),'rb').read(); i=12; c={}
    while i+8<=len(d):
        cid=d[i:i+4]; sz=struct.unpack('<I',d[i+4:i+8])[0]; c[cid.decode('latin1')]=d[i+8:i+8+sz]; i+=8+sz+(sz&1)
    return c
def solo_records(p):
    v=chunks(p)['vpdt']; u=struct.unpack('<%dH'%(len(v)//2),v)
    recs=[]; pos=0.0; acc=0
    for i in range(58,len(u)-1,2):
        L,info=u[i],u[i+1]
        acc+=L
        if L==0xFFFF: continue
        recs.append(dict(start=pos/16.0, len=acc/16.0, info=info)); pos+=acc; acc=0
    return recs

def xt_solo_records(p):
    """Records of a V-Synth XT SOLO wave (vpfl 0102), terminator dropped."""
    v=chunks(p)['vpdt']; u=struct.unpack('<%dH'%(len(v)//2),v)
    cnt=struct.unpack('<I',v[84:88])[0]
    recs=[]; pos=0; acc=0
    for k in range(cnt):
        L,info=u[44+2*k],u[45+2*k]; acc+=L
        if L==0xFFFF: continue
        recs.append(dict(start=pos/16.0, len=acc/16.0, info=info)); pos+=acc; acc=0
    if recs and recs[-1]['len']==16 and recs[-1]['info']==1: recs.pop()
    return recs

if __name__ == '__main__':
    # Write VSE_CUTSFILE lines ("start len voiced info") for a SOLO wave.
    import sys
    p=sys.argv[1]
    ver=chunks(p).get('vpfl',b'')[:4]
    for r in (xt_solo_records(p) if ver==b'0102' else solo_records(p)):
        print('%.4f %.4f %d %d'%(r['start'],r['len'],1 if r['info']&7==7 else 0,r['info']))

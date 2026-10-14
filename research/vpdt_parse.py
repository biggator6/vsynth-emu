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

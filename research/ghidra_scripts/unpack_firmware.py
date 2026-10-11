import sys, zlib, struct, os
src, outdir = sys.argv[1], sys.argv[2]
os.makedirs(outdir, exist_ok=True)
d = open(src,'rb').read()
print(src, len(d), 'header:', d[:0x18].hex())
# scan for zlib streams
i=0; n=0
while True:
    j = d.find(b'\x78\xda', i)
    if j < 0: break
    try:
        do = zlib.decompressobj()
        out = do.decompress(d[j:])
        used = len(d[j:]) - len(do.unused_data)
        print(f'stream {n} @0x{j:x} comp {used} -> {len(out)}  pre-header {d[max(0,j-0x18):j].hex()}')
        open(f'{outdir}/{os.path.basename(src)}.{n}.bin','wb').write(out)
        n+=1; i = j+used
    except Exception as e:
        i = j+2

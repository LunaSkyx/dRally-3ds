import os, sys, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bpa import *
from PIL import Image
import collections
tr = sys.argv[1]
out = sys.argv[2]
os.makedirs(out, exist_ok=True)
e = read_bpa(tr)
name = [k for k in e if k.endswith('-INF.BIN')][0][:3]
inf = e[name + '-INF.BIN']
w, h, unk = struct.unpack_from('<3I', inf, 0)
print('INF', name, 'w', w, 'h', h, 'unk', unk)
for i in range(4):
    print(' start', i, struct.unpack_from('<3I', inf, 12 + 12 * i))
print(' powerups', [struct.unpack_from('<2I', inf, 60 + 8 * i) for i in range(16)])
print(' spectators', [struct.unpack_from('<4I', inf, 188 + 16 * i) for i in range(20)][:6])
def pad(b,n): return b + bytes(max(0,n-len(b)))
ima = pad(bpk_decode(e[name + '-IMA.BPK']), 0x30a+w*h)
print('IMA', len(ima), 'hdr', ima[:10].hex(), 'expected', 0x30a + w * h)
pal = ima[10:10 + 768]
img = Image.frombytes('P', (w, h), ima[0x30a:0x30a + w * h])
# palette: game uses 6 bit VGA values?
mx = max(pal)
print('pal max', mx)
pp = bytes(min(255, v * 4) for v in pal) if mx < 64 else pal
img.putpalette(pp)
img.convert('RGB').save(out + '/ima.png')
mas = pad(bpk_decode(e[name + '-MAS.BPK']), 0x30a+w*h)
print('MAS', len(mas), 'hdr', mas[:10].hex())
m = mas[0x30a:0x30a + w * h]
print(' mask low-nibble hist', sorted(collections.Counter(b & 15 for b in m[::7]).items()))
print(' mask high-nibble hist', sorted(collections.Counter(b >> 4 for b in m[::7]).items()))
cols = [(0,0,0),(0,0,255),(0,255,255),(255,255,0),(255,0,0),(255,0,255),(128,64,0),(0,128,0),(128,128,128),(255,128,0),(0,255,0),(255,255,255),(200,200,100),(100,100,255),(255,100,100),(90,90,90)]
pm = Image.frombytes('P', (w, h), bytes(b & 15 for b in m))
pm.putpalette(bytes(c for t in cols for c in t))
pm.convert('RGB').save(out + '/mas.png')
for nm in ('VAI', 'LR1'):
    d = pad(bpk_decode(e[name + '-' + nm + '.BPK']), 0x30a+(w//4)*(h//4))
    qw, qh = w // 4, h // 4
    print(nm, len(d), 'hdr', d[:10].hex(), 'expected', 0x30a + qw * qh)
    dd = d[0x30a:0x30a + qw * qh]
    print(' hist', sorted(collections.Counter(dd).items())[:40])
    Image.frombytes('L', (qw, qh), dd).save(out + '/%s.png' % nm.lower())
sha = bpk_decode(e[name + '-SHA.BPK']); print('SHA', len(sha), struct.unpack_from('<2I', sha, 0))
for t in ('SKI', 'BLO', 'LIT'):
    print(t, e[name + '-' + t + '.TAB'][:16].hex())
drv = e[name + '-DRV.DAT']; print('DRV first floats', struct.unpack_from('<8f', drv, 0))
ohi = e[name + '-OHI.DAT']; print('OHI first ints', struct.unpack_from('<8I', ohi, 0))
sce = bpk_decode(e[name + '-SCE.BPK']); print('SCE', len(sce), 'objects', sce[0])

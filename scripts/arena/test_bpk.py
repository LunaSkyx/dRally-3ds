"""round trip + real data: python -I test_bpk.py [path\to\TRn.BPA]"""
import os, sys, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bpa import *

random.seed(1)
for n in (1, 5, 300, 5000, 70000):
    raw = bytes(random.choice((0, 0, 0, 7, 255, random.randrange(256))) for _ in range(n))
    assert bpk_decode(bpk_encode(raw)) == raw, n
print('round trip ok')
if len(sys.argv) > 1:
    e = read_bpa(sys.argv[1])
    for k, v in e.items():
        d = bpk_decode(v) if k.endswith('.BPK') else v
        print('%-14s stored %7d  raw %7d' % (k, len(v), len(d)))

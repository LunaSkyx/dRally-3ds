"""bpa.py - read and write Death Rally .BPA archives and .BPK (LZW) entries.

Formats (found in bpa.c / bpk.c of this port):
  BPA   u32 count, then 255 x (13 byte name, u32 size), then the entries' data back to back.
        Name byte n of an entry is stored as  char + (0x75 - 3*n)  (mod 256); the name is zero padded to 13 bytes.
  BPK   a variable width LZW stream: codes start 9 bits wide and grow to 12; 0x100 ends the stream, 0x101 resets the
        dictionary; every output byte is rotated right by 3.  The encoder below only ever writes literal codes (plus
        resets), so it never needs a dictionary - the stream is ~13 % bigger than the game's, and the game's decoder
        reads it just the same (tests: scripts/arena/test_bpk.py).
"""
import struct


def _enc_name(name):
    b = name.upper().encode('ascii')
    out = bytearray(13)
    for n, c in enumerate(b):
        out[n] = (c + 0x75 - 3 * n) & 0xFF
    return bytes(out)


def _dec_name(raw):
    out = bytearray()
    for n in range(13):
        if raw[n] == 0:
            break
        out.append((raw[n] - (0x75 - 3 * n)) & 0xFF)
    return out.decode('ascii', 'replace')


def read_bpa(path):
    """-> dict name -> bytes (as stored: a .BPK entry is still compressed)"""
    data = open(path, 'rb').read()
    n = struct.unpack_from('<I', data, 0)[0] & 0xFF
    pos = 4 + 255 * 17
    out = {}
    for i in range(n):
        raw = data[4 + 17 * i: 4 + 17 * i + 13]
        size = struct.unpack_from('<I', data, 4 + 17 * i + 13)[0]
        out[_dec_name(raw)] = data[pos: pos + size]
        pos += size
    return out


def write_bpa(path, entries):
    """entries: dict name -> bytes (or an ordered list of pairs)"""
    items = list(entries.items()) if isinstance(entries, dict) else list(entries)
    assert 0 < len(items) <= 255
    head = struct.pack('<I', len(items))
    for name, blob in items:
        assert len(blob) > 0, name
        head += _enc_name(name) + struct.pack('<I', len(blob))
    head += bytes(17 * (255 - len(items)))
    with open(path, 'wb') as f:
        f.write(head)
        for _, blob in items:
            f.write(blob)


def bpk_decode(src):
    """the game's decoder (bpk.c), straight port"""
    pos = 0          # bit position
    nbits, idmax, nid = 9, 0x200, 0x101
    dic = {}
    out = bytearray()
    prev = 0
    first = True

    def rd(n):
        nonlocal pos
        a, d = divmod(pos, 8)
        v = 0
        for k in range(3):
            if a + k < len(src):
                v |= src[a + k] << (8 * k)
        pos += n
        return (v >> d) & ((1 << n) - 1)

    def expand(code):
        s = bytearray()
        while code > 0xFF:
            e = dic[code]
            s.append(e[1])
            code = e[0]
        s.append(code)
        s.reverse()
        return s

    nid = 0x101
    while True:
        code = rd(nbits)
        if code == 0x100:
            break
        if code == 0x101:
            nbits, idmax, nid = 9, 0x200, 0x101
            dic.clear()
            continue
        # mirror of the game's dictionary bookkeeping: entry nid = (prev, first byte of this string)
        s = expand(code) if (code <= 0xFF or code < nid) else None
        if s is None:                      # the KwKwK case: string = prev string + its own first byte
            ps = expand(prev)
            s = ps + ps[:1]
        dic[nid] = (prev, s[0])
        prev = code
        out += s
        old = nid
        nid += 1
        if old != 0x101 and nid >= idmax and nbits != 12:
            nbits += 1
            idmax <<= 1
    return bytes(bytearray(((b >> 3) | (b << 5)) & 0xFF for b in out))


def bpk_encode(raw):
    """a real LZW encoder with the game's code widths (9..12 bits, 0x101 resets when the table is full)"""
    buf = bytearray()
    acc = 0
    nacc = 0

    def put(code, n):
        nonlocal acc, nacc
        acc |= code << nacc
        nacc += n
        while nacc >= 8:
            buf.append(acc & 0xFF)
            acc >>= 8
            nacc -= 8

    # the decoder rotates every output byte right by 3, so the encoder feeds it the byte rotated LEFT by 3
    data = bytes(((b << 3) | (b >> 5)) & 0xFF for b in raw)
    nbits, idmax, nid = 9, 0x200, 0x101      # the DECODER's state: its table index and code width
    put(0x101, 9)
    table = {}
    w = -1                                   # current string as a code (-1 = empty)
    k = 0                                    # codes emitted since the last reset

    def emitted(code):
        """put a data code and advance the decoder's mirror: it adds one table entry per code it reads"""
        nonlocal nbits, idmax, nid, k
        put(code, nbits)
        k += 1
        old = nid
        nid += 1
        if old != 0x101 and nid >= idmax and nbits != 12:
            nbits += 1
            idmax <<= 1

    for c in data:
        if w < 0:
            w = c
            continue
        key = (w << 8) | c
        nxt = table.get(key)
        if nxt is not None:
            w = nxt
            continue
        emitted(w)
        table[key] = nid                     # = the entry the decoder makes when it reads the NEXT code
        if nid >= 0xFFF:                     # table full: reset
            put(0x101, nbits)
            nbits, idmax, nid = 9, 0x200, 0x101
            table = {}
            k = 0
        w = c
    if w >= 0:
        put(w, nbits)
    put(0x100, nbits)
    if nacc:
        buf.append(acc & 0xFF)
    buf += bytes(8)
    return bytes(buf)

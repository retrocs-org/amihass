#!/usr/bin/env python3
"""LHA -lh5- compression (LZSS with an 8 KiB window and static Huffman
blocks), written for the release tools so AmiHASS.lha is compressed without
an external archiver. Follows the bit layout of LHa for UNIX (huf.c):
block size, the code-length table (NT=19), the literal/length table
(NC=510), the position table (NP=14), then the codes, most significant bit
first. Decoders: any LhA/LZX/lha on the Amiga, lhasa, lhafile."""

DICBIT = 13
DICSIZ = 1 << DICBIT
MAXMATCH = 256
THRESHOLD = 3
NC = 255 + MAXMATCH + 2 - THRESHOLD  # 510
NT = 19
TBIT = 5
CBIT = 9
NP = DICBIT + 1
PBIT = 4
BLOCK = 16384  # codes per block (the size field is 16 bits)


class Bits:
    def __init__(self):
        self.out = bytearray()
        self.acc = 0
        self.n = 0

    def put(self, count, value):
        if count == 0:
            return
        self.acc = (self.acc << count) | (value & ((1 << count) - 1))
        self.n += count
        while self.n >= 8:
            self.n -= 8
            self.out.append((self.acc >> self.n) & 0xFF)
        self.acc &= (1 << self.n) - 1 if self.n else 0

    def finish(self):
        if self.n:
            self.out.append((self.acc << (8 - self.n)) & 0xFF)
            self.acc = self.n = 0
        return bytes(self.out)


def code_lengths(freq, limit=16):
    """Huffman code lengths, at most `limit` bits; zero for unused symbols."""
    used = [i for i, f in enumerate(freq) if f]
    lengths = [0] * len(freq)
    if len(used) <= 1:
        return lengths, (used[0] if used else 0)
    weights = list(freq)
    while True:
        import heapq
        heap = [(weights[i], i, (i,)) for i in used]
        heapq.heapify(heap)
        depth = {i: 0 for i in used}
        serial = len(freq)
        while len(heap) > 1:
            w1, _, a = heapq.heappop(heap)
            w2, _, b = heapq.heappop(heap)
            for s in a + b:
                depth[s] += 1
            heapq.heappush(heap, (w1 + w2, serial, a + b))
            serial += 1
        if max(depth.values()) <= limit:
            for s, d in depth.items():
                lengths[s] = d
            return lengths, None
        weights = [(w + 1) // 2 if w else 0 for w in weights]  # flatten and retry


def canonical(lengths):
    count = [0] * 18
    for l in lengths:
        count[l] += 1
    start = [0] * 18
    for i in range(1, 17):
        start[i + 1] = (start[i] + count[i]) << 1
    codes = [0] * len(lengths)
    for i, l in enumerate(lengths):
        if l:
            codes[i] = start[l]
            start[l] += 1
    return codes


def p_bits(p):
    return p.bit_length()


def tokens(data):
    """LZSS with hash chains: literals (int) and (length, distance-1) pairs."""
    out = []
    n = len(data)
    head = {}
    prev = [-1] * n
    i = 0

    def insert(pos):
        if pos + 2 < n:
            key = data[pos:pos + 3]
            prev[pos] = head.get(key, -1)
            head[key] = pos

    while i < n:
        best_len, best_dist = 0, 0
        if i + 2 < n:
            cand = head.get(data[i:i + 3], -1)
            tries = 128
            limit = min(MAXMATCH, n - i)
            while cand >= 0 and i - cand <= DICSIZ and tries:
                if data[cand + best_len] == data[i + best_len] if best_len < limit else False:
                    l = 0
                    while l < limit and data[cand + l] == data[i + l]:
                        l += 1
                    if l > best_len:
                        best_len, best_dist = l, i - cand
                        if l == limit:
                            break
                cand = prev[cand]
                tries -= 1
        if best_len >= THRESHOLD:
            out.append((best_len, best_dist - 1))
            for k in range(best_len):
                insert(i + k)
            i += best_len
        else:
            out.append(data[i])
            insert(i)
            i += 1
    return out


def write_pt_len(bits, lengths, n, nbit, special):
    while n > 0 and lengths[n - 1] == 0:
        n -= 1
    bits.put(nbit, n)
    i = 0
    while i < n:
        k = lengths[i]
        i += 1
        if k <= 6:
            bits.put(3, k)
        else:
            bits.put(k - 3, (1 << (k - 3)) - 2)
        if i == special:
            while i < 6 and lengths[i] == 0:
                i += 1
            bits.put(2, i - 3)


def send_block(bits, block):
    c_freq = [0] * NC
    p_freq = [0] * NP
    for t in block:
        if isinstance(t, int):
            c_freq[t] += 1
        else:
            c_freq[t[0] - THRESHOLD + 256] += 1
            p_freq[p_bits(t[1])] += 1
    bits.put(16, len(block))
    c_len, c_single = code_lengths(c_freq)
    if c_single is None:
        # code lengths of the C table, themselves Huffman coded (T table)
        t_freq = [0] * NT
        n = NC
        while n > 0 and c_len[n - 1] == 0:
            n -= 1
        runs = []
        i = 0
        while i < n:
            k = c_len[i]
            i += 1
            if k == 0:
                count = 1
                while i < n and c_len[i] == 0:
                    i += 1
                    count += 1
                runs.append(('zero', count))
                if count <= 2:
                    t_freq[0] += count
                elif count <= 18:
                    t_freq[1] += 1
                elif count == 19:
                    t_freq[0] += 1
                    t_freq[1] += 1
                else:
                    t_freq[2] += 1
            else:
                runs.append(('len', k))
                t_freq[k + 2] += 1
        t_len, t_single = code_lengths(t_freq)
        if t_single is None:
            write_pt_len(bits, t_len, NT, TBIT, 3)
            t_code = canonical(t_len)
        else:
            bits.put(TBIT, 0)
            bits.put(TBIT, t_single)
            t_len = [0] * NT
            t_code = [0] * NT
        bits.put(CBIT, n)
        for kind, value in runs:
            if kind == 'len':
                bits.put(t_len[value + 2], t_code[value + 2])
            elif value <= 2:
                for _ in range(value):
                    bits.put(t_len[0], t_code[0])
            elif value <= 18:
                bits.put(t_len[1], t_code[1])
                bits.put(4, value - 3)
            elif value == 19:
                bits.put(t_len[0], t_code[0])
                bits.put(t_len[1], t_code[1])
                bits.put(4, 15)
            else:
                bits.put(t_len[2], t_code[2])
                bits.put(CBIT, value - 20)
        c_code = canonical(c_len)
    else:
        bits.put(TBIT, 0)
        bits.put(TBIT, 0)
        bits.put(CBIT, 0)
        bits.put(CBIT, c_single)
        c_len = [0] * NC
        c_code = [0] * NC
    p_len, p_single = code_lengths(p_freq)
    if p_single is None:
        write_pt_len(bits, p_len, NP, PBIT, -1)
        p_code = canonical(p_len)
    else:
        bits.put(PBIT, 0)
        bits.put(PBIT, p_single)
        p_len = [0] * NP
        p_code = [0] * NP
    for t in block:
        if isinstance(t, int):
            bits.put(c_len[t], c_code[t])
        else:
            c = t[0] - THRESHOLD + 256
            bits.put(c_len[c], c_code[c])
            pb = p_bits(t[1])
            bits.put(p_len[pb], p_code[pb])
            if pb > 1:
                bits.put(pb - 1, t[1])


def compress(data):
    """Raw -lh5- stream for data (the LHA header stores the sizes)."""
    data = bytes(data)
    if not data:
        return b''
    toks = tokens(data)
    bits = Bits()
    for start in range(0, len(toks), BLOCK):
        send_block(bits, toks[start:start + BLOCK])
    return bits.finish()


class _Reader:
    def __init__(self, data):
        self.data = data
        self.pos = 0  # in bits

    def get(self, count):
        value = 0
        for _ in range(count):
            byte = self.data[self.pos >> 3] if (self.pos >> 3) < len(self.data) else 0
            value = (value << 1) | ((byte >> (7 - (self.pos & 7))) & 1)
            self.pos += 1
        return value


class _Table:
    """Canonical Huffman decoding; a table with one symbol reads no bits."""
    def __init__(self, lengths=None, single=None):
        self.single = single
        self.map = {}
        if lengths is not None:
            for sym, code in enumerate(canonical(lengths)):
                if lengths[sym]:
                    self.map[(lengths[sym], code)] = sym

    def decode(self, reader):
        if self.single is not None:
            return self.single
        code = length = 0
        while length < 17:
            code = (code << 1) | reader.get(1)
            length += 1
            sym = self.map.get((length, code))
            if sym is not None:
                return sym
        raise ValueError('bad -lh5- code')


def _read_pt(reader, nn, nbit, special):
    n = reader.get(nbit)
    if n == 0:
        return _Table(single=reader.get(nbit))
    if n > nn:
        raise ValueError('bad -lh5- table')
    lengths = [0] * nn
    i = 0
    while i < n:
        c = reader.get(3)
        if c == 7:
            while reader.get(1):
                c += 1
        lengths[i] = c
        i += 1
        if i == special:
            i += reader.get(2)
    return _Table(lengths)


def decompress(data, size):
    """Inflate a raw -lh5- stream to `size` bytes (used to verify archives)."""
    out = bytearray()
    reader = _Reader(data)
    while len(out) < size:
        count = reader.get(16)
        if not count:
            raise ValueError('empty -lh5- block')
        t = _read_pt(reader, NT, TBIT, 3)
        n = reader.get(CBIT)
        if n == 0:
            c_table = _Table(single=reader.get(CBIT))
        else:
            if n > NC:
                raise ValueError('bad -lh5- table')
            lengths = [0] * NC
            i = 0
            while i < n:
                c = t.decode(reader)
                if c <= 2:
                    i += 1 if c == 0 else reader.get(4) + 3 if c == 1 else reader.get(CBIT) + 20
                else:
                    lengths[i] = c - 2
                    i += 1
            c_table = _Table(lengths)
        p_table = _read_pt(reader, NP, PBIT, -1)
        for _ in range(count):
            c = c_table.decode(reader)
            if c < 256:
                out.append(c)
            else:
                length = c - 256 + THRESHOLD
                pb = p_table.decode(reader)
                dist = pb if pb <= 1 else (1 << (pb - 1)) | reader.get(pb - 1)
                start = len(out) - dist - 1
                if start < 0:
                    raise ValueError('bad -lh5- distance')
                for k in range(length):
                    out.append(out[start + k])
            if len(out) > size:
                raise ValueError('-lh5- data longer than its header says')
    return bytes(out)

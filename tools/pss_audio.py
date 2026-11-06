#!/usr/bin/env python3
"""The sound of a PS2 .pss (MPEG-PS with Sony's SShd/SSbd audio in private
stream 1) as a WAV: pss_audio.py in.pss out.wav"""
import struct, sys, wave
d = open(sys.argv[1], 'rb').read()
pay = bytearray(); i = 0
while True:
    i = d.find(b'\x00\x00\x01', i)
    if i < 0 or i + 6 > len(d): break
    sid = d[i + 3]
    if sid == 0xBD or sid == 0xBF or 0xC0 <= sid <= 0xEF:
        L = int.from_bytes(d[i + 4:i + 6], 'big')
        if sid == 0xBD:
            hl = d[i + 8]
            p = d[i + 9 + hl:i + 6 + L]
            pay += p[4:]          # 4-byte substream header (ff a0 ..)
        i += 6 + L; continue
    i += 3
assert pay[:4] == b'SShd', pay[:8]
hs = struct.unpack_from('<I', pay, 4)[0]
codec, rate, ch, inter = struct.unpack_from('<IIII', pay, 8)
j = 8 + hs
assert pay[j:j + 4] == b'SSbd', pay[j:j + 8]
size = struct.unpack_from('<I', pay, j + 4)[0]
data = pay[j + 8:j + 8 + size]
assert codec == 1, f'codec {codec} (only 16-bit PCM handled)'
out = bytearray(); blk = inter * ch
for k in range(0, len(data) - blk + 1, blk):
    chans = [data[k + c * inter:k + (c + 1) * inter] for c in range(ch)]
    for s in range(0, inter, 2):
        for c in range(ch):
            out += chans[c][s:s + 2]
w = wave.open(sys.argv[2], 'wb'); w.setnchannels(ch); w.setsampwidth(2); w.setframerate(rate); w.writeframes(bytes(out)); w.close()
print(f'{sys.argv[1]}: {rate} Hz, {ch} ch, {len(out) // (2 * ch) / rate:.1f} s')

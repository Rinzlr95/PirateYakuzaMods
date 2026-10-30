"""Build an experimental SRMM loose-file mod from the user's original archive.

Python 3 standard library only. The source archive is never modified.
Format references: Kaplas80/ParManager and Ret-HZ/LibARMP on GitHub.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile
import zlib

SOURCE_SHA256 = '8b3e6951ac3e94cd6e67cd73d5aadcbd2314ff47a3e1bb86e9ecca2fdbded132'
ROOT = Path(__file__).resolve().parents[1]


def extract(blob, wanted):
    assert blob[:6] == b'PARC\x02\x01', 'Unsupported PAR format'
    folders, _, count, records = struct.unpack_from('>4I', blob, 16)
    result = {}
    for i in range(count):
        start = 32 + folders * 64 + i * 64
        name = blob[start:start + 64].split(b'\0')[0].decode()
        if name not in wanted:
            continue
        flag, size, packed, low, _, high, _ = struct.unpack_from('>6IQ', blob, records + i * 32)
        offset = ((high << 32) | low) & 0x00ffffffffffffff
        data = blob[offset:offset + packed]
        if flag == 0x80000000:
            assert data[:6] == b'SLLZ\x00\x02'
            assert struct.unpack_from('<II', data, 8) == (size, packed)
            chunks, position, total = [], 16, 0
            while total < size:
                length = int.from_bytes(data[position:position + 3], 'big')
                expected = int.from_bytes(data[position + 3:position + 5], 'big') + 1
                end = position + (length & 0x7fffff)
                assert position + 5 <= end <= len(data)
                chunk = data[position + 5:end]
                chunk = chunk[:expected] if length & 0x800000 else zlib.decompress(chunk)
                assert len(chunk) == expected
                chunks.append(chunk)
                total += len(chunk)
                position = end
            data = b''.join(chunks)
        else:
            assert flag == 0
        assert len(data) == size and data[:4] == b'armp'
        result[name] = data
    assert set(result) == wanted, 'Missing target table'
    return result


def build(source, output):
    blob = source.read_bytes()
    if hashlib.sha256(blob).hexdigest() != SOURCE_SHA256:
        raise ValueError('Archive differs from the inspected build; inspect it before patching.')
    edits = json.loads((ROOT / 'research/flotsam-patches.json').read_text())
    originals = extract(blob, {e['file'] for e in edits})
    patched = {name: bytearray(data) for name, data in originals.items()}
    allowed = {name: set() for name in originals}
    for edit in edits:
        name, offset = edit['file'], edit['offset']
        fmt = '<' + edit['format']
        assert struct.unpack_from(fmt, patched[name], offset)[0] == edit['before']
        struct.pack_into(fmt, patched[name], offset, edit['after'])
        assert struct.unpack_from(fmt, patched[name], offset)[0] == edit['after']
        allowed[name].update(range(offset, offset + struct.calcsize(fmt)))
    for name, data in patched.items():
        assert len(data) == len(originals[name])
        changed = {i for i, (a, b) in enumerate(zip(originals[name], data)) if a != b}
        assert changed and changed <= allowed[name], 'Unintended byte changes'
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, data in patched.items():
            archive.writestr('Flotsam2x-Experimental/db.spr.en/' + name, data)
        archive.writestr('Flotsam2x-Experimental/patch-manifest.json', json.dumps(edits, indent=2))
        archive.write(ROOT / 'INSTALL-FLOTSAM.txt', 'INSTALL-FLOTSAM.txt')
    with zipfile.ZipFile(output) as archive:
        assert archive.testzip() is None
        for name, data in patched.items():
            assert archive.read('Flotsam2x-Experimental/db.spr.en/' + name) == data
    print(f'Built {output}: {len(edits)} validated field edits across {len(patched)} tables.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    build(args.archive, args.output)

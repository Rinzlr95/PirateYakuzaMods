"""Build the items-only flotsam loot test; no spawn or cash changes."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile

from build_flotsam import extract

ROOT = Path(__file__).resolve().parent
MOD = 'FlotsamLoot2x-ItemsOnly-Test'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def build(source, output):
    spec = json.loads((ROOT / 'flotsam-loot-patches.json').read_text())
    blob = source.read_bytes()
    require(hashlib.sha256(blob).hexdigest() == spec['source_sha256'],
            'Source archive differs from the inspected upload; inspect before patching.')
    original = extract(blob, {spec['table']})[spec['table']]
    patched = bytearray(original)
    allowed = set()
    require(len(spec['edits']) == 16, 'Unexpected edit count')
    for edit in spec['edits']:
        offset, fmt = edit['offset'], '<' + edit['format']
        require(struct.unpack_from(fmt, original, offset)[0] == edit['before'],
                'Baseline mismatch: ' + edit['item_name'])
        require(edit['after'] == 2 * edit['before'] > 0, 'Invalid multiplier')
        field_bytes = set(range(offset, offset + struct.calcsize(fmt)))
        require(not (field_bytes & allowed), 'Overlapping edits')
        allowed.update(field_bytes)
        struct.pack_into(fmt, patched, offset, edit['after'])
        require(struct.unpack_from(fmt, patched, offset)[0] == edit['after'],
                'Patched value mismatch')
    require(len(patched) == len(original), 'Table length changed')
    changed = {i for i, (a, b) in enumerate(zip(original, patched)) if a != b}
    require(changed and changed <= allowed, 'Unexpected byte changes')
    output.parent.mkdir(parents=True, exist_ok=True)
    payload_name = MOD + '/db.spr.en/' + spec['table']
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        archive.writestr(payload_name, patched)
        archive.writestr(MOD + '/patch-manifest.json', json.dumps(spec, indent=2))
        archive.writestr('INSTALL-FLOTSAM-LOOT.txt', 'FlotsamLoot2x\n\nDoubles item quantities from floating flotsam crates. Spawn rates, loot chances\nand unit selling prices stay the same.\n\nINSTALL\nExtract the mod folder into runtime\\media\\mods. The path should be:\nmods\\FlotsamLoot2x-ItemsOnly-Test\\db.spr.en\\reward_table.bin\nEnable it in ShinRyuModManager, regenerate the load order, and restart the game.\nThe outer folder can be renamed; db.spr.en must sit directly inside it.\n\nRequires ShinRyuModManager/Parless. Tested with English game text.\nConflicts with other mods replacing db.spr.en\\reward_table.bin.\nTo remove, disable it and regenerate the load order.\n\nThanks to the ShinRyuModManager, Parless and community modding-tool developers.\n')
    with zipfile.ZipFile(output) as archive:
        require(archive.testzip() is None, 'ZIP integrity failure')
        require(archive.read(payload_name) == patched, 'ZIP payload differs')
    output.with_suffix('.zip.sha256').write_text(
        hashlib.sha256(output.read_bytes()).hexdigest() + '  ' + output.name + '\n')
    print(f'Built {output}: 16 item quantity edits; no cash or spawn edits.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    build(args.archive, args.output)

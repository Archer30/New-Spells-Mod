"""Validate remapped provider PNGs and byte-identical animation packaging."""
from pathlib import Path
import json
import struct
import zipfile
import zlib

root = Path(__file__).resolve().parents[1]
assets = root / 'NewSpellsExpansion/assets'
package = root / 'dist/New Spells Expansion/Data'
manifest = json.loads((assets / 'spell-assets.json').read_text())
assert [s['spellId'] for s in manifest['spells']] == [96, 97]
with zipfile.ZipFile(package / 'NewSpellsExpansion_png_data.zip') as archive:
    members = {name.replace('\\', '/'): name for name in archive.namelist() if name.lower().endswith('.png')}
    assert len(members) == 8
    for spell in manifest['spells']:
        for icon in spell['icons']:
            frame = spell['spellId'] + (icon['def'] == 'SpellInt.def')
            name = f'Data/Defs/{icon["def"]}/0_{frame}.png'
            assert archive.read(members[name]) == (assets / icon['source']).read_bytes(), name

def member(path):
    data = path.read_bytes()
    assert data[:4] == b'LOD\0' and struct.unpack_from('<I', data, 8)[0] == 1
    name, offset, size, _, packed = struct.unpack_from('<16sIIII', data, 92)
    payload = zlib.decompress(data[offset:offset+packed]) if packed else data[offset:offset+size]
    assert len(payload) == size
    return name.rstrip(b'\0').lower(), payload

name, animation = member(package / 'NewSpellsExpansion.pac')
assert name == b'nsebliz.def'
assert animation == member(assets / 'provenance/blizzard.original.pac')[1]
assert animation == (assets / 'source/NSEBLIZ.def').read_bytes()
print('Validated 8 exact PNG overrides and byte-identical Blizzard DEF animation.')

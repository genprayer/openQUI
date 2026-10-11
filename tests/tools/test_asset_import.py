"""Exercise RCC decoding and all-or-nothing import with synthetic resources."""
import importlib.util
import json
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

MODULE_PATH = Path(__file__).resolve().parents[2] / 'tools/assets/import_library.py'
SPEC = importlib.util.spec_from_file_location('asset_import', MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def bundle(name='example.png', payload=b'image', flags=0):
    encoded_name = name.encode('utf-16-be')
    names = struct.pack('>HI', len(name), 0) + encoded_name
    root = struct.pack('>IHII', 0, 2, 1, 1)
    file_entry = struct.pack('>IHHHI', 0, flags, 0, 0, 0)
    data = struct.pack('>I', len(payload)) + payload
    tree_offset = 20
    names_offset = tree_offset + len(root) + len(file_entry)
    data_offset = names_offset + len(names)
    return b'qres' + struct.pack('>IIII', 1, tree_offset, data_offset, names_offset) + root + file_entry + names + data


class AssetImportTests(unittest.TestCase):
    def test_decodes_raw_and_zlib_payloads(self):
        self.assertEqual(list(MODULE.resource_files(bundle())), [('example.png', b'image')])
        payload = struct.pack('>I', 5) + zlib.compress(b'image')
        self.assertEqual(list(MODULE.resource_files(bundle(payload=payload, flags=1))), [('example.png', b'image')])

    def test_rejects_traversal_invalid_offsets_and_cycles(self):
        for name in ['..', 'a/b', 'a\\b', '']:
            with self.assertRaises(ValueError):
                list(MODULE.resource_files(bundle(name=name)))
        broken = bytearray(bundle())
        struct.pack_into('>I', broken, 8, len(broken) + 1)
        with self.assertRaises(ValueError):
            list(MODULE.resource_files(bytes(broken)))
        cyclic = bytearray(bundle())
        struct.pack_into('>I', cyclic, 30, 0)
        with self.assertRaises(ValueError):
            list(MODULE.resource_files(bytes(cyclic)))

    def test_rejects_size_mismatch_and_oversized_decompression(self):
        for expected in [1, MODULE.MAX_FILE_BYTES + 1]:
            payload = struct.pack('>I', expected) + zlib.compress(b'image')
            with self.assertRaises(ValueError):
                list(MODULE.resource_files(bundle(payload=payload, flags=1)))

    def test_imports_arbitrary_models_reproducibly_without_executable_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            devices = root / 'installation/devices'
            devices.mkdir(parents=True)
            manifest = json.dumps({'common': {'UserModelName': 'Future headset', 'DeviceType': 'Headset'}}).encode()
            (devices / 'future_headset.rcc').write_bytes(bundle('future_headset_manifest.cuebmf', manifest))
            (devices / 'other_keyboard.rcc').write_bytes(bundle())
            (devices / 'code.rcc').write_bytes(bundle('script.qml', b'do not execute'))
            output = root / 'library'
            first = MODULE.import_library(devices.parent, output)
            second = MODULE.import_library(devices.parent, output)
            self.assertEqual(first, second)
            self.assertEqual(first['models'][0]['id'], 'future_headset')
            self.assertFalse(first['isCompleteCorsairCatalog'])
            self.assertFalse(list((output / 'assets').rglob('*.qml')))
            self.assertNotIn(str(root), json.dumps(first))

    def test_bad_new_bundle_preserves_previous_import(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            devices = root / 'installation/devices'
            devices.mkdir(parents=True)
            path = devices / 'valid.rcc'
            path.write_bytes(bundle())
            output = root / 'library'
            MODULE.import_library(devices.parent, output)
            previous = (output / 'index.json').read_bytes()
            path.write_bytes(b'broken')
            with self.assertRaises(ValueError):
                MODULE.import_library(devices.parent, output)
            self.assertEqual((output / 'index.json').read_bytes(), previous)

    def test_rejects_symlinked_input(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / 'target.rcc'
            target.write_bytes(bundle())
            link = root / 'link.rcc'
            link.symlink_to(target)
            with self.assertRaises(ValueError):
                MODULE.read_bounded(link)


if __name__ == '__main__':
    unittest.main()

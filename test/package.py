#!/usr/bin/env python3
"""Check install paths, file permissions, ARM ABI and Qt dependencies."""
import argparse
from pathlib import Path, PurePosixPath
import re
import struct
import subprocess
import tarfile
import tempfile


def check(package, qt, readelf):
    library = 'usr/local/Kobo/imageformats/libnm.so' if qt == 5 else 'imageformats/libnm.so'
    expected = {library}
    if qt == 5:
        expected.add('mnt/onboard/.adds/nm/doc')
    with tarfile.open(package) as archive:
        entries = archive.getmembers()
        names = [str(PurePosixPath(entry.name)) for entry in entries]
        if set(names) != expected or len(names) != len(expected):
            raise ValueError(f'unexpected package paths: {names}')
        if any(not entry.isfile() or entry.mode & 0o022 or entry.uid or entry.gid for entry in entries):
            raise ValueError('package files must be root-owned and not group/other writable')
        data = archive.extractfile(entries[names.index(library)]).read()
    if len(data) < 52 or data[:6] != b'\x7fELF\x01\x01' or struct.unpack_from('<H', data, 18)[0] != 40:
        raise ValueError('plugin must be a little-endian ELF32 ARM library')
    if not struct.unpack_from('<I', data, 36)[0] & 0x400:
        raise ValueError('plugin must use the ARM hard-float ABI')
    if b'.nm-test-request' in data or b'nm_test_start' in data:
        raise ValueError('test driver found in release plugin')
    with tempfile.TemporaryDirectory(prefix='nm-package-') as folder:
        path = Path(folder) / 'libnm.so'
        path.write_bytes(data)
        dynamic = subprocess.run([readelf, '-d', str(path)], check=True, text=True, capture_output=True).stdout
    dependencies = re.findall(r'\(NEEDED\).*\[([^]]+)\]', dynamic)
    required = {f'libQt{qt}{module}.so.{qt}' for module in ('Core', 'Gui', 'Widgets')}
    if not required.issubset(dependencies) or any(name.startswith(f'libQt{11-qt}') for name in dependencies):
        raise ValueError(f'wrong Qt dependencies: {dependencies}')
    print(f'{package}: package paths, permissions, ARM hard-float and Qt{qt} dependencies passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('--qt', type=int, choices=(5, 6), required=True)
    parser.add_argument('--readelf', default='readelf')
    args = parser.parse_args()
    check(args.package, args.qt, args.readelf)

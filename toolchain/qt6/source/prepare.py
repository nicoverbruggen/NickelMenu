#!/usr/bin/env python3
"""Prepare upstream Qt with the local patches from Kobo's published packages-v5 source."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import urllib.request

SOURCE = Path(__file__).resolve().parent


def prepare(destination, archive=None, source=SOURCE):
    spec = json.loads((source / 'source.json').read_text())
    destination = Path(destination).resolve()
    if destination.exists():
        raise ValueError(f'Refusing to replace existing source tree: {destination}')
    destination.parent.mkdir(parents=True, exist_ok=True)
    # A failed download or patch must not leave a tree that the next build reuses.
    with tempfile.TemporaryDirectory(prefix='qt-source-', dir=destination.parent) as temporary:
        temporary = Path(temporary)
        if archive is None:
            archive = temporary / 'qtbase.tar.xz'
            print(f'Downloading {spec["url"]}', flush=True)
            with urllib.request.urlopen(spec['url'], timeout=60) as response, archive.open('wb') as output:
                shutil.copyfileobj(response, output)
        archive = Path(archive).resolve()
        digest = hashlib.sha256()
        with archive.open('rb') as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                digest.update(chunk)
        if digest.hexdigest() != spec['sha256']:
            raise ValueError('Qt source archive checksum mismatch')
        subprocess.run(['tar', '-xf', str(archive), '-C', str(temporary)], check=True)
        tree = temporary / spec['directory']
        for name in spec['patches']:
            with (source / name).open('rb') as patch:
                subprocess.run(['patch', '--batch', '--forward', '--fuzz=0', '-p1'],
                               cwd=tree, stdin=patch, check=True)
        tree.rename(destination)
    print(f'Prepared Kobo Qt {spec["version"]}: {destination}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--archive', type=Path, help='Use a cached upstream archive without downloading')
    args = parser.parse_args()
    prepare(args.destination, args.archive)


if __name__ == '__main__':
    main()

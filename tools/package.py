#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Package the Wii app and license notices for installation on an SD card."""
import argparse
import hashlib
from pathlib import Path
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parent.parent


def package(build, out):
    boot = build / 'boot.dol'
    if b'PAP1\x43\x00\x00\x00' in boot.read_bytes():
        raise ValueError('Refusing a build with embedded game data; package make wii, not make full.')

    meta = ROOT / 'assets/meta.xml'
    ET.parse(meta)
    notices = [ROOT / 'LICENSE', *sorted((ROOT / 'licenses').glob('*.txt'))]
    out.mkdir(parents=True, exist_ok=True)
    target = out / 'Paper-Airplane-Chase-GX.zip'
    # Shop installs extract the zip to the SD root, so everything lives in the app folder.
    app = 'apps/paperplane/'
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        archive.write(ROOT / 'README.md', app + 'README.md')
        for path in notices:
            archive.write(path, app + path.relative_to(ROOT).as_posix())
        archive.write(boot, app + 'boot.dol')
        archive.write(ROOT / 'assets/icon.png', app + 'icon.png')
        archive.write(meta, app + 'meta.xml')
        archive.writestr(
            app + 'INSTALL.txt',
            '1. Copy apps/ to the root of your SD card.\n'
            '2. Put your USA .nds ROM in apps/paperplane/, beside boot.dol. Any filename is fine.\n'
            '3. Launch Paper Airplane Chase GX in the Homebrew Channel.\n'
            'Game data is prepared automatically on first launch; unzip the ROM first if needed.\n'
            'Keep scores.dat when updating.\n'
            'Source code and build tools are available in the project repository.\n')
    (out / 'SHA256SUMS').write_text(
        hashlib.sha256(target.read_bytes()).hexdigest() + '  ' + target.name + '\n')
    print(target, target.stat().st_size)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=Path.home() / '.cache/paperplane-build')
    parser.add_argument('--output', type=Path, default=ROOT / 'dist')
    args = parser.parse_args()
    package(args.build, args.output)

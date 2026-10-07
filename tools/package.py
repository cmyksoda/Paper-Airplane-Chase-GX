#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Package a Wii BYOR release, optionally with a separate private local install."""
import argparse
import hashlib
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = '1.0.0'
SOURCE_DIRS = ('source', 'assets', 'dev', 'tools', 'licenses', 'tests')
SOURCE_FILES = ('Makefile', 'LICENSE', 'README.md')


def add(z, name, data):
    info = zipfile.ZipInfo(name, (2026, 9, 30, 0, 0, 0))
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o100644 << 16
    z.writestr(info, data)


def package(build, output, pack=None, unpacked=False):
    boot = (build / 'boot.dol').read_bytes()
    if not boot or boot[:4] == b'\x7fELF':
        raise ValueError('Build the Wii boot.dol first')
    if b'PAP1\x43\x00\x00\x00' in boot:
        raise ValueError('Refusing an embedded-data build in the BYOR package')

    output.mkdir(parents=True, exist_ok=True)
    app = 'apps/paperplane/'
    runtime = {'boot.dol': boot,
               'icon.png': (ROOT/'assets/icon.png').read_bytes(),
               'meta.xml': (ROOT/'assets/meta.xml').read_bytes(),
               'LICENSE': (ROOT/'LICENSE').read_bytes()}
    for path in sorted((ROOT/'licenses').glob('*')):
        if path.is_file():
            runtime['licenses/'+path.name] = path.read_bytes()

    if unpacked:
        if pack:
            raise ValueError(
                'Unpacked release staging uses the BYOR executable without private game data')
        stage = output/f'Paper-Airplane-Chase-GX-v{VERSION}'
        stage.mkdir()  # Refuse to overwrite icon/metadata edits in an existing stage.
        for name, data in sorted(runtime.items()):
            dest = stage/app/name
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(data)
        (stage/'INSTALL.txt').write_text(
            f'Paper Airplane Chase GX {VERSION}\n\n'
            'Copy the apps folder to the root of your SD card.\n'
            'Put your unmodified USA game dump (.nds, .app or 00000001) beside boot.dol.\n'
            'Launch from the Homebrew Channel; game.pak is prepared automatically.\n'
            'Keep scores.dat when updating.\n\n'
            'Wii Remote: A selects upright grip; 1/2 selects sideways grip.\n'
            'Left/right chooses a game; up/down\n'
            'chooses a Time Attack course. A/2 starts; Plus/Start pauses.\n'
            'Pause menus use up/down and A/2; B goes back.\n'
            'Home/GameCube Z opens the menu or exits from the title.\n\n'
            'Endless/Time Attack: fresh input on another controller takes over.\n'
            'Race: the starter is Player 1. Press or move on another controller\n'
            'to join as Player 2 and begin the countdown. Assignments stay fixed\n'
            'until returning to the title.\n\n'
            'No game dump or extracted game data is included.\n')
        print(stage)
        return

    paths = [ROOT/p for p in SOURCE_FILES]
    for directory in SOURCE_DIRS:
        paths += [p for p in (ROOT/directory).rglob('*')
                  if p.is_file() and '__pycache__' not in p.parts and p.suffix != '.pyc']

    byor = output/f'Paper-Airplane-Chase-GX-v{VERSION}-BYOR.zip'
    with zipfile.ZipFile(byor, 'w') as z:
        for name, data in sorted(runtime.items()):
            add(z, app+name, data)
        add(z, 'README.md', (ROOT/'README.md').read_bytes())
        for path in sorted(paths):
            add(z, 'PaperAirplaneChaseGX-source/'+path.relative_to(ROOT).as_posix(),
                path.read_bytes())

    written = [byor]
    if pack:
        data = pack.read_bytes()
        if data[:8] != b'PAP1\x43\x00\x00\x00':
            raise ValueError('Invalid Paper Airplane Chase pack')
        local = output/f'Paper-Airplane-Chase-GX-v{VERSION}-Local.zip'
        with zipfile.ZipFile(local, 'w') as z:
            for name, value in sorted(runtime.items()):
                add(z, app+name, value)
            add(z, app+'game.pak', data)
            add(z, 'PRIVATE-LOCAL-BUILD.txt',
                b'Contains game data extracted from your own dump. Keep this package private.\n'
                b'Extract to the SD card root and launch from the Homebrew Channel.\n'
                b'Source and licenses accompany the separate BYOR package.\n')
            add(z, 'README.md', (ROOT/'README.md').read_bytes())
        written.append(local)

    manifest = ''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in written)
    (output/'SHA256SUMS.txt').write_text(manifest)
    print(manifest, end='')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=Path.home()/'.cache/paperplane-build')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--pack', type=Path)
    parser.add_argument('--unpacked', action='store_true',
                        help='Stage an editable runtime-only release folder without creating ZIPs')
    args = parser.parse_args()
    package(args.build.expanduser(), args.output.expanduser(),
            args.pack.expanduser() if args.pack else None, args.unpacked)

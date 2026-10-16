"""Package an existing Windows EXE without changing its bytes (stdlib only)."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--version', default='1.0.0')
    parser.add_argument('--output', type=Path, default=ROOT / 'dist')
    parser.add_argument('--expected-sha256')
    args = parser.parse_args()
    if not args.version or any(c not in '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-' for c in args.version):
        parser.error('Version must contain only letters, numbers, dots or hyphens')
    expected = sha256(args.exe)
    if args.expected_sha256 and expected != args.expected_sha256.lower():
        parser.error('Input EXE does not match the required SHA-256')
    output = args.output.resolve()
    package = output / 'PrinceOfPersia-Level1'
    if package.exists():
        parser.error('Package folder already exists; select a new output folder')
    package.mkdir(parents=True)
    sources = {
        'PrinceOfPersia-Level1.exe': args.exe,
        'CONTROLS.txt': ROOT / 'CONTROLS.txt',
        'PLAY.txt': ROOT / 'PLAY.txt',
        'docs/MANUAL.md': ROOT / 'docs/MANUAL.md',
        'docs/VERIFICATION.md': ROOT / 'docs/VERIFICATION.md',
        'docs/PROVENANCE.md': ROOT / 'docs/PROVENANCE.md',
        'LICENSE.md': ROOT / 'LICENSE.md',
        'THIRD-PARTY-NOTICES.txt': ROOT / 'THIRD-PARTY-NOTICES.txt',
        'SDL2-LICENSE.txt': ROOT / 'licenses/SDL2.txt',
        'licenses/SDL2.txt': ROOT / 'licenses/SDL2.txt',
    }
    for name, source in sources.items():
        (package / name).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, package / name)
    if sha256(package / 'PrinceOfPersia-Level1.exe') != expected:
        raise RuntimeError('Packaged executable hash mismatch')
    checksums = ''.join(f'{sha256(package / name)}  {name}\n' for name in sorted(sources))
    (package / 'SHA256SUMS.txt').write_text(checksums, encoding='utf-8', newline='\n')
    archive = output / f'PrinceOfPersia-Level1-v{args.version}-Windows-x64.zip'
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
        for path in sorted(package.rglob('*')):
            if path.is_file():
                bundle.write(path, arcname=f'{package.name}/{path.relative_to(package).as_posix()}')
    with zipfile.ZipFile(archive) as bundle:
        if bundle.testzip() is not None:
            raise RuntimeError('Archive CRC check failed')
        packaged = bundle.read(f'{package.name}/PrinceOfPersia-Level1.exe')
        if hashlib.sha256(packaged).hexdigest() != expected:
            raise RuntimeError('Archived executable hash mismatch')
    # Convenient individual release downloads alongside the complete ZIP.
    for name in ('PrinceOfPersia-Level1.exe', 'CONTROLS.txt'):
        shutil.copyfile(package / name, output / name)
    shutil.copyfile(package / 'docs/MANUAL.md', output / 'MANUAL.md')
    downloads = [archive, output / 'PrinceOfPersia-Level1.exe', output / 'CONTROLS.txt', output / 'MANUAL.md']
    (output / 'SHA256SUMS.txt').write_text(''.join(
        f'{sha256(path)}  {path.name}\n' for path in downloads), encoding='utf-8', newline='\n')
    print(json.dumps({'archive': archive.name, 'exe_sha256': expected,
                      'archive_sha256': sha256(archive), 'files': len(sources) + 1}))


if __name__ == '__main__':
    main()

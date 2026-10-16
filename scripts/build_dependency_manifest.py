"""Hash compiler-reported dependencies, including the external SDL SDK.

The resulting local build receipt may contain absolute dependency paths.
It belongs in ignored build output, not in the public repository.
"""
import hashlib
from pathlib import Path
import shlex
import sys

ROOT = Path(__file__).resolve().parents[1]


def dependencies(make_text):
    paths = set()
    for rule in make_text.replace('\\\n', ' ').splitlines():
        if ':' not in rule:
            continue
        for token in shlex.split(rule.split(':', 1)[1]):
            paths.add((ROOT / token).resolve(strict=True))
    return paths


def label(path):
    try:
        return path.relative_to(ROOT).as_posix()
    except ValueError:
        return path.as_posix()


if __name__ == '__main__':
    names = dependencies(Path(sys.argv[1]).read_text())
    names.update(Path(name).resolve(strict=True) for name in sys.argv[3:])
    Path(sys.argv[2]).write_text(''.join(
        f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {label(path)}\n'
        for path in sorted(names)))


"""Compile actual production host harnesses without local research metadata."""
import hashlib
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def sdl_arguments():
    cflags = os.environ.get('SDL2_CFLAGS')
    libraries = os.environ.get('SDL2_LIBS')
    if cflags is not None and libraries is not None:
        return shlex.split(cflags), shlex.split(libraries)
    if cflags is not None or libraries is not None:
        raise RuntimeError('Set both SDL2_CFLAGS and SDL2_LIBS, or neither')
    if shutil.which('sdl2-config'):
        command = ['sdl2-config']
    elif shutil.which('pkg-config'):
        command = ['pkg-config']
        probe = subprocess.run(command + ['--exists', 'sdl2'], check=False)
        if probe.returncode:
            raise RuntimeError('SDL2 development files not found; see tests/README.md')
    else:
        raise RuntimeError('SDL2 development files not found; see tests/README.md')
    suffix = [] if command[0] == 'sdl2-config' else ['sdl2']
    return tuple(shlex.split(subprocess.check_output(
        command + [option] + suffix, text=True)) for option in ['--cflags', '--libs'])


def source_hashes():
    return {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
            for folder in ['src', 'generated', 'tests', 'scripts']
            for path in sorted((ROOT / folder).iterdir())
            if path.is_file() and path.suffix in ['.c', '.h', '.py', '.list']}


def compile_harness(name):
    out = ROOT / 'build' / 'portable-tests'
    out.mkdir(parents=True, exist_ok=True)
    sources = [line.strip() for line in (ROOT / 'scripts/sources.list').read_text().splitlines()
               if line.strip() and not line.startswith('#') and line.strip() != 'src/windows_main.c']
    cflags, libraries = sdl_arguments()
    command = [os.environ.get('HOST_CC', 'gcc'), '-std=c17', '-O2', '-Wall', '-Wextra',
               '-Werror', '-pedantic', '-Isrc', *cflags, f'tests/{name}.c', *sources,
               *libraries, '-lm', '-o', str(out / name)]
    subprocess.run(command, cwd=ROOT, check=True, timeout=120)
    return out / name


def run_harness(binary, *args):
    environment = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    # Failed assertions should not leave core dumps containing game process data.
    def disable_core_dumps():
        import resource
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    return subprocess.run([str(binary), *map(str, args)], cwd=ROOT, env=environment,
                          capture_output=True, text=True, timeout=180,
                          preexec_fn=disable_core_dumps)


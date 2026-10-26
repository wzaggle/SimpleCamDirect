"""Native CI builds. One pinned source revision, real compilers on each OS."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
CONFIG = 'RelWithDebInfo'
SPEC = json.loads((ROOT / 'buildspec.json').read_text())
DEPS = json.loads((ROOT / 'ci/dependencies.json').read_text())
PARALLEL = min(os.cpu_count() or 2, 4)


def run(*args, env=None):
    print('+', ' '.join(str(x) for x in args), flush=True)
    subprocess.run([str(x) for x in args], cwd=ROOT, env=env, check=True)


def fetch(item):
    cache = ROOT / '.ci-downloads'
    cache.mkdir(exist_ok=True)
    dest = cache / item['url'].rsplit('/', 1)[-1]
    def valid():
        return dest.exists() and hashlib.sha256(dest.read_bytes()).hexdigest() == item['sha256']
    if not valid():
        dest.unlink(missing_ok=True)
        print('Downloading', item['url'], flush=True)
        request = urllib.request.Request(item['url'], headers={'User-Agent': 'SimpleCamPTZ-build'})
        with urllib.request.urlopen(request, timeout=120) as response, dest.open('wb') as target:
            shutil.copyfileobj(response, target)
    if not valid():
        dest.unlink(missing_ok=True)
        raise RuntimeError('SHA256 mismatch for ' + item['url'])
    return dest


def extract(archive, destination, flatten=False):
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive) as source:
        source.extractall(destination, filter='data')
    if flatten:
        candidates = [destination] + [p for p in destination.iterdir() if p.is_dir()]
        return next(p for p in candidates if (p / 'CMakeLists.txt').is_file())
    # OBS deps archives normally contain prefix contents directly, but allow
    # their single versioned prefix directory as well.
    if not (destination / 'lib').exists() and not (destination / 'Frameworks').exists():
        children = list(destination.iterdir())
        if len(children) == 1 and children[0].is_dir():
            return children[0]
    return destination


def build_sdk(kind):
    sdk = ROOT / '.ci-sdk'
    sources = extract(fetch(DEPS['obs']), sdk / 'source', flatten=True)
    build = sdk / 'build'
    install = sdk / 'install'
    prefixes = [install]
    options = ['-DENABLE_FRONTEND=OFF', '-DENABLE_PLUGINS=OFF', '-DENABLE_SCRIPTING=OFF',
               '-DENABLE_BROWSER=OFF', '-DCMAKE_BUILD_TYPE=' + CONFIG,
               '-DOBS_VERSION_OVERRIDE=' + DEPS['obs']['version']]
    if kind == 'macos':
        if platform.machine() != 'arm64':
            raise RuntimeError('The Apple Silicon job must run on an arm64 Mac')
        for item in DEPS['macos']:
            prefixes.append(extract(fetch(item), sdk / item['name']))
        options += ['-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0',
                    '-DCMAKE_PREFIX_PATH=' + ';'.join(map(str, prefixes)),
                    '-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY=-']
        generator = 'Xcode'
    else:
        # Only the SDK/API targets are built, not a replacement OBS executable.
        options += ['-DENABLE_WAYLAND=OFF', '-DENABLE_PULSEAUDIO=OFF']
        generator = 'Ninja'
    run('cmake', '-S', sources, '-B', build, '-G', generator, *options)
    run('cmake', '--build', build, '--config', CONFIG, '--target', 'obs-frontend-api', '--parallel', PARALLEL)
    run('cmake', '--install', build, '--config', CONFIG, '--component', 'Development', '--prefix', install)
    # Imported development targets can refer to runtime artifacts in the SDK.
    # Install only the library runtime components as well if the export needs
    # them; normal plugin packages never copy these host libraries.
    return prefixes


def test_environment(kind):
    env = os.environ.copy()
    if kind == 'windows':
        names = ('Qt6Core.dll', 'Qt6Network.dll', 'obs.dll', 'libobs.dll', 'obs-frontend-api.dll')
        dirs = set()
        for name in names:
            for path in (ROOT / '.deps').rglob(name):
                if 'Debug' not in str(path):
                    dirs.add(str(path.parent))
        env['PATH'] = os.pathsep.join(sorted(dirs)) + os.pathsep + env.get('PATH', '')
    elif kind == 'linux':
        dirs = [str(p.parent) for p in (ROOT / '.ci-sdk').rglob('libobs.so*') if p.is_file()]
        env['LD_LIBRARY_PATH'] = os.pathsep.join(sorted(set(dirs))) + os.pathsep + env.get('LD_LIBRARY_PATH', '')
    return env


def package(kind, build, output):
    output.mkdir(parents=True, exist_ok=True)
    if kind == 'windows':
        run('powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
            ROOT / 'build-installer.ps1', '-BuildDir', build, '-Configuration', CONFIG)
        setup = ROOT / 'dist' / f"SimpleCamPTZ-Setup-{SPEC['version']}-x64.exe"
        shutil.copy2(setup, output / setup.name)
        shutil.copytree(ROOT / 'stage/simplecamptz', output / 'portable/simplecamptz',
                        dirs_exist_ok=True, ignore=shutil.ignore_patterns('*.pdb'))
        instructions = 'Close OBS, then run the Setup EXE. The portable folder is an alternative manual installation.\n'
    elif kind == 'macos':
        stage = ROOT / 'stage-macos'
        run('cmake', '--install', build, '--config', CONFIG, '--prefix', stage)
        bundle = stage / 'simplecamptz.plugin'
        if not bundle.is_dir():
            raise RuntimeError('Expected staged simplecamptz.plugin bundle missing')
        run('codesign', '--force', '--deep', '--sign', '-', bundle)
        run('codesign', '--verify', '--deep', '--strict', bundle)
        run('lipo', '-verify_arch', 'arm64', bundle / 'Contents/MacOS/simplecamptz')
        # A .tar.gz keeps bundle symlinks and permissions intact through the
        # final all-platform artifact ZIP.
        with tarfile.open(output / f"SimpleCamPTZ-{SPEC['version']}-macOS-arm64.tar.gz", 'w:gz') as archive:
            archive.add(bundle, arcname=bundle.name)
        pkgroot = ROOT / 'package-macos/Library/Application Support/obs-studio/plugins'
        pkgroot.mkdir(parents=True, exist_ok=True)
        shutil.copytree(bundle, pkgroot / bundle.name, symlinks=True, dirs_exist_ok=True)
        run('pkgbuild', '--identifier', 'com.wzaggle.simplecamptz', '--version', SPEC['version'],
            '--root', ROOT / 'package-macos', '--install-location', '/',
            output / f"SimpleCamPTZ-Setup-{SPEC['version']}-macOS-arm64.pkg")
        instructions = ('Apple Silicon only; macOS 13+. Close OBS and run the PKG (all users).\n'
                        'Alternative: extract the tar.gz and copy simplecamptz.plugin into\n'
                        '~/Library/Application Support/obs-studio/plugins/ for this user.\n'
                        'Test packages are ad-hoc signed, not Apple Developer ID signed or notarized.\n'
                        'If macOS blocks the installer, use its normal Privacy & Security/Open Anyway approval.\n'
                        'Do not disable Gatekeeper globally. OBS load and camera behavior still require M1 testing.\n')
    else:
        root = ROOT / 'package-linux'
        run('cmake', '--install', build, '--config', CONFIG, '--prefix', root / 'usr')
        if not list(root.rglob('simplecamptz.so*')):
            raise RuntimeError('Expected staged Linux plugin missing')
        control = root / 'DEBIAN'
        control.mkdir(exist_ok=True)
        (control / 'control').write_text(
            f"Package: simplecamptz\nVersion: {SPEC['version']}\nArchitecture: amd64\n"
            'Maintainer: William Zaggle\nSection: video\nPriority: optional\n'
            'Depends: obs-studio (>= 32.0.0), libqt6widgets6 (>= 6.4), libqt6network6 (>= 6.4), libstdc++6\n'
            'Description: SimpleCamPTZ camera controls and preset sources for native Ubuntu OBS\n', encoding='utf-8')
        run('dpkg-deb', '--root-owner-group', '--build', root,
            output / f"SimpleCamPTZ-{SPEC['version']}-Ubuntu24.04-amd64.deb")
        instructions = ('Ubuntu 24.04 x64 with native OBS 32+, NOT Flatpak or Snap. Close OBS.\n'
                        'Install: sudo apt install ./SimpleCamPTZ-*-Ubuntu24.04-amd64.deb\n'
                        'OBS load and camera behavior still require Ubuntu testing.\n')
    (output / 'INSTALL.txt').write_text(instructions, encoding='utf-8')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--platform', choices=['windows', 'macos', 'linux'], required=True)
    args = parser.parse_args()
    kind = args.platform
    expected = {'windows': 'Windows', 'macos': 'Darwin', 'linux': 'Linux'}[kind]
    if platform.system() != expected:
        raise RuntimeError('Run each build on its native operating system')
    preset = {'windows': 'windows-x64', 'macos': 'macos-arm64', 'linux': 'linux-x64'}[kind]
    prefixes = build_sdk(kind) if kind != 'windows' else []
    extra = ['-DCMAKE_PREFIX_PATH=' + ';'.join(map(str, prefixes))] if prefixes else []
    run('cmake', '--preset', preset, *extra)
    build = ROOT / 'out/build' / ('simplecamptz-' + preset)
    targets = ['simplecamptz', 'simplecamptz_packet_tests', 'simplecamptz_profile_tests',
               'simplecamptz_runtime_state_tests', 'simplecamptz_camera_concurrency_tests']
    run('cmake', '--build', build, '--config', CONFIG, '--target', *targets, '--parallel', PARALLEL)
    run('ctest', '--test-dir', build, '-C', CONFIG, '--output-on-failure', env=test_environment(kind))
    folder = {'windows': 'Windows', 'macos': 'macOS', 'linux': 'Linux'}[kind]
    output = ROOT / 'packages' / folder
    package(kind, build, output)
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    info = {'product': 'SimpleCamPTZ', 'version': SPEC['version'], 'revision': revision,
            'platform': kind, 'architecture': platform.machine(), 'obs_sdk': DEPS['obs']['version'],
            'automated_tests': 'passed', 'hardware_runtime_test': 'not performed by CI',
            'macos_signing': 'ad-hoc; not notarized' if kind == 'macos' else None}
    (output / 'BUILD-INFO.json').write_text(json.dumps(info, indent=2) + '\n')
    lines = [hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.name
             for p in sorted(output.iterdir()) if p.is_file()]
    (output / 'SHA256SUMS.txt').write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()

import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import package
import validate_tag
import publish
import portable_openssl
build_spec = importlib.util.spec_from_file_location('release_build', ROOT / 'tools/release/build.py')
release_build = importlib.util.module_from_spec(build_spec); build_spec.loader.exec_module(release_build)
spec = importlib.util.spec_from_file_location('dependencies', ROOT / 'tools/dependencies.py')
dependencies = importlib.util.module_from_spec(spec); spec.loader.exec_module(dependencies)


class DependencyTests(unittest.TestCase):
    def test_chromium_pin_is_derived_from_the_selected_webengine_commit(self):
        calls = []
        def api(path):
            calls.append(path)
            if '/commits/' in path: return {'sha': 'e' * 40}
            return {'sha': 'c' * 40}
        def content(repo, path, ref):
            if path == '.cmake.conf': return 'set(QT_REPO_MODULE_VERSION "6.141.1")'
            self.assertEqual(repo, 'qtwebengine-chromium')
            self.assertEqual(ref, 'c' * 40)
            return 'MAJOR=141\nMINOR=0\nBUILD=1\nPATCH=2\n'
        with patch.object(dependencies, 'api', side_effect=api), patch.object(dependencies, 'content', side_effect=content):
            result = dependencies.propose({'abi': 1}, '6.12.1', 'v6.141.1')
        self.assertEqual(result['qt'], '6.12.1')
        self.assertEqual(result['chromium_version'], '141.0.1.2')
        self.assertEqual(result['chromium_commit'], 'c' * 40)
        self.assertIn('qtwebengine/contents/src/3rdparty?ref=' + 'e' * 40, calls)

    def test_automatic_updates_reject_prereleases(self):
        with patch.object(dependencies, 'api', return_value={'sha': 'e' * 40}), patch.object(dependencies, 'content', return_value='set(QT_REPO_MODULE_VERSION "6.142.0")\nset(QT_REPO_MODULE_PRERELEASE_VERSION_SEGMENT "alpha1")'):
            with self.assertRaisesRegex(RuntimeError, 'prerelease'):
                dependencies.propose({}, '6.12.1', 'dev')


class ReleaseTests(unittest.TestCase):
    def test_macos_package_refuses_newer_dependency_before_signing(self):
        import json
        with tempfile.TemporaryDirectory() as temporary:
            app = Path(temporary) / 'arbiter.app'
            resources = app / 'Contents/Resources'; resources.mkdir(parents=True)
            (resources / 'release.json').write_text(json.dumps({'minimum_os': '14.4'}))
            binary = app / 'libcrypto.3.dylib'; binary.touch()
            with patch.object(package, 'machos', return_value=[binary]), \
                    patch.object(package, 'run', side_effect=['arm64', 'cmd LC_BUILD_VERSION\nminos 26.0']) as commands:
                with self.assertRaisesRegex(RuntimeError, 'above package target'):
                    package.sign_macos(app, 'test identity', True)
                self.assertFalse(any(c.args[0] == 'codesign' for c in commands.call_args_list))

    def test_portable_openssl_rejects_modified_source_before_compiling(self):
        import json
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            pins = root / 'tools/release'; pins.mkdir(parents=True)
            (pins / 'openssl.json').write_text(json.dumps({'version': '3.5.9', 'url': 'https://example.invalid/source', 'sha256': '0' * 64}))
            archive = root / 'build-release-openssl/3.5.9-arm64-macos14.4/openssl-3.5.9.tar.gz'
            archive.parent.mkdir(parents=True); archive.write_bytes(b'modified source')
            with patch.object(portable_openssl, 'ROOT', root), \
                    patch.object(portable_openssl.platform, 'machine', return_value='arm64'), \
                    patch.object(portable_openssl, 'run') as commands:
                with self.assertRaisesRegex(RuntimeError, 'checksum mismatch'):
                    portable_openssl.ensure('14.4', 12)
                commands.assert_not_called()
            self.assertFalse(archive.exists())

    def test_macos_deployment_versions_cover_old_and_new_load_commands(self):
        lines = ['cmd LC_BUILD_VERSION', 'cmdsize 32', 'platform 1', 'minos 14.4',
                 'sdk 26.0', 'cmd LC_VERSION_MIN_MACOSX', 'cmdsize 16',
                 'version 13.0', 'sdk 14.0']
        self.assertEqual(package.deployment_versions(lines), [(14, 4, 0), (13, 0, 0)])
        with patch.object(package, 'run', return_value='\n'.join(lines)):
            self.assertEqual(package.macos_minimum(Path('/qt')), '14.4')
        with patch.object(package, 'run', return_value='cmd LC_BUILD_VERSION\nminos 26.0'):
            self.assertEqual(package.macos_minimum(Path('/qt')), '26.0')

    def test_linux_keyring_tests_never_reuse_persistent_data(self):
        seen = []
        original = {'ARBITER_TEST_KEYCHAIN': '1', 'XDG_DATA_HOME': '/persistent/user/data'}
        def check(*args, env):
            data = Path(env['XDG_DATA_HOME']); control = Path(env['GNOME_KEYRING_CONTROL'])
            self.assertNotEqual(str(data), original['XDG_DATA_HOME'])
            self.assertTrue(data.is_dir() and control.is_dir())
            self.assertEqual(data.stat().st_mode & 0o777, 0o700)
            self.assertIn('dbus-run-session', args)
            seen.append(data)
        with patch.object(release_build, 'run', side_effect=check):
            release_build.run_linux_tests(Path('/tmp/tests'), original)
            release_build.run_linux_tests(Path('/tmp/tests'), original)
        self.assertNotEqual(seen[0], seen[1])
        self.assertTrue(all(not path.exists() for path in seen))
        self.assertEqual(original['XDG_DATA_HOME'], '/persistent/user/data')

    def test_docker_sandbox_profile_keeps_default_deny(self):
        import json
        profile = json.loads((ROOT / 'tools/release/docker/seccomp-userns.json').read_text())
        self.assertEqual(profile['defaultAction'], 'SCMP_ACT_ERRNO')
        unconditional = {name for rule in profile['syscalls']
                         if rule['action'] == 'SCMP_ACT_ALLOW' and not rule.get('args')
                         and not rule.get('includes') and not rule.get('excludes')
                         for name in rule['names']}
        self.assertTrue({'clone', 'clone3', 'unshare', 'setns'} <= unconditional)
        self.assertFalse({'mount', 'reboot', 'kexec_load', 'bpf'} & unconditional)

    def test_linux_never_copies_a_different_stock_engine_version(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); qt = root / 'qt'; sdk = root / 'sdk'; stage = root / 'stage'
            (stage / 'bin').mkdir(parents=True)
            for prefix in (qt, sdk):
                (prefix / 'lib').mkdir(parents=True)
                (prefix / 'qml/QtWebEngine').mkdir(parents=True)
                (prefix / 'qml/QtWebEngine/qmldir').write_text(prefix.name)
            (qt / 'lib/libQt6Core.so.6.12.0').write_text('base Qt')
            (qt / 'lib/libicuuc.so.73.2').write_text('ICU runtime')
            (qt / 'lib/libicuuc.so.73').symlink_to('libicuuc.so.73.2')
            (qt / 'lib/libicutest.so.73.2').touch()
            (qt / 'lib/libQt6WebEngineCore.so.6.139.0').write_text('stock')
            (qt / 'qml/QtWebEngine/obsolete.qml').touch()
            (sdk / 'lib/libQt6WebEngineCore.so.6.140.0').write_text('patched')
            (sdk / 'libexec').mkdir(); (sdk / 'libexec/QtWebEngineProcess').touch()
            package.linux(stage, sdk, qt)
            self.assertTrue((stage / 'lib/libQt6Core.so.6.12.0').exists())
            self.assertTrue((stage / 'lib/libicuuc.so.73').is_symlink())
            self.assertEqual((stage / 'lib/libicuuc.so.73').read_text(), 'ICU runtime')
            self.assertFalse((stage / 'lib/libicutest.so.73.2').exists())
            self.assertTrue((stage / 'lib/libQt6WebEngineCore.so.6.140.0').exists())
            self.assertFalse((stage / 'lib/libQt6WebEngineCore.so.6.139.0').exists())
            self.assertFalse((stage / 'qml/QtWebEngine/obsolete.qml').exists())
            self.assertEqual((stage / 'qml/QtWebEngine/qmldir').read_text(), 'sdk')

    def test_linux_packages_runtime_elfs_without_development_objects(self):
        def header(kind):
            return b'\x7fELF\x02\x01' + bytes(10) + kind.to_bytes(2, 'little')
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); qt = root / 'qt'; sdk = root / 'sdk'; stage = root / 'stage'
            module = qt / 'qml/Example'; module.mkdir(parents=True)
            (module / 'view.qml').write_text('import QtQuick\nItem {}')
            (module / 'plugin.so').write_bytes(header(3))
            (module / 'resource.o').write_bytes(header(1))
            (module / 'resource.elf').write_bytes(header(1))
            (module / 'static.a').write_bytes(b'!<arch>\n')
            (module / 'objects-RelWithDebInfo').mkdir()
            (module / 'objects-RelWithDebInfo/resource.o').write_bytes(header(1))
            (stage / 'bin').mkdir(parents=True)
            executable = stage / 'bin/arbiter'; executable.write_bytes(header(2))
            with patch.object(package, 'run') as commands:
                package.linux(stage, sdk, qt)
            copied = stage / 'qml/Example'
            self.assertTrue((copied / 'view.qml').exists())
            self.assertEqual({call.args[-1] for call in commands.call_args_list},
                             {executable, copied / 'plugin.so'})
            self.assertFalse(any(p.suffix in ('.o', '.a') for p in stage.rglob('*')))
            self.assertFalse((copied / 'resource.elf').exists())
            self.assertFalse((copied / 'objects-RelWithDebInfo').exists())

    def test_uploader_rejects_mixed_local_builds_before_updating_release(self):
        import hashlib
        import json
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / 'arbiter-0.1.0-macos-arm64-unsigned.zip'
            with zipfile.ZipFile(archive, 'w') as bundle:
                bundle.writestr('arbiter.app/Contents/Resources/release.json',
                                json.dumps({'version': '0.1.0', 'commit': 'b' * 40}))
            digest = hashlib.sha256(archive.read_bytes()).hexdigest()
            (root / (archive.name + '.sha256')).write_text(digest + '  ' + archive.name)
            def command(*args, **kwargs):
                if args[:3] == ('git', 'show', 'a' * 40 + ':VERSION.txt'): return '0.1.0\n'
                if args[:3] == ('gh', 'repo', 'view'): return 'owner/repo\n'
                return 'a' * 40 + '\n'
            with patch.object(publish, 'ROOT', root), patch.object(publish, 'run', side_effect=command) as commands, \
                    patch.object(sys, 'argv', ['publish.py', '--tag', 'v0.1.0', '--assets', str(root), '--unsigned']), \
                    patch.dict(os.environ, {}, clear=True):
                with self.assertRaisesRegex(RuntimeError, 'different commit'):
                    publish.main()
                self.assertFalse(any(call.args[:2] == ('gh', 'release') for call in commands.call_args_list))

    def test_partial_upload_preserves_draft_and_uploads_only_selected_platform(self):
        import hashlib
        import json
        from identity import fingerprint
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            archive = root / 'arbiter-0.1.0-windows-x86_64.zip'
            metadata = {'version': '0.1.0', 'commit': 'a' * 40, 'platform': 'windows',
                        'architecture': 'x86_64', 'signed': True,
                        'dependencies': json.loads((ROOT / 'dependencies.json').read_text()),
                        'engine': {'inputs_sha256': fingerprint()}}
            with zipfile.ZipFile(archive, 'w') as bundle:
                bundle.writestr('arbiter/release.json', json.dumps(metadata))
            sidecar = root / (archive.name + '.sha256')
            sidecar.write_text(hashlib.sha256(archive.read_bytes()).hexdigest() + '  ' + archive.name)
            def command(*args, **kwargs):
                if args[:2] == ('git', 'archive'):
                    (root / 'arbiter-0.1.0-source.tar.gz').write_bytes(b'source')
                if args[:2] == ('git', 'show'): return '0.1.0'
                if args[:3] == ('gh', 'repo', 'view'): return 'owner/repo'
                return 'a' * 40
            with patch.object(publish, 'run', side_effect=command) as commands, \
                    patch.object(publish.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, '{"isDraft":true}')), \
                    patch.object(sys, 'argv', ['publish.py', '--tag', 'v0.1.0', '--assets', str(root), '--platform', 'windows']), \
                    patch.dict(os.environ, {}, clear=True):
                publish.main()
            uploads = [c.args for c in commands.call_args_list if c.args[:3] == ('gh', 'release', 'upload')]
            self.assertEqual(len(uploads), 1)
            self.assertIn(archive, uploads[0])
            self.assertIn(sidecar, uploads[0])
            self.assertIn(root / 'SHA256SUMS-windows', uploads[0])
            self.assertFalse(any(c.args[:3] == ('gh', 'release', 'edit') for c in commands.call_args_list))

    def test_macos_deployment_replaces_stock_engine_as_a_unit(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); sdk = root / 'sdk'; app = root / 'Arbiter.app'
            for module in ('QtWebEngineCore', 'QtWebEngineQuick'):
                relative = Path('lib') / (module + '.framework')
                source = sdk / relative / 'Versions/A'
                source.mkdir(parents=True); (source / module).write_text('patched')
                (source.parent / 'Current').symlink_to('A', target_is_directory=True)
                (source.parent / 'Resources').mkdir()
                destination = app / 'Contents/Frameworks' / relative.name / 'Versions/A'
                destination.mkdir(parents=True); (destination / module).write_text('stock')
            helper = sdk / 'lib/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess'
            helper.parent.mkdir(); helper.write_text('patched helper')
            module = sdk / 'qml/QtWebEngine'; module.mkdir(parents=True)
            (module / 'qmldir').write_text('patched import')
            sql = app / 'Contents/PlugIns/sqldrivers'; sql.mkdir(parents=True)
            (sql / 'libqsqlite.dylib').touch(); (sql / 'libqsqlodbc.dylib').touch()
            package.macos_engine(app, sdk)
            framework = app / 'Contents/Frameworks/QtWebEngineCore.framework'
            self.assertEqual((framework / 'Versions/Current/QtWebEngineCore').read_text(), 'patched')
            self.assertTrue((framework / 'Versions/Current').is_symlink())
            self.assertFalse((framework / 'Versions/Resources').exists())
            self.assertEqual((framework / 'Versions/A/Helpers/QtWebEngineProcess').read_text(), 'patched helper')
            self.assertEqual((app / 'Contents/Resources/qml/QtWebEngine/qmldir').read_text(), 'patched import')
            self.assertEqual([p.name for p in sql.iterdir()], ['libqsqlite.dylib'])

    def test_codec_and_widevine_packaging_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name in ('QtWebEngineProcess', 'qtwebengine_resources.pak', 'en-US.pak'):
                (root / name).touch()
            package.audit(root)
            for name in ('libavcodec.so.61', 'widevinecdm.dll', 'libopenh264.so'):
                path = root / name; path.touch()
                with self.assertRaisesRegex(RuntimeError, 'Forbidden'): package.audit(root)
                path.unlink()
            (root / 'QtWebEngineProcess').unlink()
            with self.assertRaisesRegex(RuntimeError, 'missing'): package.audit(root)

    def test_release_tags_require_version_and_default_branch_ancestry(self):
        previous = Path.cwd()
        with tempfile.TemporaryDirectory() as temporary:
            os.chdir(temporary)
            try:
                def git(*args): subprocess.run(['git', *args], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                git('init'); git('config', 'user.name', 'Tests'); git('config', 'user.email', 'tests@example.invalid')
                Path('VERSION.txt').write_text('0.1.0-rc.1\n')
                git('add', 'VERSION.txt'); git('commit', '-m', 'release'); git('tag', 'v0.1.0-rc.1')
                git('update-ref', 'refs/remotes/origin/master', 'HEAD')
                self.assertEqual(validate_tag.validate('v0.1.0-rc.1', 'master')['prerelease'], 'true')
                git('tag', 'v0.2.0')
                with self.assertRaises(ValueError): validate_tag.validate('v0.2.0', 'master')
                for tag in ('--help', 'refs/heads/master', 'v1.0.0;touch unexpected'):
                    with self.assertRaises(ValueError): validate_tag.validate(tag, 'master')
                Path('VERSION.txt').write_text('0.2.0\n'); git('add', 'VERSION.txt'); git('commit', '-m', 'unmerged'); git('tag', 'v0.2.1')
                with self.assertRaises(subprocess.CalledProcessError): validate_tag.validate('v0.2.1', 'master')
            finally: os.chdir(previous)


if __name__ == '__main__': unittest.main()

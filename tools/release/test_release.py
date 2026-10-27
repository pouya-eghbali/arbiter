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
            (qt / 'lib/libQt6WebEngineCore.so.6.139.0').write_text('stock')
            (qt / 'qml/QtWebEngine/obsolete.qml').touch()
            (sdk / 'lib/libQt6WebEngineCore.so.6.140.0').write_text('patched')
            (sdk / 'libexec').mkdir(); (sdk / 'libexec/QtWebEngineProcess').touch()
            package.linux(stage, sdk, qt)
            self.assertTrue((stage / 'lib/libQt6Core.so.6.12.0').exists())
            self.assertTrue((stage / 'lib/libQt6WebEngineCore.so.6.140.0').exists())
            self.assertFalse((stage / 'lib/libQt6WebEngineCore.so.6.139.0').exists())
            self.assertFalse((stage / 'qml/QtWebEngine/obsolete.qml').exists())
            self.assertEqual((stage / 'qml/QtWebEngine/qmldir').read_text(), 'sdk')

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

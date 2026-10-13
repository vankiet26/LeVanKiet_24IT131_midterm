#!/usr/bin/env python3
"""Behavior checks against the supplied subset, not GNU ls extensions."""
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import tempfile
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve()) if len(sys.argv) > 1 else str(Path('myls').resolve())


class MyLsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        for name, content in [('alpha', b'a'), ('beta', b'b' * 30), ('.hidden', b'h')]:
            (self.root / name).write_bytes(content)
        (self.root / 'sub').mkdir()
        (self.root / 'sub' / 'inside').write_text('nested')
        os.utime(self.root / 'alpha', (1000000000, 1000000000))
        os.utime(self.root / 'beta', (1100000000, 1100000000))

    def tearDown(self):
        self.temp.cleanup()

    def run_ls(self, *args, env=None):
        variables = dict(os.environ, LC_ALL='C', TZ='UTC')
        if env:
            variables.update(env)
        return subprocess.run([BINARY, *map(str, args)], cwd=self.root, env=variables,
                              capture_output=True, timeout=5)

    def text(self, *args):
        result = self.run_ls(*args)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout.decode()

    def test_default_and_reverse(self):
        expected = ['alpha', 'beta', 'sub']
        if os.geteuid() == 0:
            expected.insert(0, '.hidden')
        self.assertEqual(self.text().splitlines(), expected)
        self.assertEqual(self.text('-r').splitlines(), expected[::-1])

    def test_hidden_options(self):
        self.assertEqual(self.text('-A').splitlines(), ['.hidden', 'alpha', 'beta', 'sub'])
        self.assertEqual(self.text('-a').splitlines(), ['.', '..', '.hidden', 'alpha', 'beta', 'sub'])

    def test_size_time_reverse(self):
        self.assertEqual(self.text('-S', 'alpha', 'beta').splitlines(), ['beta', 'alpha'])
        self.assertEqual(self.text('-Sr', 'alpha', 'beta').splitlines(), ['alpha', 'beta'])
        self.assertEqual(self.text('-t', 'alpha', 'beta').splitlines(), ['beta', 'alpha'])
        self.assertEqual(self.text('-tr', 'alpha', 'beta').splitlines(), ['alpha', 'beta'])
        # Last sort selector wins. -f preserves operand order within each group.
        self.assertEqual(self.text('-tf', 'beta', 'alpha').splitlines(), ['beta', 'alpha'])

    def test_access_and_change_time(self):
        os.utime(self.root / 'alpha', (1200000000, 1000000000))
        self.assertEqual(self.text('-tu', 'alpha', 'beta').splitlines(), ['alpha', 'beta'])
        a = self.root / 'alpha'
        b = self.root / 'beta'
        os.chmod(a, 0o640)
        values = {p.name: (p.stat().st_ctime_ns, p.name) for p in (a, b)}
        expected = sorted(values, key=lambda n: (-values[n][0], n))
        self.assertEqual(self.text('-tc', 'alpha', 'beta').splitlines(), expected)

    def test_last_option_wins(self):
        self.assertIn('sub:', self.text('-dR', 'sub'))
        self.assertEqual(self.text('-Rd', 'sub'), 'sub\n')
        uid = str(os.stat(self.root / 'alpha').st_uid)
        self.assertEqual(self.text('-ln', 'alpha').split()[2], uid)
        self.assertEqual(self.text('-nl', 'alpha').split()[2],
                         __import__('pwd').getpwuid(os.getuid()).pw_name)

    def test_long_permissions_and_devices(self):
        os.chmod(self.root / 'alpha', 0o4755)
        self.assertTrue(self.text('-l', 'alpha').startswith('-rwsr-xr-x'))
        os.chmod(self.root / 'alpha', 0o4644)
        self.assertTrue(self.text('-l', 'alpha').startswith('-rwSr--r--'))
        os.chmod(self.root / 'sub', 0o1777)
        self.assertTrue(self.text('-ld', 'sub').startswith('drwxrwxrwt'))
        self.assertTrue(self.text('-l', '/dev/null').startswith('c'))
        self.assertRegex(self.text('-l', '/dev/null'), r'\d+, \d+')

    def test_classification_and_symlinks(self):
        os.chmod(self.root / 'alpha', 0o755)
        (self.root / 'link').symlink_to('alpha')
        (self.root / 'broken').symlink_to('missing')
        os.mkfifo(self.root / 'pipe')
        sock = None
        try:
            try:
                sock = socket.socket(socket.AF_UNIX)
                sock.bind(str(self.root / 'sock'))
            except PermissionError:
                print('NOTE: sandbox disallows Unix socket creation; socket suffix check omitted.')
            output = self.text('-F')
            names = ['alpha*', 'sub/', 'link@', 'broken@', 'pipe|']
            if (self.root / 'sock').exists():
                names.append('sock=')
            for name in names:
                self.assertIn(name + '\n', output)
        finally:
            if sock:
                sock.close()
        self.assertIn('link -> alpha', self.text('-l', 'link'))
        self.assertIn('broken -> missing', self.text('-l', 'broken'))
        (self.root / 'dirlink').symlink_to('sub', target_is_directory=True)
        self.assertEqual(self.text('dirlink'), 'inside\n')
        self.assertEqual(self.text('-d', 'dirlink'), 'dirlink\n')

    def test_recursive_no_symlink_loop(self):
        (self.root / 'sub' / 'back').symlink_to('..', target_is_directory=True)
        output = self.text('-Ra')
        self.assertIn('./sub:', output)
        self.assertIn('inside', output)
        self.assertNotIn('./sub/back:', output)

    def test_numeric_inode_blocks_environment(self):
        info = (self.root / 'alpha').stat()
        self.assertEqual(int(self.text('-i', 'alpha').split()[0]), info.st_ino)
        allocated = info.st_blocks * 512
        self.assertEqual(int(self.text('-s', 'alpha').split()[0]), info.st_blocks)
        result = self.run_ls('-s', 'alpha', env={'BLOCKSIZE': '1K'})
        self.assertEqual(int(result.stdout.split()[0]), (allocated + 1023) // 1024)
        self.assertEqual(int(self.text('-shk', 'alpha').split()[0]), (allocated + 1023) // 1024)
        self.assertRegex(self.text('-skh', 'alpha').split()[0], r'[BKMG]$')
        self.assertEqual(self.text('-lh', 'alpha').split()[4], '1B')
        # -k changes block counts, not byte sizes in the long format.
        self.assertEqual(self.text('-lk', 'alpha').split()[4], '1')
        (self.root / 'sparse').touch()
        os.truncate(self.root / 'sparse', 1024 * 1024)
        self.assertEqual(self.text('-lh', 'sparse').split()[4], '1.0M')
        self.assertEqual(int(self.text('-s', 'sparse').split()[0]), 0)

    def test_nonprintable_and_utf8(self):
        name = 'bad\tname'
        (self.root / name).touch()
        self.assertEqual(self.text('-q', name), 'bad?name\n')
        self.assertEqual(self.text('-qw', name), name + '\n')
        self.assertEqual(self.text('-wq', name), 'bad?name\n')
        (self.root / 'tiếng_Việt').touch()
        result = self.run_ls('-q', 'tiếng_Việt', env={'LC_ALL': 'C.UTF-8'})
        self.assertEqual(result.stdout.decode(), 'tiếng_Việt\n')

    def test_empty_multiple_and_errors(self):
        (self.root / 'empty').mkdir()
        self.assertEqual(self.text('empty'), '')
        self.assertEqual(self.text('-l', 'empty'), 'total 0\n')
        self.assertTrue(self.text('sub', 'beta', 'alpha').startswith('alpha\nbeta\n\nsub:\n'))
        for args in [('-z',), ('not_found',), ('',)]:
            result = self.run_ls(*args)
            self.assertGreater(result.returncode, 0)
            self.assertIn(b'myls:', result.stderr)
        result = self.run_ls('not_found', 'alpha')
        self.assertGreater(result.returncode, 0)
        self.assertEqual(result.stdout, b'alpha\n')

    def test_timezone(self):
        utc = self.run_ls('-l', 'alpha', env={'TZ': 'UTC'}).stdout
        east = self.run_ls('-l', 'alpha', env={'TZ': 'UTC-24'}).stdout
        self.assertNotEqual(utc, east)


if __name__ == '__main__':
    unittest.main(verbosity=2)

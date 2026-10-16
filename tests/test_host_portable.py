"""Native Linux production-host checks using SDL dummy drivers."""
import json
import unittest
from portable_host_support import ROOT, compile_harness, run_harness, source_hashes


class PortableHostTests(unittest.TestCase):
    def test_input_adapter_and_audio_pause_resume(self):
        before = source_hashes()
        binary = compile_harness('host_input_harness')
        result = run_harness(binary)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report['status'], 'passed')
        self.assertEqual(report['host_input_scenarios'], 8)
        self.assertFalse(report['os_input_claim'])
        self.assertEqual(source_hashes(), before, 'Inputs changed during verification')

    def test_full_first_level_route(self):
        before = source_hashes()
        binary = compile_harness('host_route_harness')
        state_path = ROOT / 'build/portable-tests/host-route-state.json'
        result = run_harness(binary, state_path)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        report = json.loads(result.stdout)
        self.assertTrue(report['success'])
        self.assertTrue(report['victory'])
        self.assertEqual(report['frames'], 2466)
        self.assertEqual(report['next_level'], 2)
        self.assertEqual(report['health'], 2)
        self.assertEqual(report['sword'], 1)
        self.assertEqual(report['guard_health'], 0)
        self.assertGreater(report['input_events'], 0)
        self.assertGreater(report['converted_samples'], 0)
        self.assertFalse(report['os_input_claim'])
        self.assertEqual(source_hashes(), before, 'Inputs changed during verification')
        (ROOT / 'build/portable-tests/host-route-result.json').write_text(
            json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    unittest.main(verbosity=2)


import test from 'node:test';
import assert from 'node:assert/strict';
import { buildJogCommands } from '../src/lib/machine-commands.js';

test('builds jog as one G-code command per line and restores absolute mode', () => {
  assert.deepEqual(buildJogCommands(-1, 0, 10, 2500), [
    'G21',
    'G91',
    'G0 X-10 Y0 F2500',
    'G90'
  ]);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const { getCompileBoardProfile } = require('../firmware-targets.cjs');

test('maps only firmware targets with matching board profiles', () => {
  assert.equal(getCompileBoardProfile('mega'), 'BOARD_MEGA_RAMPS14');
  assert.equal(getCompileBoardProfile('mega-standalone'), 'BOARD_MEGA_STANDALONE');
  assert.equal(getCompileBoardProfile('esp32'), 'BOARD_ESP32_STANDALONE');
});

test('does not compile unsupported boards with a fallback Mega profile', () => {
  for (const board of ['nano-new', 'nano-old', 'uno', 'esp32s3', 'stm32', 'pico', 'missing']) {
    assert.equal(getCompileBoardProfile(board), null);
  }
});

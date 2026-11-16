import test from 'node:test';
import assert from 'node:assert/strict';
import { generateGcode } from '../src/lib/gcode-generator.js';

test('refuses dual-head jobs until firmware can switch tools', () => {
  assert.throws(
    () => generateGcode([[{ x: 0, y: 0 }, { x: 10, y: 10 }]], { heads: 2 }),
    /Dual-head operation is not supported by the current firmware/
  );
});

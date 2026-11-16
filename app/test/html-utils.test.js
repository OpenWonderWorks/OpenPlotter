import test from 'node:test';
import assert from 'node:assert/strict';
import { escapeHtml } from '../src/lib/html-utils.js';

test('escapes untrusted text before it is inserted into toast markup', () => {
  assert.equal(
    escapeHtml('<img src=x onerror="alert(1)"> &'),
    '&lt;img src=x onerror=&quot;alert(1)&quot;&gt; &amp;'
  );
});

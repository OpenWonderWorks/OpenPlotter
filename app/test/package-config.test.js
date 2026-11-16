import test from 'node:test';
import assert from 'node:assert/strict';
import { existsSync, readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const appDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const packageJson = JSON.parse(readFileSync(path.join(appDir, 'package.json'), 'utf8'));

test('clean-checkout Electron startup builds the renderer first', () => {
  for (const scriptName of ['electron:start', 'electron:dev']) {
    const script = packageJson.scripts[scriptName];
    assert.match(script, /npm run build/);
    assert.ok(script.includes('electron .'));
  }
});

test('desktop packaging includes runtime modules and existing firmware resources', () => {
  const packagedFiles = packageJson.build.files;
  for (const file of ['electron-main.cjs', 'electron-preload.cjs', 'firmware-targets.cjs']) {
    assert.ok(packagedFiles.includes(file), `${file} must be in the packaged app`);
  }

  for (const resource of packageJson.build.extraResources) {
    assert.ok(
      existsSync(path.resolve(appDir, resource.from)),
      `extraResources source does not exist: ${resource.from}`
    );
  }
});

test('desktop package formats are configured for all release platforms', () => {
  assert.equal(packageJson.build.win.target, 'portable');
  assert.equal(packageJson.build.mac.target, 'dmg');
  assert.equal(packageJson.build.linux.target, 'AppImage');
  assert.match(packageJson.scripts['electron:build'], /--publish never/);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import { canSendManualCommand, invalidateGcode, isCutReady } from '../src/lib/gcode-state.js';

test('invalidating an edited design prevents a stale job from starting', () => {
  const state = { connected: true, gcode: 'G1 X10 Y10' };
  assert.equal(isCutReady(state), true);

  invalidateGcode(state);

  assert.equal(state.gcode, '');
  assert.equal(isCutReady(state), false);
});

test('a generated job cannot start while disconnected', () => {
  assert.equal(isCutReady({ connected: false, gcode: 'G1 X10 Y10' }), false);
});

test('a generated job cannot start while a job or jog queue is draining', () => {
  const state = {
    connected: true,
    gcode: 'G1 X10 Y10',
    connection: { sending: false, waitingForOk: false, awaitingIdle: true }
  };

  assert.equal(isCutReady(state), false);

  state.connection.awaitingIdle = false;
  state.connection.sending = true;
  assert.equal(isCutReady(state), false);

  state.connection.sending = false;
  assert.equal(isCutReady(state), true);
});

test('manual commands are only allowed while connected and the command queue is idle', () => {
  const state = {
    connected: true,
    connection: { sending: false, waitingForOk: false, awaitingIdle: false }
  };
  assert.equal(canSendManualCommand(state), true);

  for (const queueState of ['sending', 'waitingForOk', 'awaitingIdle']) {
    state.connection.sending = false;
    state.connection.waitingForOk = false;
    state.connection.awaitingIdle = false;
    state.connection[queueState] = true;
    assert.equal(canSendManualCommand(state), false, `blocked while ${queueState}`);
  }

  state.connected = false;
  state.connection.awaitingIdle = false;
  assert.equal(canSendManualCommand(state), false);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import { WebSocketManager } from '../src/lib/websocket.js';
import { SerialManager } from '../src/lib/serial.js';

test('normalizes JSON status messages from the ESP32 into machine coordinates', () => {
  const manager = new WebSocketManager();
  let received;
  manager.onStatus = status => { received = status; };

  manager.handleMessage('{"state":"Run","pos":{"x":12.5,"y":-3,"z":0},"feed":900,"tool":true}');

  assert.deepEqual(received, {
    state: 'Run',
    mPos: { x: 12.5, y: -3, z: 0 },
    buffer: 0,
    feed: 900,
    tool: true
  });
});

test('keeps job progress below 100 until the final command is acknowledged', () => {
  const previousWebSocket = globalThis.WebSocket;
  globalThis.WebSocket = { OPEN: 1 };

  try {
    const manager = new WebSocketManager();
    const sent = [];
    const progress = [];
    manager.connected = true;
    manager.ws = { readyState: 1, send: line => sent.push(line) };
    manager.onProgress = percent => progress.push(percent);

    manager.startSending(['G1 X10 F500', 'M5']);
    manager.handleMessage('ok');

    assert.deepEqual(sent, ['G1 X10 F500\n', 'M5\n']);
    assert.equal(progress.at(-1), 99);

    manager.handleMessage('ok');
    assert.equal(progress.at(-1), 99);
    manager.handleMessage('{"state":"Idle","pos":{"x":10,"y":0,"z":0}}');
    assert.equal(progress.at(-1), 100);
  } finally {
    if (previousWebSocket === undefined) delete globalThis.WebSocket;
    else globalThis.WebSocket = previousWebSocket;
  }
});

test('serial job progress also waits for the final command acknowledgement', () => {
  const manager = new SerialManager();
  const sent = [];
  const progress = [];
  manager.connected = true;
  manager.writer = { write: data => sent.push(new TextDecoder().decode(data)) };
  manager.onProgress = percent => progress.push(percent);

  manager.startSending(['G1 X10 F500', 'M5']);
  manager.handleLine('ok');

  assert.deepEqual(sent, ['G1 X10 F500\n', 'M5\n']);
  assert.equal(progress.at(-1), 99);

  manager.handleLine('ok');
  assert.equal(progress.at(-1), 99);
  manager.handleLine('<Idle|MPos:10,0,0|Bf:0>');
  assert.equal(progress.at(-1), 100);
});

test('WebSocket machine errors abort the remaining job without completing progress', () => {
  const previousWebSocket = globalThis.WebSocket;
  globalThis.WebSocket = { OPEN: 1 };

  try {
    const manager = new WebSocketManager();
    const sent = [];
    const progress = [];
    const aborted = [];
    manager.connected = true;
    manager.ws = { readyState: 1, send: line => sent.push(line) };
    manager.onProgress = percent => progress.push(percent);
    manager.onJobAborted = error => aborted.push(error);

    manager.startSending(['G1 X10 F500', 'M5']);
    manager.handleMessage('error:20 (Unsupported command)');
    manager.handleMessage('ok');
    manager.handleMessage('{"state":"Idle","pos":{"x":0,"y":0,"z":0}}');

    assert.deepEqual(sent, ['G1 X10 F500\n', '\x18']);
    assert.ok(progress.every(percent => percent < 100));
    assert.equal(aborted.length, 1);
    assert.equal(manager.sending, false);
    assert.deepEqual(manager.queue, []);
  } finally {
    if (previousWebSocket === undefined) delete globalThis.WebSocket;
    else globalThis.WebSocket = previousWebSocket;
  }
});

test('Serial machine errors abort the remaining job without completing progress', () => {
  const manager = new SerialManager();
  const sent = [];
  const progress = [];
  const aborted = [];
  manager.connected = true;
  manager.writer = { write: data => sent.push(new TextDecoder().decode(data)) };
  manager.onProgress = percent => progress.push(percent);
  manager.onJobAborted = error => aborted.push(error);

  manager.startSending(['G1 X10 F500', 'M5']);
  manager.handleLine('error:20 (Unsupported command)');
  manager.handleLine('ok');
  manager.handleLine('<Idle|MPos:0,0,0|Bf:0>');

  assert.deepEqual(sent, ['G1 X10 F500\n', '\x18']);
  assert.ok(progress.every(percent => percent < 100));
  assert.equal(aborted.length, 1);
  assert.equal(manager.sending, false);
  assert.deepEqual(manager.queue, []);
});

test('WebSocket jog sequences send one command per acknowledgement', () => {
  const previousWebSocket = globalThis.WebSocket;
  globalThis.WebSocket = { OPEN: 1 };

  try {
    const manager = new WebSocketManager();
    const sent = [];
    const progress = [];
    manager.connected = true;
    manager.ws = { readyState: 1, send: line => sent.push(line) };
    manager.onProgress = percent => progress.push(percent);

    assert.equal(manager.sendCommandSequence(['G21', 'G91', 'G0 X-10 Y0 F2500', 'G90'], { abortWithReset: true }), true);
    assert.deepEqual(sent, ['G21\n']);
    manager.handleMessage('ok');
    assert.deepEqual(sent, ['G21\n', 'G91\n']);
    manager.handleMessage('ok');
    assert.deepEqual(sent, ['G21\n', 'G91\n', 'G0 X-10 Y0 F2500\n']);
    manager.handleMessage('ok');
    assert.deepEqual(sent, ['G21\n', 'G91\n', 'G0 X-10 Y0 F2500\n', 'G90\n']);
    manager.handleMessage('ok');

    assert.equal(manager.sending, false);
    assert.equal(manager.awaitingIdle, true);
    assert.equal(manager.startSending(['M5']), false);
    manager.handleMessage('{"state":"Idle","pos":{"x":-10,"y":0,"z":0}}');
    assert.equal(manager.awaitingIdle, false);
    assert.deepEqual(progress, []);
    assert.equal(manager.startSending(['M5']), true);
    manager.stopSending();
  } finally {
    if (previousWebSocket === undefined) delete globalThis.WebSocket;
    else globalThis.WebSocket = previousWebSocket;
  }
});

test('Serial jog sequences send one command per acknowledgement', () => {
  const manager = new SerialManager();
  const sent = [];
  const progress = [];
  manager.connected = true;
  manager.writer = { write: data => sent.push(new TextDecoder().decode(data)) };
  manager.onProgress = percent => progress.push(percent);

  assert.equal(manager.sendCommandSequence(['G21', 'G91', 'G0 X-10 Y0 F2500', 'G90'], { abortWithReset: true }), true);
  assert.deepEqual(sent, ['G21\n']);
  manager.handleLine('ok');
  assert.deepEqual(sent, ['G21\n', 'G91\n']);
  manager.handleLine('ok');
  assert.deepEqual(sent, ['G21\n', 'G91\n', 'G0 X-10 Y0 F2500\n']);
  manager.handleLine('ok');
  assert.deepEqual(sent, ['G21\n', 'G91\n', 'G0 X-10 Y0 F2500\n', 'G90\n']);
  manager.handleLine('ok');

  assert.equal(manager.sending, false);
  assert.equal(manager.awaitingIdle, true);
  assert.equal(manager.startSending(['M5']), false);
  manager.handleLine('<Idle|MPos:-10,0,0|Bf:0>');
  assert.equal(manager.awaitingIdle, false);
  assert.deepEqual(progress, []);
  assert.equal(manager.startSending(['M5']), true);
  manager.stopSending();
});

test('WebSocket jog command errors clear the sequence before a later acknowledgement', () => {
  const previousWebSocket = globalThis.WebSocket;
  globalThis.WebSocket = { OPEN: 1 };

  try {
    const manager = new WebSocketManager();
    const sent = [];
    const progress = [];
    manager.connected = true;
    manager.ws = { readyState: 1, send: line => sent.push(line) };
    manager.onProgress = percent => progress.push(percent);

    manager.sendCommandSequence(['G21', 'G91', 'G0 X-10 Y0 F2500', 'G90'], { abortWithReset: true });
    manager.handleMessage('ok');
    manager.handleMessage('error:20 (Unsupported command)');
    manager.handleMessage('ok');
    manager.handleMessage('{"state":"Idle","pos":{"x":0,"y":0,"z":0}}');

    assert.deepEqual(sent, ['G21\n', 'G91\n', '\x18']);
    assert.equal(manager.sending, false);
    assert.deepEqual(manager.queue, []);
    assert.ok(progress.every(percent => percent < 100));
  } finally {
    if (previousWebSocket === undefined) delete globalThis.WebSocket;
    else globalThis.WebSocket = previousWebSocket;
  }
});

test('Serial jog command errors clear the sequence before a later acknowledgement', () => {
  const manager = new SerialManager();
  const sent = [];
  const progress = [];
  manager.connected = true;
  manager.writer = { write: data => sent.push(new TextDecoder().decode(data)) };
  manager.onProgress = percent => progress.push(percent);

  manager.sendCommandSequence(['G21', 'G91', 'G0 X-10 Y0 F2500', 'G90'], { abortWithReset: true });
  manager.handleLine('ok');
  manager.handleLine('error:20 (Unsupported command)');
  manager.handleLine('ok');
  manager.handleLine('<Idle|MPos:0,0,0|Bf:0>');

  assert.deepEqual(sent, ['G21\n', 'G91\n', '\x18']);
  assert.equal(manager.sending, false);
  assert.deepEqual(manager.queue, []);
  assert.ok(progress.every(percent => percent < 100));
});

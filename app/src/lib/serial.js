/**
 * OpenPlotter — Web Serial Manager
 * Manages USB Serial connectivity and handles queued G-code sending with flow control.
 */

export class SerialManager {
  constructor() {
    this.port = null;
    this.reader = null;
    this.writer = null;
    this.connected = false;
    this.baudRate = 115200;
    
    // Callbacks
    this.onStatus = () => {};
    this.onConnect = () => {};
    this.onDisconnect = () => {};
    this.onLineReceived = () => {};
    this.onProgress = () => {};
    this.onError = () => {};
    
    // Send Queue
    this.queue = [];
    this.queueIndex = 0;
    this.sending = false;
    this.paused = false;
    this.waitingForOk = false;
    
    // Status Query Timer
    this.statusInterval = null;
    this.rxBuffer = '';
    
    // Connection guard
    this._connecting = false;
  }

  isSupported() {
    return 'serial' in navigator;
  }

  async connect(baudRate = 115200) {
    if (!this.isSupported()) {
      throw new Error('Web Serial API is not supported in this browser. Use Chrome, Edge, or Opera.');
    }
    
    // Prevent double-connect
    if (this.connected || this._connecting) {
      throw new Error('Already connected or connection in progress.');
    }
    
    this._connecting = true;
    
    try {
      this.port = await navigator.serial.requestPort();
      await this.port.open({ baudRate });
      this.connected = true;
      this.baudRate = baudRate;
      
      this.writer = this.port.writable.getWriter();
      this.readPromise = this.startReading();
      this.startStatusPolling();
      
      this.onConnect();
      return true;
    } catch (err) {
      console.error('Failed to connect:', err);
      this._surfaceError('Connection failed', err);
      this.disconnect();
      throw err;
    } finally {
      this._connecting = false;
    }
  }

  async disconnect() {
    if (this._disconnecting) return;
    this._disconnecting = true;

    this.stopStatusPolling();
    this.stopSending();
    
    if (this.reader) {
      try {
        await this.reader.cancel();
      } catch (e) {}
      // Do not nullify this.reader here; let finally block release the lock
    }
    
    if (this.readPromise) {
      const p = this.readPromise;
      this.readPromise = null;
      try { await p; } catch(e) {}
    }
    
    if (this.writer) {
      try {
        this.writer.releaseLock();
      } catch (e) {}
      this.writer = null;
    }
    
    if (this.port) {
      try {
        await this.port.close();
      } catch (e) {}
      this.port = null;
    }
    
    const wasConnected = this.connected;
    this.connected = false;
    this._connecting = false;
    this._disconnecting = false;
    if (wasConnected) {
      this.onDisconnect();
    }
  }

  async startReading() {
    while (this.port && this.port.readable) {
      try {
        this.reader = this.port.readable.getReader();
        const decoder = new TextDecoder();
        
        while (true) {
          const { value, done } = await this.reader.read();
          if (done) {
            break;
          }
          if (value) {
            const chunk = decoder.decode(value);
            this.handleIncomingData(chunk);
          }
        }
      } catch (err) {
        console.error('Serial read error:', err);
        this._surfaceError('Serial read error', err);
        break;
      } finally {
        if (this.reader) {
          try { this.reader.releaseLock(); } catch(e) {}
          this.reader = null;
        }
      }
    }
    this.readPromise = null;
    this.disconnect();
  }

  handleIncomingData(chunk) {
    this.rxBuffer += chunk;
    let newlineIndex;
    
    while ((newlineIndex = this.rxBuffer.indexOf('\n')) !== -1) {
      const line = this.rxBuffer.slice(0, newlineIndex).trim();
      this.rxBuffer = this.rxBuffer.slice(newlineIndex + 1);
      
      if (line) {
        this.handleLine(line);
      }
    }
  }

  handleLine(line) {
    this.onLineReceived(line);
    
    // Grbl-style responses
    if (line === 'ok') {
      this.waitingForOk = false;
      this.sendNext();
    } else if (line.startsWith('error:')) {
      console.warn('Machine reported error:', line);
      this._surfaceError('Machine error', new Error(line));
      this.waitingForOk = false;
      this.sendNext();
    } else if (line.startsWith('<') && line.endsWith('>')) {
      this.parseStatusReport(line);
    }
  }

  parseStatusReport(line) {
    // Format: <Idle|MPos:10.000,20.000,0.000|B:0|L:100>
    const clean = line.slice(1, -1);
    const parts = clean.split('|');
    const state = parts[0];
    let mPos = { x: 0, y: 0, z: 0 };
    let buffer = 0;
    
    parts.forEach(part => {
      if (part.startsWith('MPos:')) {
        const coords = part.substring(5).split(',').map(parseFloat);
        mPos = { x: coords[0] || 0, y: coords[1] || 0, z: coords[2] || 0 };
      } else if (part.startsWith('B:')) {
        buffer = parseInt(part.substring(2)) || 0;
      }
    });
    
    this.onStatus({ state, mPos, buffer });
  }

  startStatusPolling() {
    this.stopStatusPolling();
    // Query status every 300ms
    this.statusInterval = setInterval(() => {
      if (this.connected) {
        this.sendRealtimeCharacter('?');
      }
    }, 300);
  }

  stopStatusPolling() {
    if (this.statusInterval) {
      clearInterval(this.statusInterval);
      this.statusInterval = null;
    }
  }

  async sendRealtimeCharacter(char) {
    if (!this.connected || !this.writer) return;
    try {
      const encoder = new TextEncoder();
      const data = encoder.encode(char);
      await this.writer.write(data);
    } catch (err) {
      console.error('Failed to send realtime character:', err);
      this._surfaceError('Send failed', err);
    }
  }

  async sendLine(line) {
    if (!this.connected || !this.writer) return;
    try {
      const encoder = new TextEncoder();
      const data = encoder.encode(line + '\n');
      await this.writer.write(data);
    } catch (err) {
      console.error('Failed to send line:', err);
      this._surfaceError('Send failed — disconnecting', err);
      this.disconnect();
    }
  }

  // Queue Sending Operations
  startSending(gcodeLines) {
    this.queue = gcodeLines.map(l => l.trim()).filter(l => l.length > 0 && !l.startsWith(';'));
    this.queueIndex = 0;
    this.sending = true;
    this.paused = false;
    this.waitingForOk = false;
    this.sendNext();
  }

  stopSending() {
    this.sending = false;
    this.paused = false;
    this.queue = [];
    this.queueIndex = 0;
    this.waitingForOk = false;
  }

  pauseSending() {
    this.paused = true;
    this.sendRealtimeCharacter('!'); // Feed hold
  }

  resumeSending() {
    this.paused = false;
    this.sendRealtimeCharacter('~'); // Resume cycle
    this.sendNext();
  }

  sendNext() {
    if (!this.sending || this.paused || this.waitingForOk) return;
    
    if (this.queueIndex >= this.queue.length) {
      this.sending = false;
      this.onProgress(100, this.queueIndex, this.queue.length);
      return;
    }
    
    const line = this.queue[this.queueIndex];
    this.queueIndex++;
    this.waitingForOk = true;
    
    const progressPercent = Math.round((this.queueIndex / this.queue.length) * 100);
    this.onProgress(progressPercent, this.queueIndex, this.queue.length);
    
    this.sendLine(line);
  }

  /**
   * Internal helper — surfaces errors to the UI via the onError callback
   */
  _surfaceError(context, err) {
    try {
      this.onError(context, err);
    } catch (e) {
      // Prevent onError handler from crashing the serial manager
      console.error('Error in onError callback:', e);
    }
  }
}
export const serial = new SerialManager();

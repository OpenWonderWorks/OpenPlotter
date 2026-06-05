/**
 * OpenPlotter — WebSocket Manager
 * Manages WebSocket connectivity for ESP32 and ESP32-bridge setups.
 * Provides a similar API to SerialManager for easy swapping.
 */

export class WebSocketManager {
  constructor() {
    this.ws = null;
    this.connected = false;
    this.ip = '';
    
    // Callbacks
    this.onStatus = () => {};
    this.onConnect = () => {};
    this.onDisconnect = () => {};
    this.onLineReceived = () => {};
    this.onProgress = () => {};
    
    // Send Queue
    this.queue = [];
    this.queueIndex = 0;
    this.sending = false;
    this.paused = false;
    this.waitingForOk = false;
    
    // Status Query Timer
    this.statusInterval = null;
  }

  connect(ipAddress) {
    return new Promise((resolve, reject) => {
      this.disconnect();
      this.ip = ipAddress;
      
      const wsUrl = `ws://${ipAddress}/ws`;
      console.log(`Connecting to WebSocket at ${wsUrl}`);
      
      try {
        this.ws = new WebSocket(wsUrl);
      } catch (err) {
        reject(err);
        return;
      }
      
      this.ws.onopen = () => {
        this.connected = true;
        this.startStatusPolling();
        this.onConnect();
        resolve(true);
      };
      
      this.ws.onclose = () => {
        this.handleDisconnect();
      };
      
      this.ws.onerror = (err) => {
        console.error('WebSocket error:', err);
        reject(err);
      };
      
      this.ws.onmessage = (event) => {
        this.handleMessage(event.data);
      };
    });
  }

  disconnect() {
    this.stopStatusPolling();
    this.stopSending();
    
    if (this.ws) {
      try {
        this.ws.close();
      } catch (e) {}
      this.ws = null;
    }
    
    this.handleDisconnect();
  }

  handleDisconnect() {
    if (this.connected) {
      this.connected = false;
      this.onDisconnect();
    }
  }

  handleMessage(data) {
    const lines = data.split('\n');
    lines.forEach(line => {
      const trimmed = line.trim();
      if (!trimmed) return;
      
      this.onLineReceived(trimmed);
      
      if (trimmed === 'ok') {
        this.waitingForOk = false;
        this.sendNext();
      } else if (trimmed.startsWith('error:')) {
        console.warn('Machine reported error via WS:', trimmed);
        this.waitingForOk = false;
        this.sendNext();
      } else if (trimmed.startsWith('<') && trimmed.endsWith('>')) {
        this.parseStatusReport(trimmed);
      }
    });
  }

  parseStatusReport(line) {
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

  sendRealtimeCharacter(char) {
    if (!this.connected || !this.ws) return;
    try {
      this.ws.send(char);
    } catch (err) {
      console.error('Failed to send WS character:', err);
    }
  }

  sendLine(line) {
    if (!this.connected || !this.ws) return;
    try {
      this.ws.send(line + '\n');
    } catch (err) {
      console.error('Failed to send WS line:', err);
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
}
export const websocket = new WebSocketManager();

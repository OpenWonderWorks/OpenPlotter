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
    this.onError = () => {};
    
    // Send Queue
    this.queue = [];
    this.queueIndex = 0;
    this.sending = false;
    this.paused = false;
    this.waitingForOk = false;
    
    // Status Query Timer
    this.statusInterval = null;
    
    // Connection guard
    this._connecting = false;
    
    // Auto-reconnect state
    this._autoReconnect = false;
    this._reconnectAttempts = 0;
    this._reconnectTimer = null;
    this._maxReconnectDelay = 30000; // 30s max
    this._wasIntentionalDisconnect = false;
    
    // Heartbeat
    this._heartbeatInterval = null;
    this._lastPong = 0;
    this._heartbeatTimeoutMs = 10000; // 10s without a response = stale
  }

  connect(ipAddress) {
    return new Promise((resolve, reject) => {
      // Prevent double-connect
      if (this.connected || this._connecting) {
        reject(new Error('Already connected or connection in progress.'));
        return;
      }
      
      this._wasIntentionalDisconnect = false;
      this._connecting = true;
      this.ip = ipAddress;
      
      const wsUrl = `ws://${ipAddress}/ws`;
      console.log(`Connecting to WebSocket at ${wsUrl}`);
      
      // Connection timeout
      const connectTimeout = setTimeout(() => {
        if (!this.connected) {
          this._connecting = false;
          if (this.ws) {
            try { this.ws.close(); } catch (e) {}
            this.ws = null;
          }
          const err = new Error(`WebSocket connection timed out after 5 seconds (${wsUrl})`);
          this._surfaceError('Connection timeout', err);
          reject(err);
        }
      }, 5000);
      
      try {
        this.ws = new WebSocket(wsUrl);
      } catch (err) {
        clearTimeout(connectTimeout);
        this._connecting = false;
        reject(err);
        return;
      }
      
      this.ws.onopen = () => {
        clearTimeout(connectTimeout);
        this._connecting = false;
        this.connected = true;
        this._reconnectAttempts = 0;
        this._lastPong = Date.now();
        this.startStatusPolling();
        this._startHeartbeat();
        this.onConnect();
        resolve(true);
      };
      
      this.ws.onclose = () => {
        clearTimeout(connectTimeout);
        this._connecting = false;
        this._stopHeartbeat();
        this._handleDisconnect();
      };
      
      this.ws.onerror = (err) => {
        clearTimeout(connectTimeout);
        this._connecting = false;
        console.error('WebSocket error:', err);
        this._surfaceError('WebSocket error', err);
        reject(err);
      };
      
      this.ws.onmessage = (event) => {
        this._lastPong = Date.now();
        this.handleMessage(event.data);
      };
    });
  }

  disconnect() {
    this._wasIntentionalDisconnect = true;
    this._clearReconnectTimer();
    this.stopStatusPolling();
    this._stopHeartbeat();
    this.stopSending();
    
    if (this.ws) {
      try {
        this.ws.close();
      } catch (e) {}
      this.ws = null;
    }
    
    this._handleDisconnect();
  }

  _handleDisconnect() {
    if (this.connected) {
      this.connected = false;
      this.onDisconnect();
      
      // Auto-reconnect on unexpected disconnects (e.g., WiFi drop during a cut)
      if (!this._wasIntentionalDisconnect && this.sending) {
        this._scheduleReconnect();
      }
    }
    this._connecting = false;
  }

  _scheduleReconnect() {
    this._clearReconnectTimer();
    this._reconnectAttempts++;
    
    // Exponential backoff: 1s, 2s, 4s, 8s, ... capped at 30s
    const delay = Math.min(1000 * Math.pow(2, this._reconnectAttempts - 1), this._maxReconnectDelay);
    
    console.log(`WebSocket: scheduling reconnect attempt ${this._reconnectAttempts} in ${delay}ms`);
    this._surfaceError('Connection lost', new Error(`Reconnecting in ${Math.round(delay / 1000)}s (attempt ${this._reconnectAttempts})...`));
    
    this._reconnectTimer = setTimeout(async () => {
      if (this._wasIntentionalDisconnect) return;
      
      try {
        await this.connect(this.ip);
        console.log('WebSocket: reconnected successfully');
        // Resume sending if there's a queue
        if (this.queue.length > 0 && this.queueIndex < this.queue.length) {
          this.sending = true;
          this.paused = false;
          this.waitingForOk = false;
          this.sendNext();
        }
      } catch (err) {
        console.warn('WebSocket: reconnect failed:', err.message);
        // Will schedule next attempt via onclose -> _handleDisconnect
      }
    }, delay);
  }

  _clearReconnectTimer() {
    if (this._reconnectTimer) {
      clearTimeout(this._reconnectTimer);
      this._reconnectTimer = null;
    }
  }

  _startHeartbeat() {
    this._stopHeartbeat();
    this._lastPong = Date.now();
    
    this._heartbeatInterval = setInterval(() => {
      if (!this.connected) return;
      
      const elapsed = Date.now() - this._lastPong;
      if (elapsed > this._heartbeatTimeoutMs) {
        console.warn(`WebSocket: no data received for ${elapsed}ms — connection may be stale`);
        this._surfaceError('Connection stale', new Error('No data received from plotter — check WiFi connection'));
      }
    }, 5000);
  }

  _stopHeartbeat() {
    if (this._heartbeatInterval) {
      clearInterval(this._heartbeatInterval);
      this._heartbeatInterval = null;
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
        this._surfaceError('Machine error', new Error(trimmed));
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
    if (!this.connected || !this.ws || this.ws.readyState !== WebSocket.OPEN) return;
    try {
      this.ws.send(char);
    } catch (err) {
      console.error('Failed to send WS character:', err);
      this._surfaceError('Send failed', err);
    }
  }

  sendLine(line) {
    if (!this.connected || !this.ws || this.ws.readyState !== WebSocket.OPEN) return;
    try {
      this.ws.send(line + '\n');
    } catch (err) {
      console.error('Failed to send WS line:', err);
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
      console.error('Error in onError callback:', e);
    }
  }
}
export const websocket = new WebSocketManager();

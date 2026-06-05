import './style.css';
import { parseSVG } from './lib/svg-parser.js';
import { generateGcode } from './lib/gcode-generator.js';
import { serial } from './lib/serial.js';
import { websocket } from './lib/websocket.js';

// Application State
const state = {
  connectionMode: 'usb', // 'usb' or 'wifi'
  connection: null,      // active connection instance (serial or websocket)
  connected: false,
  svgData: null,         // parsed SVG paths & dimensions
  gcode: '',             // generated G-code text
  currentPosition: { x: 0, y: 0, z: 0 },
  machineState: 'DISCONNECTED',
  activeTab: 'tab-control',
  
  // Canvas drawing state
  zoom: 1.5,
  panX: 50,
  panY: 50,
  isDragging: false,
  lastMouseX: 0,
  lastMouseY: 0,
  bedSizeX: 300,
  bedSizeY: 300,
  
  // Settings
  settings: {},
  profileSettings: {
    vinyl: { pressure: 90, speed: 1500, passes: 1 },
    cardstock: { pressure: 220, speed: 800, passes: 2 },
    paper: { pressure: 60, speed: 1800, passes: 1 },
    custom: { pressure: 120, speed: 1500, passes: 1 }
  },
  currentProfile: 'vinyl',
  
  // Tool state (for test button)
  toolDown: false
};

// DOM Cache
const dom = {
  statusBadge: document.getElementById('status-badge'),
  btnShowConnect: document.getElementById('btn-show-connect'),
  btnDisconnect: document.getElementById('btn-disconnect'),
  
  // Modals
  modalConnect: document.getElementById('modal-connect'),
  btnCloseConnect: document.getElementById('btn-close-connect'),
  btnModalCancel: document.getElementById('btn-modal-cancel'),
  btnModalConnect: document.getElementById('btn-modal-connect'),
  connModeRadio: document.getElementsByName('conn-mode'),
  connParamsUsb: document.getElementById('conn-params-usb'),
  connParamsWifi: document.getElementById('conn-params-wifi'),
  connUsbBaud: document.getElementById('conn-usb-baud'),
  connWifiIp: document.getElementById('conn-wifi-ip'),
  
  // File Import
  dropzone: document.getElementById('dropzone'),
  fileInput: document.getElementById('file-input'),
  svgInfo: document.getElementById('svg-info'),
  svgW: document.getElementById('svg-w'),
  svgH: document.getElementById('svg-h'),
  svgPaths: document.getElementById('svg-paths'),
  
  // Configs
  cfgScale: document.getElementById('cfg-scale'),
  cfgTolerance: document.getElementById('cfg-tolerance'),
  cfgOffsetX: document.getElementById('cfg-offset-x'),
  cfgOffsetY: document.getElementById('cfg-offset-y'),
  
  // Material/Cutting
  profileCards: document.querySelectorAll('.profile-card'),
  cfgPressure: document.getElementById('cfg-pressure'),
  cfgSpeedLinear: document.getElementById('cfg-speed-linear'),
  cfgSpeedRapid: document.getElementById('cfg-speed-rapid'),
  cfgPasses: document.getElementById('cfg-passes'),
  cfgUpDownDelays: document.getElementById('cfg-updown-delays'),
  
  // Canvas Actions
  btnCanvasFit: document.getElementById('btn-canvas-fit'),
  btnCanvasClear: document.getElementById('btn-canvas-clear'),
  btnGenGcode: document.getElementById('btn-gen-gcode'),
  btnStartCut: document.getElementById('btn-start-cut'),
  btnPauseCut: document.getElementById('btn-pause-cut'),
  btnStopCut: document.getElementById('btn-stop-cut'),
  
  // Canvas
  canvas: document.getElementById('plotter-canvas'),
  canvasContainer: document.getElementById('canvas-container'),
  
  // Tabs
  tabHeaders: document.querySelectorAll('.tab-header'),
  tabContents: document.querySelectorAll('.tab-content'),
  
  // Controls
  btnEstop: document.getElementById('btn-estop'),
  btnUnlock: document.getElementById('btn-unlock'),
  jogYPlus: document.getElementById('jog-y-plus'),
  jogYMinus: document.getElementById('jog-y-minus'),
  jogXPlus: document.getElementById('jog-x-plus'),
  jogXMinus: document.getElementById('jog-x-minus'),
  btnHome: document.getElementById('btn-home'),
  cfgJogStep: document.getElementById('cfg-jog-step'),
  btnZeroXY: document.getElementById('btn-zero-xy'),
  btnToolTest: document.getElementById('btn-tool-test'),
  
  // Quick utilities
  btnCmdStatus: document.getElementById('btn-cmd-status'),
  btnCmdEndstops: document.getElementById('btn-cmd-endstops'),
  btnCmdHelp: document.getElementById('btn-cmd-help'),
  btnCmdMotorsOff: document.getElementById('btn-cmd-motors-off'),
  
  // Console
  terminalLog: document.getElementById('terminal-log'),
  terminalInput: document.getElementById('terminal-input'),
  btnTerminalSend: document.getElementById('btn-terminal-send'),
  btnTerminalClear: document.getElementById('btn-terminal-clear'),
  
  // Settings Tab
  settingsLoading: document.getElementById('settings-loading'),
  btnSettingsLoad: document.getElementById('btn-settings-load'),
  settingsList: document.getElementById('settings-list'),
  settingsActions: document.getElementById('settings-actions'),
  btnSettingsSave: document.getElementById('btn-settings-save'),
  btnSettingsReset: document.getElementById('btn-settings-reset'),
  
  // Bottom Progress
  progressText: document.getElementById('progress-text'),
  progressFill: document.getElementById('progress-fill'),
  progressStats: document.getElementById('progress-stats'),
  coordX: document.getElementById('coord-x'),
  coordY: document.getElementById('coord-y'),
  coordZ: document.getElementById('coord-z')
};

// Canvas 2D context
const ctx = dom.canvas.getContext('2d');

// Initialize Canvas Sizing
function resizeCanvas() {
  const containerWidth = dom.canvasContainer.clientWidth;
  const containerHeight = dom.canvasContainer.clientHeight;
  const size = Math.min(containerWidth, containerHeight, 600) - 32;
  dom.canvas.width = size;
  dom.canvas.height = size;
  drawCanvas();
}
window.addEventListener('resize', resizeCanvas);
// Run initial resize
setTimeout(resizeCanvas, 100);

// Set default connection
state.connection = serial;

// --- Connectivity Managers ---
function setupConnectionHandlers() {
  // Bind Connection Dialog trigger
  dom.btnShowConnect.addEventListener('click', () => {
    dom.modalConnect.classList.add('open');
  });
  
  dom.btnCloseConnect.addEventListener('click', () => dom.modalConnect.classList.remove('open'));
  dom.btnModalCancel.addEventListener('click', () => dom.modalConnect.classList.remove('open'));
  
  // Toggle connection parameters visually based on Radio select
  dom.connModeRadio.forEach(radio => {
    radio.addEventListener('change', (e) => {
      state.connectionMode = e.target.value;
      if (state.connectionMode === 'usb') {
        dom.connParamsUsb.style.display = 'block';
        dom.connParamsWifi.style.display = 'none';
        state.connection = serial;
      } else {
        dom.connParamsUsb.style.display = 'none';
        dom.connParamsWifi.style.display = 'block';
        state.connection = websocket;
      }
    });
  });
  
  // Trigger connection
  dom.btnModalConnect.addEventListener('click', async () => {
    dom.btnModalConnect.disabled = true;
    dom.btnModalConnect.innerText = 'Connecting...';
    
    try {
      if (state.connectionMode === 'usb') {
        const baud = parseInt(dom.connUsbBaud.value);
        await serial.connect(baud);
      } else {
        const ip = dom.connWifiIp.value.trim();
        await websocket.connect(ip);
      }
      dom.modalConnect.classList.remove('open');
    } catch (err) {
      alert(`Connection failed: ${err.message || err}`);
    } finally {
      dom.btnModalConnect.disabled = false;
      dom.btnModalConnect.innerText = 'Connect';
    }
  });
  
  dom.btnDisconnect.addEventListener('click', async () => {
    await state.connection.disconnect();
  });
}

// Bind connection events
function registerConnectionCallbacks() {
  // Shared bindings for serial/websocket
  [serial, websocket].forEach(conn => {
    conn.onConnect = () => {
      state.connected = true;
      dom.btnShowConnect.style.display = 'none';
      dom.btnDisconnect.style.display = 'inline-flex';
      
      dom.terminalInput.disabled = false;
      dom.btnTerminalSend.disabled = false;
      dom.btnSettingsLoad.disabled = false;
      
      logToTerminal('System Connected.', 'info');
      updateStartCutStatus();
      
      // Request initial settings and status
      setTimeout(() => {
        conn.sendRealtimeCharacter('?');
        conn.sendLine('$$');
      }, 500);
    };
    
    conn.onDisconnect = () => {
      state.connected = false;
      dom.btnShowConnect.style.display = 'inline-flex';
      dom.btnDisconnect.style.display = 'none';
      
      dom.terminalInput.disabled = true;
      dom.btnTerminalSend.disabled = true;
      dom.btnSettingsLoad.disabled = true;
      
      state.machineState = 'DISCONNECTED';
      updateStatusBadge();
      logToTerminal('System Disconnected.', 'info');
      updateStartCutStatus();
    };
    
    conn.onLineReceived = (line) => {
      logToTerminal(line, 'rx');
      parseSettingsOutput(line);
    };
    
    conn.onStatus = (status) => {
      state.machineState = status.state;
      state.currentPosition = status.mPos;
      
      dom.coordX.innerText = status.mPos.x.toFixed(2);
      dom.coordY.innerText = status.mPos.y.toFixed(2);
      dom.coordZ.innerText = status.mPos.z.toFixed(2);
      
      updateStatusBadge();
      drawCanvas();
    };
    
    conn.onProgress = (percent, index, total) => {
      dom.progressFill.style.width = `${percent}%`;
      dom.progressText.innerText = `Progress: ${percent}%`;
      dom.progressStats.innerText = `(${index}/${total} lines)`;
      
      if (percent === 100) {
        logToTerminal('Cut Completed Successfully!', 'info');
        resetCutButtons();
      }
    };
  });
}

function updateStatusBadge() {
  dom.statusBadge.className = `status-indicator status-${state.machineState}`;
  dom.statusBadge.innerText = state.machineState;
}

function logToTerminal(text, type) {
  const line = document.createElement('div');
  line.className = `terminal-line terminal-${type}`;
  
  if (type === 'tx') {
    line.innerText = `> ${text}`;
  } else if (type === 'rx') {
    line.innerText = `< ${text}`;
  } else {
    line.innerText = text;
  }
  
  dom.terminalLog.appendChild(line);
  dom.terminalLog.scrollTop = dom.terminalLog.scrollHeight;
  
  // Cap terminal lines to prevent memory issues
  while (dom.terminalLog.childNodes.length > 200) {
    dom.terminalLog.removeChild(dom.terminalLog.firstChild);
  }
}

// --- SVG Parsing & Canvas UI ---
function setupFileImporter() {
  // Prevent browser default behaviors
  ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
    dom.dropzone.addEventListener(eventName, e => e.preventDefault(), false);
  });
  
  dom.dropzone.addEventListener('dragover', () => {
    dom.dropzone.style.borderColor = 'var(--accent-cyan)';
  });
  
  dom.dropzone.addEventListener('dragleave', () => {
    dom.dropzone.style.borderColor = 'var(--border-color)';
  });
  
  dom.dropzone.addEventListener('drop', (e) => {
    dom.dropzone.style.borderColor = 'var(--border-color)';
    const file = e.dataTransfer.files[0];
    if (file) handleSVGFile(file);
  });
  
  dom.dropzone.addEventListener('click', () => {
    dom.fileInput.click();
  });
  
  dom.fileInput.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (file) handleSVGFile(file);
  });
}

function handleSVGFile(file) {
  const reader = new FileReader();
  reader.onload = (e) => {
    const svgText = e.target.result;
    const tolerance = parseFloat(dom.cfgTolerance.value);
    
    try {
      state.svgData = parseSVG(svgText, tolerance);
      
      // Update info box
      dom.svgW.innerText = Math.round(state.svgData.width);
      dom.svgH.innerText = Math.round(state.svgData.height);
      dom.svgPaths.innerText = state.svgData.paths.length;
      dom.svgInfo.style.display = 'block';
      
      logToTerminal(`Parsed SVG. Found ${state.svgData.paths.length} paths.`, 'info');
      
      autoFitDesign();
      dom.btnGenGcode.disabled = false;
      drawCanvas();
    } catch (err) {
      alert(`Failed to parse SVG: ${err.message}`);
    }
  };
  reader.readAsText(file);
}

function autoFitDesign() {
  if (!state.svgData) return;
  
  // Calculate bounds of current SVG paths
  let minX = Infinity, maxX = -Infinity;
  let minY = Infinity, maxY = -Infinity;
  
  state.svgData.paths.forEach(path => {
    path.forEach(pt => {
      if (pt.x < minX) minX = pt.x;
      if (pt.x > maxX) maxX = pt.x;
      if (pt.y < minY) minY = pt.y;
      if (pt.y > maxY) maxY = pt.y;
    });
  });
  
  if (minX === Infinity) return; // empty svg
  
  const designWidth = maxX - minX;
  const designHeight = maxY - minY;
  
  // Target dimensions (with 10mm margins)
  const margin = 10;
  const targetW = state.bedSizeX - 2 * margin;
  const targetH = state.bedSizeY - 2 * margin;
  
  // Compute scale to fit best
  const scaleX = targetW / designWidth;
  const scaleY = targetH / designHeight;
  const finalScale = parseFloat(Math.min(scaleX, scaleY).toFixed(3));
  
  dom.cfgScale.value = finalScale;
  
  // Center it on the bed
  const scaledCenter = {
    x: minX + designWidth / 2,
    y: minY + designHeight / 2
  };
  const bedCenter = { x: state.bedSizeX / 2, y: state.bedSizeY / 2 };
  
  // Offset to match center
  dom.cfgOffsetX.value = Math.round(bedCenter.x - scaledCenter.x * finalScale);
  dom.cfgOffsetY.value = Math.round(bedCenter.y - scaledCenter.y * finalScale);
  
  logToTerminal(`Auto-fitted design. Scale: ${finalScale}, Offset X: ${dom.cfgOffsetX.value}, Offset Y: ${dom.cfgOffsetY.value}`, 'info');
}

// Canvas Drawing Loop
function drawCanvas() {
  if (!dom.canvas) return;
  
  ctx.clearRect(0, 0, dom.canvas.width, dom.canvas.height);
  
  ctx.save();
  // Set pan and zoom transforms
  ctx.translate(state.panX, state.panY);
  ctx.scale(state.zoom, state.zoom);
  
  // 1. Draw Plotter Bed boundary (0,0 to 300,300)
  ctx.fillStyle = '#0a0d16';
  ctx.fillRect(0, 0, state.bedSizeX, state.bedSizeY);
  
  // Draw bed border grid
  ctx.strokeStyle = '#1e293b';
  ctx.lineWidth = 1;
  ctx.strokeRect(0, 0, state.bedSizeX, state.bedSizeY);
  
  // Grid Lines (every 10mm thin, 50mm thick)
  for (let x = 10; x < state.bedSizeX; x += 10) {
    ctx.strokeStyle = (x % 50 === 0) ? '#334155' : '#1e293b';
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, state.bedSizeY);
    ctx.stroke();
  }
  for (let y = 10; y < state.bedSizeY; y += 10) {
    ctx.strokeStyle = (y % 50 === 0) ? '#334155' : '#1e293b';
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(state.bedSizeX, y);
    ctx.stroke();
  }
  
  // 2. Draw SVG paths
  if (state.svgData) {
    const scale = parseFloat(dom.cfgScale.value) || 1.0;
    const offX = parseFloat(dom.cfgOffsetX.value) || 0;
    const offY = parseFloat(dom.cfgOffsetY.value) || 0;
    
    ctx.strokeStyle = 'rgba(0, 230, 243, 0.7)';
    ctx.lineWidth = 1.5;
    
    state.svgData.paths.forEach(path => {
      if (path.length === 0) return;
      
      ctx.beginPath();
      // Apply offset and scaling
      const first = path[0];
      ctx.moveTo(first.x * scale + offX, first.y * scale + offY);
      
      for (let i = 1; i < path.length; i++) {
        const pt = path[i];
        ctx.lineTo(pt.x * scale + offX, pt.y * scale + offY);
      }
      ctx.stroke();
    });
  }
  
  // 3. Draw active pen/blade tool position
  const tx = state.currentPosition.x;
  const ty = state.currentPosition.y;
  
  // Draw glowing crosshair at current coordinate
  ctx.strokeStyle = '#ff007f';
  ctx.lineWidth = 1;
  ctx.beginPath();
  // Horizontal crosshair line
  ctx.moveTo(tx - 10, ty);
  ctx.lineTo(tx + 10, ty);
  // Vertical crosshair line
  ctx.moveTo(tx, ty - 10);
  ctx.lineTo(tx, ty + 10);
  ctx.stroke();
  
  // Inner circle
  ctx.fillStyle = '#ff007f';
  ctx.beginPath();
  ctx.arc(tx, ty, 3, 0, 2 * Math.PI);
  ctx.fill();
  
  // Glow effect
  ctx.strokeStyle = 'rgba(255, 0, 127, 0.4)';
  ctx.lineWidth = 3;
  ctx.beginPath();
  ctx.arc(tx, ty, 6, 0, 2 * Math.PI);
  ctx.stroke();
  
  ctx.restore();
  
  // Draw bed origin label
  ctx.fillStyle = 'rgba(255, 255, 255, 0.4)';
  ctx.font = '10px monospace';
  ctx.fillText('Origin (0,0)', 10 + state.panX, -10 + state.panY + state.bedSizeY * state.zoom);
}

// Interactive zoom & pan handlers on canvas
function setupCanvasInteractions() {
  dom.canvas.addEventListener('mousedown', (e) => {
    state.isDragging = true;
    state.lastMouseX = e.clientX;
    state.lastMouseY = e.clientY;
  });
  
  window.addEventListener('mouseup', () => {
    state.isDragging = false;
  });
  
  dom.canvas.addEventListener('mousemove', (e) => {
    if (state.isDragging) {
      const dx = e.clientX - state.lastMouseX;
      const dy = e.clientY - state.lastMouseY;
      state.panX += dx;
      state.panY += dy;
      state.lastMouseX = e.clientX;
      state.lastMouseY = e.clientY;
      drawCanvas();
    }
  });
  
  dom.canvas.addEventListener('wheel', (e) => {
    e.preventDefault();
    const zoomFactor = 1.1;
    
    // Zoom centered on mouse position
    const rect = dom.canvas.getBoundingClientRect();
    const mouseX = e.clientX - rect.left;
    const mouseY = e.clientY - rect.top;
    
    // Position of mouse relative to bed before zoom
    const bedX = (mouseX - state.panX) / state.zoom;
    const bedY = (mouseY - state.panY) / state.zoom;
    
    if (e.deltaY < 0) {
      state.zoom *= zoomFactor;
    } else {
      state.zoom /= zoomFactor;
    }
    state.zoom = Math.max(0.5, Math.min(10, state.zoom));
    
    // Update pan to center on bed coordinates
    state.panX = mouseX - bedX * state.zoom;
    state.panY = mouseY - bedY * state.zoom;
    
    drawCanvas();
  }, { passive: false });
  
  // Fit button
  dom.btnCanvasFit.addEventListener('click', () => {
    const scale = Math.min(dom.canvas.width / state.bedSizeX, dom.canvas.height / state.bedSizeY) * 0.9;
    state.zoom = scale;
    state.panX = (dom.canvas.width - state.bedSizeX * scale) / 2;
    state.panY = (dom.canvas.height - state.bedSizeY * scale) / 2;
    drawCanvas();
  });
  
  // Clear button
  dom.btnCanvasClear.addEventListener('click', () => {
    state.svgData = null;
    state.gcode = '';
    dom.svgInfo.style.display = 'none';
    dom.btnGenGcode.disabled = true;
    updateStartCutStatus();
    drawCanvas();
    logToTerminal('Design cleared.', 'info');
  });
}

// --- Parameter Toggles & G-code Generator ---
function setupParamHandlers() {
  // Recalculate G-code if scaling inputs or offset inputs change
  [dom.cfgScale, dom.cfgTolerance, dom.cfgOffsetX, dom.cfgOffsetY].forEach(input => {
    input.addEventListener('change', () => {
      // Re-trigger draw
      drawCanvas();
    });
  });
  
  // Material profile selection cards
  dom.profileCards.forEach(card => {
    card.addEventListener('click', () => {
      dom.profileCards.forEach(c => c.classList.remove('active'));
      card.classList.add('active');
      
      const profileName = card.dataset.profile;
      state.currentProfile = profileName;
      
      const p = state.profileSettings[profileName];
      if (profileName !== 'custom') {
        dom.cfgPressure.value = p.pressure;
        dom.cfgSpeedLinear.value = p.speed;
        dom.cfgPasses.value = p.passes;
      }
    });
  });
  
  // Hook manual edits to trigger 'custom' profile active
  [dom.cfgPressure, dom.cfgSpeedLinear, dom.cfgPasses].forEach(input => {
    input.addEventListener('input', () => {
      dom.profileCards.forEach(c => c.classList.remove('active'));
      document.querySelector('[data-profile="custom"]').classList.add('active');
      state.currentProfile = 'custom';
    });
  });
  
  // Generate G-code button handler
  dom.btnGenGcode.addEventListener('click', () => {
    if (!state.svgData) return;
    
    const scale = parseFloat(dom.cfgScale.value) || 1.0;
    const offX = parseFloat(dom.cfgOffsetX.value) || 0;
    const offY = parseFloat(dom.cfgOffsetY.value) || 0;
    const pressure = parseInt(dom.cfgPressure.value) || 90;
    const speed = parseInt(dom.cfgSpeedLinear.value) || 1500;
    const rapidSpeed = parseInt(dom.cfgSpeedRapid.value) || 3000;
    const passes = parseInt(dom.cfgPasses.value) || 1;
    const delay = parseInt(dom.cfgUpDownDelays.value) || 150;
    
    state.gcode = generateGcode(state.svgData.paths, {
      feedRateLinear: speed,
      feedRateRapid: rapidSpeed,
      bladePressure: pressure,
      toolDownDelay: delay,
      toolUpDelay: delay,
      passCount: passes,
      scale: scale,
      offsetX: offX,
      offsetY: offY,
      invertY: true,
      svgSize: { w: state.svgData.width, h: state.svgData.height }
    });
    
    logToTerminal(`Generated ${state.gcode.split('\n').length} lines of G-code. Ready to cut.`, 'info');
    updateStartCutStatus();
  });
}

function updateStartCutStatus() {
  const isReady = state.connected && state.gcode.length > 0;
  dom.btnStartCut.disabled = !isReady;
}

// --- Active Homing & Cutting Control panel ---
function setupCutterHandlers() {
  // Start Cut
  dom.btnStartCut.addEventListener('click', () => {
    if (!state.gcode) return;
    
    const lines = state.gcode.split('\n');
    logToTerminal(`Starting cut queue with ${lines.length} lines...`, 'info');
    
    state.connection.startSending(lines);
    
    // Toggle buttons
    dom.btnStartCut.style.display = 'none';
    dom.btnPauseCut.style.display = 'inline-flex';
    dom.btnPauseCut.innerText = 'Pause';
    dom.btnStopCut.style.display = 'inline-flex';
    
    // Disable layout options while running
    setInputsDisabled(true);
  });
  
  // Pause Cut
  dom.btnPauseCut.addEventListener('click', () => {
    if (state.connection.paused) {
      logToTerminal('Resuming cut segment...', 'info');
      state.connection.resumeSending();
      dom.btnPauseCut.innerText = 'Pause';
    } else {
      logToTerminal('Pausing cut segment (feed hold)...', 'info');
      state.connection.pauseSending();
      dom.btnPauseCut.innerText = 'Resume';
    }
  });
  
  // Abort Cut
  dom.btnStopCut.addEventListener('click', () => {
    logToTerminal('Aborting cut queue!', 'info');
    state.connection.stopSending();
    resetCutButtons();
    
    // Send reset
    state.connection.sendRealtimeCharacter(String.fromCharCode(24)); // Ctrl+X Reset
  });
  
  // E-stop button
  dom.btnEstop.addEventListener('click', () => {
    logToTerminal('EMERGENCY STOP PRESSED!', 'info');
    state.connection.sendRealtimeCharacter('!'); // Feed hold
    state.connection.sendRealtimeCharacter(String.fromCharCode(24)); // Reset
    if (state.connection.sending) {
      state.connection.stopSending();
    }
    resetCutButtons();
  });
  
  // Unlock button ($X)
  dom.btnUnlock.addEventListener('click', () => {
    logToTerminal('Sending Unlock command ($X)...', 'info');
    state.connection.sendLine('$X');
  });
  
  // Homing button
  dom.btnHome.addEventListener('click', () => {
    logToTerminal('Initializing Homing Cycle ($H)...', 'info');
    state.connection.sendLine('$H');
  });
  
  // Zero XY
  dom.btnZeroXY.addEventListener('click', () => {
    logToTerminal('Zeroing work coordinates (G92 X0 Y0)...', 'info');
    state.connection.sendLine('G92 X0 Y0');
  });
  
  // Test Tool (toggle M3 / M5)
  dom.btnToolTest.addEventListener('click', () => {
    const pressure = parseInt(dom.cfgPressure.value) || 90;
    if (state.toolDown) {
      logToTerminal('Test Tool: LIFT (M5)', 'info');
      state.connection.sendLine('M5');
      state.toolDown = false;
    } else {
      logToTerminal(`Test Tool: DROP (M3 S${pressure})`, 'info');
      state.connection.sendLine(`M3 S${pressure}`);
      state.toolDown = true;
    }
  });
  
  // Jog Buttons
  dom.jogXMinus.addEventListener('click', () => sendJogMove(-1, 0));
  dom.jogXPlus.addEventListener('click', () => sendJogMove(1, 0));
  dom.jogYMinus.addEventListener('click', () => sendJogMove(0, -1)); // Grid Y- is down
  dom.jogYPlus.addEventListener('click', () => sendJogMove(0, 1));  // Grid Y+ is up
  
  // Quick utils
  dom.btnCmdStatus.addEventListener('click', () => state.connection.sendRealtimeCharacter('?'));
  dom.btnCmdEndstops.addEventListener('click', () => state.connection.sendLine('M119'));
  dom.btnCmdHelp.addEventListener('click', () => state.connection.sendLine('M100'));
  dom.btnCmdMotorsOff.addEventListener('click', () => state.connection.sendLine('M18'));
}

function sendJogMove(dirX, dirY) {
  const step = parseFloat(dom.cfgJogStep.value) || 1.0;
  const speed = 2500;
  const dx = dirX * step;
  const dy = dirY * step;
  
  // Invert Y direction if needed to match coordinates
  // SVG top-left, physical coordinates bottom-left:
  // Usually, pushing UP button moves motor Y in positive direction. So dy * step is positive.
  
  logToTerminal(`Jogging: G91 G0 X${dx} Y${dy} F${speed}`, 'info');
  state.connection.sendLine(`G21 G91 G0 X${dx} Y${dy} F${speed} G90`);
}

function resetCutButtons() {
  dom.btnStartCut.style.display = 'inline-flex';
  dom.btnPauseCut.style.display = 'none';
  dom.btnStopCut.style.display = 'none';
  setInputsDisabled(false);
}

function setInputsDisabled(disabled) {
  dom.dropzone.style.pointerEvents = disabled ? 'none' : 'auto';
  dom.fileInput.disabled = disabled;
  dom.cfgScale.disabled = disabled;
  dom.cfgTolerance.disabled = disabled;
  dom.cfgOffsetX.disabled = disabled;
  dom.cfgOffsetY.disabled = disabled;
  dom.cfgPressure.disabled = disabled;
  dom.cfgSpeedLinear.disabled = disabled;
  dom.cfgSpeedRapid.disabled = disabled;
  dom.cfgPasses.disabled = disabled;
  dom.cfgUpDownDelays.disabled = disabled;
}

// --- Tabs Manager ---
function setupTabHandlers() {
  dom.tabHeaders.forEach(header => {
    header.addEventListener('click', () => {
      const tabId = header.dataset.tab;
      state.activeTab = tabId;
      
      // Update header states
      dom.tabHeaders.forEach(h => h.classList.remove('active'));
      header.classList.add('active');
      
      // Update pane states
      dom.tabContents.forEach(pane => {
        if (pane.id === tabId) {
          pane.classList.add('active');
        } else {
          pane.classList.remove('active');
        }
      });
    });
  });
}

// --- Console Terminal Handlers ---
function setupConsoleHandlers() {
  dom.btnTerminalSend.addEventListener('click', sendConsoleInput);
  
  dom.terminalInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
      sendConsoleInput();
    }
  });
  
  dom.btnTerminalClear.addEventListener('click', () => {
    dom.terminalLog.innerHTML = '<div class="terminal-line terminal-info">Console cleared.</div>';
  });
}

function sendConsoleInput() {
  const val = dom.terminalInput.value.trim();
  if (val) {
    logToTerminal(val, 'tx');
    state.connection.sendLine(val);
    dom.terminalInput.value = '';
  }
}

// --- Settings ($) System Parser & Form ---
function parseSettingsOutput(line) {
  // Matches "$100=80.000 (steps/mm X)" or "$100=80.000"
  const match = line.match(/^\$(\d+)=([\d\.-]+)(?:\s*\((.*)\))?/);
  if (match) {
    const num = parseInt(match[1]);
    const val = parseFloat(match[2]);
    const desc = match[3] || getSettingDescription(num);
    
    state.settings[num] = { val, desc };
    
    // If we are parsing settings, show the UI
    renderSettingsForm();
  }
}

function getSettingDescription(num) {
  const defaults = {
    100: 'X steps/mm',
    101: 'Y steps/mm',
    102: 'Z steps/mm',
    103: 'C steps/deg',
    110: 'X max rate, mm/min',
    111: 'Y max rate, mm/min',
    112: 'Z max rate, mm/min',
    113: 'C max rate, deg/min',
    120: 'Acceleration, mm/s^2',
    130: 'Junction deviation, mm',
    140: 'X max travel, mm',
    141: 'Y max travel, mm',
    142: 'Z max travel, mm',
    150: 'Homing seek rate, mm/min',
    151: 'Homing feed rate, mm/min',
    152: 'Homing pull-off, mm',
    160: 'Homing method (0=sensored, 1=sensorless)',
    161: 'StallGuard threshold (0-255)',
    162: 'TMC Run Current (mA)',
    163: 'TMC Hold Current (mA)',
    164: 'TMC Microstepping',
    165: 'TMC StealthChop Mode (0=spreadCycle, 1=stealthChop)',
    170: 'Servo up angle, deg',
    171: 'Servo down angle, deg',
    172: 'Servo delay, ms',
    173: 'Blade pressure, 0-255',
    174: 'Tool type (0=servo, 1=solenoid, 2=tangential)',
    180: 'Invert X direction',
    181: 'Invert Y direction',
    182: 'Invert Z direction',
    183: 'Invert C direction',
    190: 'WiFi mode (0=AP, 1=STA)'
  };
  return defaults[num] || `Setting $${num}`;
}

function renderSettingsForm() {
  dom.settingsLoading.style.display = 'none';
  dom.settingsList.style.display = 'flex';
  dom.settingsActions.style.display = 'grid';
  
  // Clear old list
  dom.settingsList.innerHTML = '';
  
  // Sort settings by key
  const keys = Object.keys(state.settings).map(Number).sort((a, b) => a - b);
  
  keys.forEach(key => {
    const setting = state.settings[key];
    
    const row = document.createElement('div');
    row.className = 'form-group';
    row.style.marginBottom = '10px';
    row.style.borderBottom = '1px solid rgba(255,255,255,0.03)';
    row.style.paddingBottom = '8px';
    
    const label = document.createElement('label');
    label.innerText = `$${key} — ${setting.desc}`;
    label.style.fontWeight = '500';
    
    const input = document.createElement('input');
    input.type = 'number';
    input.value = setting.val;
    input.step = key >= 100 && key <= 103 ? '0.001' : '1';
    input.dataset.settingNum = key;
    
    row.appendChild(label);
    row.appendChild(input);
    dom.settingsList.appendChild(row);
  });
}

function setupSettingsHandlers() {
  dom.btnSettingsLoad.addEventListener('click', () => {
    state.settings = {};
    dom.settingsList.innerHTML = '';
    dom.settingsLoading.innerText = 'Loading settings from machine...';
    state.connection.sendLine('$$');
  });
  
  // Save settings trigger
  dom.btnSettingsSave.addEventListener('click', () => {
    const inputs = dom.settingsList.querySelectorAll('input');
    let commands = [];
    
    inputs.forEach(input => {
      const num = parseInt(input.dataset.settingNum);
      const oldVal = state.settings[num].val;
      const newVal = parseFloat(input.value);
      
      if (oldVal !== newVal) {
        commands.push(`$${num}=${newVal}`);
        state.settings[num].val = newVal; // update locally
      }
    });
    
    if (commands.length === 0) {
      alert('No settings were modified.');
      return;
    }
    
    logToTerminal(`Saving ${commands.length} settings to EEPROM...`, 'info');
    commands.forEach(cmd => {
      state.connection.sendLine(cmd);
    });
    
    // Save to EEPROM
    state.connection.sendLine('M500');
    alert('Settings updated on machine.');
  });
  
  dom.btnSettingsReset.addEventListener('click', () => {
    if (confirm('Are you sure you want to restore factory default settings?')) {
      logToTerminal('Restoring factory defaults ($RST=*)...', 'info');
      state.connection.sendLine('$RST=*');
      // Reload setting values
      setTimeout(() => {
        state.settings = {};
        state.connection.sendLine('$$');
      }, 1000);
    }
  });
}

// --- App Bootstrap ---
function init() {
  setupConnectionHandlers();
  registerConnectionCallbacks();
  setupFileImporter();
  setupCanvasInteractions();
  setupParamHandlers();
  setupCutterHandlers();
  setupTabHandlers();
  setupConsoleHandlers();
  setupSettingsHandlers();
  
  // Initial draw of empty bed
  drawCanvas();
}

// Run init
init();

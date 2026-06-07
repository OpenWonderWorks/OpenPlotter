import './style.css';
import { parseSVG } from './lib/svg-parser.js';
import { generateGcode, getToolVerb, getToolAction, getToolColorClass, getToolPressureLabel, getToolSpeedLabel, getWorkflowOptions, getDualHeadLabel, estimateJobTime, formatTime } from './lib/gcode-generator.js';
import { serial } from './lib/serial.js';
import { websocket } from './lib/websocket.js';
import { logManager, LOG_CATEGORY } from './lib/log-manager.js';
import { traceImage, renderTracePreview } from './lib/image-tracer.js';

// ══════════════════════════════════════════════════════════════
// Global Error Handler
// ══════════════════════════════════════════════════════════════
window.onerror = function(msg, url, line, col, error) {
  logManager.error(`Uncaught: ${msg} at ${url}:${line}:${col}`, error);
};

// ══════════════════════════════════════════════════════════════
// Toast Notifications
// ══════════════════════════════════════════════════════════════
const toastContainer = (() => {
  let c = document.getElementById('toast-container');
  if (!c) {
    c = document.createElement('div');
    c.id = 'toast-container';
    document.body.appendChild(c);
  }
  return c;
})();

function showToast(message, type = 'info', durationMs = 4000) {
  const toast = document.createElement('div');
  toast.className = `toast toast-${type}`;
  const icons = { info: 'ℹ', success: '✓', warning: '⚠', error: '✕' };
  toast.innerHTML = `
    <span class="toast-icon">${icons[type] || icons.info}</span>
    <span class="toast-message">${message}</span>
    <button class="toast-close" onclick="this.parentElement.remove()">×</button>
  `;
  toastContainer.appendChild(toast);
  requestAnimationFrame(() => toast.classList.add('toast-visible'));
  if (durationMs > 0) {
    setTimeout(() => {
      toast.classList.remove('toast-visible');
      toast.addEventListener('transitionend', () => toast.remove(), { once: true });
      setTimeout(() => { if (toast.parentNode) toast.remove(); }, 400);
    }, durationMs);
  }
  return toast;
}

// ══════════════════════════════════════════════════════════════
// Application State
// ══════════════════════════════════════════════════════════════
const state = {
  connectionMode: 'usb',
  connection: null,
  connected: false,
  svgData: null,
  gcode: '',
  currentPosition: { x: 0, y: 0, z: 0 },
  machineState: 'DISCONNECTED',
  activeTab: 'tab-control',
  
  // Canvas
  zoom: 1.5,
  panX: 50,
  panY: 50,
  isDragging: false,
  lastMouseX: 0,
  lastMouseY: 0,
  bedSizeX: 300,
  bedSizeY: 300,
  
  // Settings & Profiles
  settings: {},
  profileSettings: {
    vinyl: { pressure: 90, speed: 1500, passes: 1 },
    cardstock: { pressure: 220, speed: 800, passes: 2 },
    paper: { pressure: 60, speed: 1800, passes: 1 },
    custom: { pressure: 120, speed: 1500, passes: 1 }
  },
  currentProfile: 'vinyl',
  
  // Tool state
  toolDown: false,
  selectedWorkflow: 'head1-then-2',
  
  // Image trace state
  traceFile: null,
  traceCanvas: null
};

// ══════════════════════════════════════════════════════════════
// DOM Cache
// ══════════════════════════════════════════════════════════════
const dom = {
  statusBadge: document.getElementById('status-badge'),
  btnShowConnect: document.getElementById('btn-show-connect'),
  btnDisconnect: document.getElementById('btn-disconnect'),
  
  // Modal
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
  
  // Trace settings
  traceSettings: document.getElementById('trace-settings'),
  traceBrightness: document.getElementById('trace-brightness'),
  traceContrast: document.getElementById('trace-contrast'),
  traceThreshold: document.getElementById('trace-threshold'),
  traceSmoothing: document.getElementById('trace-smoothing'),
  traceSimplify: document.getElementById('trace-simplify'),
  traceMinLength: document.getElementById('trace-min-length'),
  traceInvert: document.getElementById('trace-invert'),
  btnRetrace: document.getElementById('btn-retrace'),
  tracePreview: document.getElementById('trace-preview'),
  
  // Plotter Config
  cfgKinematics: document.getElementById('cfg-kinematics'),
  cfgHeads: document.getElementById('cfg-heads'),
  head2Offsets: document.getElementById('head2-offsets'),
  cfgHead2X: document.getElementById('cfg-head2-x'),
  cfgHead2Y: document.getElementById('cfg-head2-y'),
  cfgToolHead1: document.getElementById('cfg-tool-head1'),
  groupToolHead2: document.getElementById('group-tool-head2'),
  cfgToolHead2: document.getElementById('cfg-tool-head2'),
  workflowSelector: document.getElementById('workflow-selector'),
  workflowCards: document.getElementById('workflow-cards'),
  cfgBedX: document.getElementById('cfg-bed-x'),
  cfgBedY: document.getElementById('cfg-bed-y'),
  
  // Ports
  flashPort: document.getElementById('flash-port'),
  btnDetectPorts: document.getElementById('btn-detect-ports'),
  connUsbPort: document.getElementById('conn-usb-port'),
  btnConnDetect: document.getElementById('btn-conn-detect'),
  
  // Configs
  cfgScale: document.getElementById('cfg-scale'),
  cfgTolerance: document.getElementById('cfg-tolerance'),
  cfgOffsetX: document.getElementById('cfg-offset-x'),
  cfgOffsetY: document.getElementById('cfg-offset-y'),
  
  // Material/Cutting
  panelMaterials: document.getElementById('panel-materials'),
  profileCards: document.querySelectorAll('.profile-card'),
  cfgPressure: document.getElementById('cfg-pressure'),
  cfgSpeedLinear: document.getElementById('cfg-speed-linear'),
  cfgSpeedRapid: document.getElementById('cfg-speed-rapid'),
  cfgPasses: document.getElementById('cfg-passes'),
  cfgUpDownDelays: document.getElementById('cfg-updown-delays'),
  
  // Dynamic labels
  labelPressure: document.getElementById('label-pressure'),
  labelSpeed: document.getElementById('label-speed'),
  labelStartBtn: document.getElementById('label-start'),
  labelGenGcode: document.getElementById('label-gen-gcode'),
  labelToolTest: document.getElementById('label-tool-test'),
  paramsTitle: document.getElementById('params-title'),
  materialTitle: document.getElementById('panel-materials-title'),
  
  // Canvas Actions
  btnCanvasFit: document.getElementById('btn-canvas-fit'),
  btnCanvasClear: document.getElementById('btn-canvas-clear'),
  btnGenGcode: document.getElementById('btn-gen-gcode'),
  btnStartCut: document.getElementById('btn-start-cut'),
  btnPauseCut: document.getElementById('btn-pause-cut'),
  btnStopCut: document.getElementById('btn-stop-cut'),
  btnExportGcode: document.getElementById('btn-export-gcode'),
  
  canvas: document.getElementById('plotter-canvas'),
  canvasWrapper: document.getElementById('canvas-wrapper'),
  
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
  
  // Quick utils
  btnCmdStatus: document.getElementById('btn-cmd-status'),
  btnCmdEndstops: document.getElementById('btn-cmd-endstops'),
  btnCmdHelp: document.getElementById('btn-cmd-help'),
  btnCmdMotorsOff: document.getElementById('btn-cmd-motors-off'),
  
  // Console
  terminalLog: document.getElementById('terminal-log'),
  terminalInput: document.getElementById('terminal-input'),
  btnTerminalSend: document.getElementById('btn-terminal-send'),
  btnTerminalClear: document.getElementById('btn-terminal-clear'),
  
  // Settings
  settingsLoading: document.getElementById('settings-loading'),
  btnSettingsLoad: document.getElementById('btn-settings-load'),
  settingsList: document.getElementById('settings-list'),
  settingsActions: document.getElementById('settings-actions'),
  btnSettingsSave: document.getElementById('btn-settings-save'),
  btnSettingsReset: document.getElementById('btn-settings-reset'),
  
  // Progress
  progressText: document.getElementById('progress-text'),
  progressFill: document.getElementById('progress-fill'),
  progressStats: document.getElementById('progress-stats'),
  coordX: document.getElementById('coord-x'),
  coordY: document.getElementById('coord-y'),
  coordZ: document.getElementById('coord-z'),
  
  // Log panel
  logPanel: document.getElementById('log-panel'),
  logHeader: document.getElementById('log-header'),
  logContent: document.getElementById('log-content'),
  logLineCount: document.getElementById('log-line-count'),
  logSearch: document.getElementById('log-search'),
  logExport: document.getElementById('log-export'),
  logClear: document.getElementById('log-clear'),
  systemLogsContainer: document.getElementById('system-logs-container'),
  btnClearLogs: document.getElementById('btn-clear-logs')
};

const ctx = dom.canvas.getContext('2d');

// ══════════════════════════════════════════════════════════════
// Initialize Log Manager
// ══════════════════════════════════════════════════════════════
function setupLogPanel() {
  logManager.bind(dom.logPanel, dom.logContent, dom.logLineCount, dom.systemLogsContainer);
  
  if (dom.btnClearLogs) {
    dom.btnClearLogs.addEventListener('click', () => {
      logManager.clear();
    });
  }
  
  // Log panel collapse/expand
  dom.logHeader.addEventListener('click', (e) => {
    if (e.target.closest('.log-controls')) return; // Don't toggle when clicking controls
    dom.logPanel.classList.toggle('collapsed');
  });

  // Setup UI tabs
  document.querySelectorAll('.tab-header').forEach(header => {
    header.addEventListener('click', () => {
      // Deactivate all
      document.querySelectorAll('.tab-header').forEach(h => h.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
      
      // Activate clicked
      header.classList.add('active');
      const targetId = header.getAttribute('data-tab');
      if (targetId) {
        const targetContent = document.getElementById(targetId);
        if (targetContent) targetContent.classList.add('active');
      }
    });
  });
  
  // Category filter buttons
  document.querySelectorAll('.log-filter-btn').forEach(btn => {
    const cat = btn.dataset.category;
    logManager._filterBtns[cat] = btn;
    btn.addEventListener('click', (e) => {
      e.stopPropagation();
      logManager.toggleCategory(cat);
    });
  });
  
  // Search
  if (dom.logSearch) {
    dom.logSearch.addEventListener('input', (e) => {
      e.stopPropagation();
      logManager.setSearch(e.target.value);
    });
    dom.logSearch.addEventListener('click', (e) => e.stopPropagation());
  }
  
  // Export / Clear
  if (dom.logExport) dom.logExport.addEventListener('click', (e) => { e.stopPropagation(); logManager.exportToFile(); });
  if (dom.logClear) dom.logClear.addEventListener('click', (e) => { e.stopPropagation(); logManager.clear(); });
  
  logManager.system('OpenPlotter v3.1.0 initialized');
}

// ══════════════════════════════════════════════════════════════
// Canvas Sizing
// ══════════════════════════════════════════════════════════════
function resizeCanvas() {
  if (!dom.canvasWrapper) return;
  const w = dom.canvasWrapper.clientWidth;
  const h = dom.canvasWrapper.clientHeight;
  const size = Math.min(w, h, 600) - 16;
  if (size > 0) {
    dom.canvas.width = size;
    dom.canvas.height = size;
  }
  drawCanvas();
}
window.addEventListener('resize', resizeCanvas);
setTimeout(resizeCanvas, 100);

// Default connection
state.connection = serial;

// ══════════════════════════════════════════════════════════════
// Tool-Aware UI Updates
// ══════════════════════════════════════════════════════════════
function updateToolLabels() {
  const heads = dom.cfgHeads ? parseInt(dom.cfgHeads.value) : 1;
  const tool1 = dom.cfgToolHead1 ? dom.cfgToolHead1.value : 'dragknife';
  const tool2 = dom.cfgToolHead2 ? dom.cfgToolHead2.value : 'pen';
  const isPen = tool1 === 'pen';
  
  const verb = getToolVerb(tool1);
  const action = getToolAction(tool1);
  
  // Update button labels
  if (dom.labelStartBtn) {
    if (heads > 1) {
      dom.labelStartBtn.textContent = getDualHeadLabel(tool1, tool2, state.selectedWorkflow);
    } else {
      dom.labelStartBtn.textContent = `Start ${action}`;
    }
  }
  
  if (dom.labelGenGcode) {
    dom.labelGenGcode.textContent = heads > 1
      ? 'Generate G-code'
      : `Generate ${verb} Path`;
  }
  
  // Update parameter labels
  if (dom.labelPressure) dom.labelPressure.textContent = getToolPressureLabel(tool1);
  if (dom.labelSpeed) dom.labelSpeed.textContent = getToolSpeedLabel(tool1);
  if (dom.paramsTitle) dom.paramsTitle.textContent = `⚙ ${verb} Parameters`;
  if (dom.labelToolTest) dom.labelToolTest.textContent = `Test ${verb}`;
  
  // Show/hide material profiles (pens don't need material profiles)
  if (dom.panelMaterials) {
    dom.panelMaterials.style.display = isPen ? 'none' : 'block';
  }
  
  // Update start button color class
  const colorClass = getToolColorClass(tool1);
  dom.btnStartCut.className = dom.btnStartCut.className
    .replace(/btn-tool-\w+/g, '')
    .replace(/btn-primary/g, '')
    .trim();
  dom.btnStartCut.classList.add(`btn-tool-${colorClass}`);
  
  // Workflow selector for dual heads
  if (heads > 1) {
    updateWorkflowSelector(tool1, tool2);
  }
}

function updateWorkflowSelector(tool1, tool2) {
  if (!dom.workflowCards) return;
  
  const options = getWorkflowOptions(tool1, tool2);
  dom.workflowCards.innerHTML = '';
  
  options.forEach(opt => {
    const btn = document.createElement('button');
    btn.className = `workflow-card${opt.id === state.selectedWorkflow ? ' active' : ''}`;
    btn.innerHTML = `<span class="workflow-card-icon">${opt.icon}</span><span>${opt.label}</span>`;
    btn.addEventListener('click', () => {
      state.selectedWorkflow = opt.id;
      dom.workflowCards.querySelectorAll('.workflow-card').forEach(c => c.classList.remove('active'));
      btn.classList.add('active');
      updateToolLabels();
    });
    dom.workflowCards.appendChild(btn);
  });
}

// ══════════════════════════════════════════════════════════════
// Connection Handlers
// ══════════════════════════════════════════════════════════════
function setupConnectionHandlers() {
  dom.btnShowConnect.addEventListener('click', () => dom.modalConnect.classList.add('open'));
  dom.btnCloseConnect.addEventListener('click', () => dom.modalConnect.classList.remove('open'));
  dom.btnModalCancel.addEventListener('click', () => dom.modalConnect.classList.remove('open'));
  
  dom.connModeRadio.forEach(radio => {
    radio.addEventListener('change', (e) => {
      state.connectionMode = e.target.value;
      if (state.connectionMode === 'usb') {
        if (dom.connParamsUsb) dom.connParamsUsb.style.display = 'block';
        if (dom.connParamsWifi) dom.connParamsWifi.style.display = 'none';
        state.connection = serial;
      } else {
        if (dom.connParamsUsb) dom.connParamsUsb.style.display = 'none';
        if (dom.connParamsWifi) dom.connParamsWifi.style.display = 'block';
        state.connection = websocket;
      }
    });
  });

  const detectPorts = async () => {
    if (!window.electronAPI) return;
    try {
      const ports = await window.electronAPI.detectBoards();
      [dom.flashPort, dom.connUsbPort].forEach(select => {
        if (!select) return;
        select.innerHTML = '<option value="">Select a Port...</option>';
        ports.forEach(p => {
          const opt = document.createElement('option');
          opt.value = p.path;
          opt.innerText = `${p.path}${p.isArduino ? ` — ${p.hint}` : ''}`;
          if (p.isArduino) opt.selected = true;
          select.appendChild(opt);
        });
      });
      logManager.system(`Port scan: found ${ports.length} port(s)`);
    } catch (err) {
      logManager.error('Port detection failed', err);
    }
  };

  if (dom.btnDetectPorts) dom.btnDetectPorts.addEventListener('click', detectPorts);
  if (dom.btnConnDetect) dom.btnConnDetect.addEventListener('click', detectPorts);
  detectPorts();
  
  dom.btnModalConnect.addEventListener('click', async () => {
    dom.btnModalConnect.disabled = true;
    dom.btnModalConnect.innerText = 'Connecting...';
    
    try {
      if (state.connectionMode === 'usb') {
        const baud = parseInt(dom.connUsbBaud.value);
        const portPath = dom.connUsbPort ? dom.connUsbPort.value : '';
        logManager.system(`Connecting via USB (baud: ${baud})...`);
        await serial.connect(baud, portPath);
      } else {
        const ip = dom.connWifiIp.value.trim();
        logManager.system(`Connecting via WebSocket to ${ip}...`);
        await websocket.connect(ip);
      }
      dom.modalConnect.classList.remove('open');
    } catch (err) {
      showToast(`Connection failed: ${err.message || err}`, 'error', 6000);
      logManager.error('Connection failed', err);
    } finally {
      dom.btnModalConnect.disabled = false;
      dom.btnModalConnect.innerText = 'Connect';
    }
  });
  
  dom.btnDisconnect.addEventListener('click', async () => {
    logManager.system('Disconnecting...');
    await state.connection.disconnect();
  });
}

function registerConnectionCallbacks() {
  [serial, websocket].forEach(conn => {
    conn.onConnect = () => {
      state.connected = true;
      dom.btnShowConnect.style.display = 'none';
      dom.btnDisconnect.style.display = 'inline-flex';
      dom.terminalInput.disabled = false;
      dom.btnTerminalSend.disabled = false;
      if (dom.btnSettingsLoad) dom.btnSettingsLoad.disabled = false;
      
      logManager.system('Machine connected');
      logToTerminal('Connected.', 'info');
      showToast('Machine connected', 'success', 3000);
      updateStartCutStatus();
      
      setTimeout(() => {
        conn.sendRealtimeCharacter('?');
        conn.sendLine('$$');
      }, 500);
    };

    conn.onError = (context, err) => {
      showToast(`${context}: ${err.message || err}`, 'error', 5000);
      logManager.error(`${context}: ${err.message || err}`);
    };
    
    conn.onDisconnect = () => {
      state.connected = false;
      dom.btnShowConnect.style.display = 'inline-flex';
      dom.btnDisconnect.style.display = 'none';
      dom.terminalInput.disabled = true;
      dom.btnTerminalSend.disabled = true;
      if (dom.btnSettingsLoad) dom.btnSettingsLoad.disabled = true;
      
      state.machineState = 'DISCONNECTED';
      updateStatusBadge();
      logManager.system('Machine disconnected');
      logToTerminal('Disconnected.', 'info');
      updateStartCutStatus();
    };
    
    conn.onLineReceived = (line) => {
      logToTerminal(line, 'rx');
      logManager.serial(line, 'rx');
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
      const verb = getToolAction(dom.cfgToolHead1 ? dom.cfgToolHead1.value : 'dragknife');
      dom.progressFill.style.width = `${percent}%`;
      dom.progressText.innerText = `${verb}: ${percent}%`;
      dom.progressStats.innerText = `(${index}/${total})`;
      
      if (percent === 100) {
        logManager.gcode('Job completed successfully!');
        logToTerminal('Job completed!', 'info');
        showToast('Job completed successfully!', 'success', 6000);
        dom.progressText.innerText = 'Complete';
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
  line.innerText = type === 'tx' ? `> ${text}` : type === 'rx' ? `< ${text}` : text;
  dom.terminalLog.appendChild(line);
  dom.terminalLog.scrollTop = dom.terminalLog.scrollHeight;
  while (dom.terminalLog.childNodes.length > 200) {
    dom.terminalLog.removeChild(dom.terminalLog.firstChild);
  }
}

// ══════════════════════════════════════════════════════════════
// File Import (SVG + PNG/JPG)
// ══════════════════════════════════════════════════════════════
function setupFileImporter() {
  ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(ev => {
    dom.dropzone.addEventListener(ev, e => e.preventDefault(), false);
  });
  
  dom.dropzone.addEventListener('dragover', () => dom.dropzone.classList.add('drag-over'));
  dom.dropzone.addEventListener('dragleave', () => dom.dropzone.classList.remove('drag-over'));
  
  dom.dropzone.addEventListener('drop', (e) => {
    dom.dropzone.classList.remove('drag-over');
    const file = e.dataTransfer.files[0];
    if (file) handleFileImport(file);
  });
  
  dom.dropzone.addEventListener('click', () => dom.fileInput.click());
  dom.fileInput.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (file) handleFileImport(file);
  });
  
  // Re-trace button
  if (dom.btnRetrace) {
    dom.btnRetrace.addEventListener('click', () => {
      if (state.traceFile) handleRasterFile(state.traceFile);
    });
  }
}

function handleFileImport(file) {
  const ext = file.name.split('.').pop().toLowerCase();
  
  if (ext === 'svg') {
    handleSVGFile(file);
  } else if (['png', 'jpg', 'jpeg'].includes(ext)) {
    handleRasterFile(file);
  } else {
    showToast(`Unsupported file type: .${ext}`, 'warning');
  }
}

function handleSVGFile(file) {
  // Hide trace settings
  if (dom.traceSettings) dom.traceSettings.style.display = 'none';
  state.traceFile = null;
  
  const reader = new FileReader();
  reader.onload = (e) => {
    const svgText = e.target.result;
    const tolerance = parseFloat(dom.cfgTolerance.value);
    
    try {
      state.svgData = parseSVG(svgText, tolerance);
      
      dom.svgW.innerText = Math.round(state.svgData.width);
      dom.svgH.innerText = Math.round(state.svgData.height);
      dom.svgPaths.innerText = state.svgData.paths.length;
      dom.svgInfo.style.display = 'block';
      
      logManager.gcode(`Parsed SVG: ${state.svgData.paths.length} paths (${state.svgData.width}×${state.svgData.height})`);
      
      autoFitDesign();
      dom.btnGenGcode.disabled = false;
      drawCanvas();
    } catch (err) {
      showToast(`Failed to parse SVG: ${err.message}`, 'error', 6000);
      logManager.error('SVG parse failed', err);
    }
  };
  reader.readAsText(file);
}

async function handleRasterFile(file) {
  state.traceFile = file;
  if (dom.traceSettings) dom.traceSettings.style.display = 'block';
  
  const options = {
    brightness: parseInt(dom.traceBrightness?.value) || 0,
    contrast: parseInt(dom.traceContrast?.value) || 0,
    threshold: parseInt(dom.traceThreshold?.value) || 128,
    smoothing: parseInt(dom.traceSmoothing?.value) || 1,
    simplifyTolerance: parseFloat(dom.traceSimplify?.value) || 1.5,
    minPathLength: parseInt(dom.traceMinLength?.value) || 5,
    invert: dom.traceInvert?.checked || false,
    blur: true
  };
  
  try {
    logManager.system(`Tracing image: ${file.name}...`);
    const result = await traceImage(file, options);
    
    state.svgData = {
      paths: result.paths,
      width: result.width,
      height: result.height,
      viewBox: null
    };
    state.traceCanvas = result.originalCanvas;
    
    dom.svgW.innerText = Math.round(result.width);
    dom.svgH.innerText = Math.round(result.height);
    dom.svgPaths.innerText = result.paths.length;
    dom.svgInfo.style.display = 'block';
    
    // Render trace preview
    if (dom.tracePreview) {
      renderTracePreview(result.originalCanvas, result.paths, dom.tracePreview);
      dom.tracePreview.style.display = 'block';
    }
    
    logManager.gcode(`Traced image: ${result.paths.length} paths from ${file.name}`);
    showToast(`Traced ${result.paths.length} paths from image`, 'success', 3000);
    
    autoFitDesign();
    dom.btnGenGcode.disabled = false;
    drawCanvas();
  } catch (err) {
    showToast(`Failed to trace image: ${err.message}`, 'error', 6000);
    logManager.error('Image trace failed', err);
  }
}

function autoFitDesign() {
  if (!state.svgData) return;
  
  let minX = Infinity, maxX = -Infinity;
  let minY = Infinity, maxY = -Infinity;
  
  state.svgData.paths.forEach(p => {
    const pts = p.points || p;
    pts.forEach(pt => {
      if (pt.x < minX) minX = pt.x;
      if (pt.x > maxX) maxX = pt.x;
      if (pt.y < minY) minY = pt.y;
      if (pt.y > maxY) maxY = pt.y;
    });
  });
  
  if (minX === Infinity) return;
  
  const designWidth = maxX - minX;
  const designHeight = maxY - minY;
  const margin = 10;
  const targetW = state.bedSizeX - 2 * margin;
  const targetH = state.bedSizeY - 2 * margin;
  const scaleX = targetW / designWidth;
  const scaleY = targetH / designHeight;
  const finalScale = parseFloat(Math.min(scaleX, scaleY).toFixed(3));
  
  dom.cfgScale.value = finalScale;
  
  const scaledCenter = { x: minX + designWidth / 2, y: minY + designHeight / 2 };
  const bedCenter = { x: state.bedSizeX / 2, y: state.bedSizeY / 2 };
  
  dom.cfgOffsetX.value = Math.round(bedCenter.x - scaledCenter.x * finalScale);
  dom.cfgOffsetY.value = Math.round(bedCenter.y - scaledCenter.y * finalScale);
  
  logManager.gcode(`Auto-fit: scale=${finalScale}, offset=(${dom.cfgOffsetX.value}, ${dom.cfgOffsetY.value})`);
}

// ══════════════════════════════════════════════════════════════
// Canvas Drawing
// ══════════════════════════════════════════════════════════════
function drawCanvas() {
  if (!dom.canvas) return;
  
  ctx.clearRect(0, 0, dom.canvas.width, dom.canvas.height);
  ctx.save();
  ctx.translate(state.panX, state.panY);
  ctx.scale(state.zoom, state.zoom);
  
  // Bed background
  ctx.fillStyle = '#080a10';
  ctx.fillRect(0, 0, state.bedSizeX, state.bedSizeY);
  ctx.strokeStyle = 'rgba(108, 140, 255, 0.08)';
  ctx.lineWidth = 1;
  ctx.strokeRect(0, 0, state.bedSizeX, state.bedSizeY);
  
  // Grid
  for (let x = 10; x < state.bedSizeX; x += 10) {
    ctx.strokeStyle = (x % 50 === 0) ? 'rgba(108, 140, 255, 0.12)' : 'rgba(108, 140, 255, 0.04)';
    ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, state.bedSizeY); ctx.stroke();
  }
  for (let y = 10; y < state.bedSizeY; y += 10) {
    ctx.strokeStyle = (y % 50 === 0) ? 'rgba(108, 140, 255, 0.12)' : 'rgba(108, 140, 255, 0.04)';
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(state.bedSizeX, y); ctx.stroke();
  }
  
  // SVG paths
  if (state.svgData) {
    const scale = parseFloat(dom.cfgScale.value) || 1.0;
    const offX = parseFloat(dom.cfgOffsetX.value) || 0;
    const offY = parseFloat(dom.cfgOffsetY.value) || 0;
    
    ctx.strokeStyle = 'rgba(34, 211, 238, 0.75)';
    ctx.lineWidth = 1.5;
    
    state.svgData.paths.forEach(p => {
      const pts = p.points || p;
      if (pts.length === 0) return;
      
      // Use original path color if available, fallback to default cyan
      ctx.strokeStyle = p.color && p.color !== '#000000' && p.color !== 'none' 
        ? p.color 
        : 'rgba(34, 211, 238, 0.75)';
        
      ctx.beginPath();
      ctx.moveTo(pts[0].x * scale + offX, pts[0].y * scale + offY);
      for (let i = 1; i < pts.length; i++) {
        ctx.lineTo(pts[i].x * scale + offX, pts[i].y * scale + offY);
      }
      ctx.stroke();
    });
  }
  
  // Tool position crosshair
  const tx = state.currentPosition.x;
  const ty = state.currentPosition.y;
  
  ctx.strokeStyle = '#f59e42';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(tx - 8, ty); ctx.lineTo(tx + 8, ty);
  ctx.moveTo(tx, ty - 8); ctx.lineTo(tx, ty + 8);
  ctx.stroke();
  
  ctx.fillStyle = '#f59e42';
  ctx.beginPath();
  ctx.arc(tx, ty, 2.5, 0, 2 * Math.PI);
  ctx.fill();
  
  ctx.strokeStyle = 'rgba(245, 158, 66, 0.35)';
  ctx.lineWidth = 2.5;
  ctx.beginPath();
  ctx.arc(tx, ty, 5, 0, 2 * Math.PI);
  ctx.stroke();
  
  ctx.restore();
  drawRulers();
}

function drawRulers() {
  const z = state.zoom;
  const px = state.panX;
  const py = state.panY;
  
  ctx.save();
  ctx.font = '9px JetBrains Mono, monospace';
  ctx.fillStyle = 'rgba(255, 255, 255, 0.3)';
  ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
  ctx.lineWidth = 1;
  
  for (let mm = 0; mm <= state.bedSizeX; mm += 10) {
    const sx = px + mm * z;
    if (sx < 0 || sx > dom.canvas.width) continue;
    const isMajor = (mm % 50 === 0);
    ctx.beginPath(); ctx.moveTo(sx, 0); ctx.lineTo(sx, isMajor ? 10 : 5); ctx.stroke();
    if (isMajor) ctx.fillText(`${mm}`, sx + 2, 16);
  }
  
  for (let mm = 0; mm <= state.bedSizeY; mm += 10) {
    const sy = py + mm * z;
    if (sy < 0 || sy > dom.canvas.height) continue;
    const isMajor = (mm % 50 === 0);
    ctx.beginPath(); ctx.moveTo(0, sy); ctx.lineTo(isMajor ? 10 : 5, sy); ctx.stroke();
    if (isMajor) ctx.fillText(`${mm}`, 12, sy + 3);
  }
  
  ctx.restore();
}

function setupCanvasInteractions() {
  dom.canvas.addEventListener('mousedown', (e) => {
    state.isDragging = true;
    state.lastMouseX = e.clientX;
    state.lastMouseY = e.clientY;
  });
  
  window.addEventListener('mouseup', () => { state.isDragging = false; });
  
  dom.canvas.addEventListener('mousemove', (e) => {
    if (state.isDragging) {
      state.panX += e.clientX - state.lastMouseX;
      state.panY += e.clientY - state.lastMouseY;
      state.lastMouseX = e.clientX;
      state.lastMouseY = e.clientY;
      drawCanvas();
    }
  });
  
  dom.canvas.addEventListener('wheel', (e) => {
    e.preventDefault();
    const rect = dom.canvas.getBoundingClientRect();
    const mouseX = e.clientX - rect.left;
    const mouseY = e.clientY - rect.top;
    const bedX = (mouseX - state.panX) / state.zoom;
    const bedY = (mouseY - state.panY) / state.zoom;
    
    state.zoom *= e.deltaY < 0 ? 1.1 : 1 / 1.1;
    state.zoom = Math.max(0.5, Math.min(10, state.zoom));
    
    state.panX = mouseX - bedX * state.zoom;
    state.panY = mouseY - bedY * state.zoom;
    drawCanvas();
  }, { passive: false });
  
  dom.btnCanvasFit.addEventListener('click', () => {
    const scale = Math.min(dom.canvas.width / state.bedSizeX, dom.canvas.height / state.bedSizeY) * 0.9;
    state.zoom = scale;
    state.panX = (dom.canvas.width - state.bedSizeX * scale) / 2;
    state.panY = (dom.canvas.height - state.bedSizeY * scale) / 2;
    drawCanvas();
  });
  
  dom.btnCanvasClear.addEventListener('click', () => {
    state.svgData = null;
    state.gcode = '';
    state.traceFile = null;
    dom.svgInfo.style.display = 'none';
    if (dom.traceSettings) dom.traceSettings.style.display = 'none';
    dom.btnGenGcode.disabled = true;
    updateStartCutStatus();
    drawCanvas();
    logManager.system('Design cleared');
  });
}

// ══════════════════════════════════════════════════════════════
// Parameters & G-code Generation
// ══════════════════════════════════════════════════════════════
function setupParamHandlers() {
  [dom.cfgScale, dom.cfgTolerance, dom.cfgOffsetX, dom.cfgOffsetY].forEach(input => {
    if (input) input.addEventListener('change', drawCanvas);
  });
  
  // Bed size changes
  if (dom.cfgBedX) dom.cfgBedX.addEventListener('change', () => { state.bedSizeX = parseInt(dom.cfgBedX.value) || 300; drawCanvas(); });
  if (dom.cfgBedY) dom.cfgBedY.addEventListener('change', () => { state.bedSizeY = parseInt(dom.cfgBedY.value) || 300; drawCanvas(); });
  
  // Material profile cards
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
  
  [dom.cfgPressure, dom.cfgSpeedLinear, dom.cfgPasses].forEach(input => {
    if (input) input.addEventListener('input', () => {
      dom.profileCards.forEach(c => c.classList.remove('active'));
      document.querySelector('[data-profile="custom"]')?.classList.add('active');
      state.currentProfile = 'custom';
    });
  });
  
  // Tool head change → update labels
  if (dom.cfgToolHead1) dom.cfgToolHead1.addEventListener('change', updateToolLabels);
  if (dom.cfgToolHead2) dom.cfgToolHead2.addEventListener('change', updateToolLabels);
  
  // Generate G-code
  dom.btnGenGcode.addEventListener('click', () => {
    if (!state.svgData) return;
    
    const heads = dom.cfgHeads ? parseInt(dom.cfgHeads.value) : 1;
    
    state.gcode = generateGcode(state.svgData.paths, {
      feedRateLinear: parseInt(dom.cfgSpeedLinear.value) || 1500,
      feedRateRapid: parseInt(dom.cfgSpeedRapid.value) || 3000,
      bladePressure: parseInt(dom.cfgPressure.value) || 90,
      toolDownDelay: parseInt(dom.cfgUpDownDelays.value) || 150,
      toolUpDelay: parseInt(dom.cfgUpDownDelays.value) || 150,
      passCount: parseInt(dom.cfgPasses.value) || 1,
      bedSizeX: state.bedSizeX,
      bedSizeY: state.bedSizeY,
      scale: parseFloat(dom.cfgScale.value) || 1.0,
      offsetX: parseFloat(dom.cfgOffsetX.value) || 0,
      offsetY: parseFloat(dom.cfgOffsetY.value) || 0,
      invertY: true,
      svgSize: { w: state.svgData.width, h: state.svgData.height },
      kinematics: dom.cfgKinematics ? dom.cfgKinematics.value : 'cartesian',
      heads: heads,
      head2Offset: {
        x: dom.cfgHead2X ? parseFloat(dom.cfgHead2X.value) : 0,
        y: dom.cfgHead2Y ? parseFloat(dom.cfgHead2Y.value) : 0
      },
      toolHead1: dom.cfgToolHead1 ? dom.cfgToolHead1.value : 'dragknife',
      toolHead2: dom.cfgToolHead2 ? dom.cfgToolHead2.value : 'pen',
      workflow: state.selectedWorkflow
    });
    
    const lineCount = state.gcode.split('\n').length;
    const est = estimateJobTime(state.svgData.paths, {
      scale: parseFloat(dom.cfgScale.value) || 1.0,
      feedRateLinear: parseInt(dom.cfgSpeedLinear.value) || 1500,
      feedRateRapid: parseInt(dom.cfgSpeedRapid.value) || 3000,
      passCount: parseInt(dom.cfgPasses.value) || 1
    });
    
    logManager.gcode(`Generated ${lineCount} lines of G-code. Est. time: ~${formatTime(est)}`);
    showToast(`Generated ${lineCount} lines (~${formatTime(est)})`, 'success', 3000);
    updateStartCutStatus();
    updateGcodePreview();
  });
}

function updateStartCutStatus() {
  const isReady = state.connected && state.gcode.length > 0;
  dom.btnStartCut.disabled = !isReady;
}

// ══════════════════════════════════════════════════════════════
// Cut/Draw Control
// ══════════════════════════════════════════════════════════════
function setupCutterHandlers() {
  dom.btnStartCut.addEventListener('click', () => {
    if (!state.gcode) return;
    const verb = getToolAction(dom.cfgToolHead1 ? dom.cfgToolHead1.value : 'dragknife').toLowerCase();
    if (!confirm(`Start ${verb}? Ensure material is loaded and tool is ready.`)) return;
    
    const lines = state.gcode.split('\n');
    logManager.gcode(`Starting job: ${lines.length} lines`);
    state.connection.startSending(lines);
    
    dom.btnStartCut.style.display = 'none';
    dom.btnPauseCut.style.display = 'inline-flex';
    dom.btnPauseCut.innerText = 'Pause';
    dom.btnStopCut.style.display = 'inline-flex';
    dom.canvas.classList.add('cutting-active');
    setInputsDisabled(true);
  });
  
  dom.btnPauseCut.addEventListener('click', () => {
    if (state.connection.paused) {
      logManager.gcode('Resuming job...');
      state.connection.resumeSending();
      dom.btnPauseCut.innerText = 'Pause';
    } else {
      logManager.gcode('Pausing job (feed hold)');
      state.connection.pauseSending();
      dom.btnPauseCut.innerText = 'Resume';
    }
  });
  
  dom.btnStopCut.addEventListener('click', () => {
    logManager.gcode('Aborting job!');
    state.connection.stopSending();
    resetCutButtons();
    state.connection.sendRealtimeCharacter(String.fromCharCode(24));
  });
  
  dom.btnEstop.addEventListener('click', () => {
    logManager.error('EMERGENCY STOP');
    state.connection.sendRealtimeCharacter('!');
    state.connection.sendRealtimeCharacter(String.fromCharCode(24));
    if (state.connection.sending) state.connection.stopSending();
    resetCutButtons();
  });
  
  dom.btnUnlock.addEventListener('click', () => {
    logManager.system('Unlock ($X)');
    state.connection.sendLine('$X');
  });
  
  dom.btnHome.addEventListener('click', () => {
    logManager.system('Homing ($H)');
    state.connection.sendLine('$H');
  });
  
  dom.btnZeroXY.addEventListener('click', () => {
    logManager.system('Zero work pos (G92 X0 Y0)');
    state.connection.sendLine('G92 X0 Y0');
  });
  
  dom.btnToolTest.addEventListener('click', () => {
    const pressure = parseInt(dom.cfgPressure.value) || 90;
    const verb = getToolVerb(dom.cfgToolHead1 ? dom.cfgToolHead1.value : 'dragknife');
    if (state.toolDown) {
      logManager.system(`Test: ${verb} tool UP (M5)`);
      state.connection.sendLine('M5');
      state.toolDown = false;
    } else {
      logManager.system(`Test: ${verb} tool DOWN (M3 S${pressure})`);
      state.connection.sendLine(`M3 S${pressure}`);
      state.toolDown = true;
    }
  });
  
  // Jog
  dom.jogXMinus.addEventListener('click', () => sendJogMove(-1, 0));
  dom.jogXPlus.addEventListener('click', () => sendJogMove(1, 0));
  dom.jogYMinus.addEventListener('click', () => sendJogMove(0, -1));
  dom.jogYPlus.addEventListener('click', () => sendJogMove(0, 1));
  
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
  logManager.serial(`Jog: G91 G0 X${dx} Y${dy} F${speed}`, 'tx');
  state.connection.sendLine(`G21 G91 G0 X${dx} Y${dy} F${speed} G90`);
}

function resetCutButtons() {
  dom.btnStartCut.style.display = 'inline-flex';
  dom.btnPauseCut.style.display = 'none';
  dom.btnStopCut.style.display = 'none';
  dom.canvas.classList.remove('cutting-active');
  setInputsDisabled(false);
}

function setInputsDisabled(disabled) {
  const els = [dom.dropzone, dom.cfgScale, dom.cfgTolerance, dom.cfgOffsetX, dom.cfgOffsetY,
    dom.cfgPressure, dom.cfgSpeedLinear, dom.cfgSpeedRapid, dom.cfgPasses, dom.cfgUpDownDelays];
  els.forEach(el => {
    if (el) {
      if (el === dom.dropzone) { el.style.pointerEvents = disabled ? 'none' : 'auto'; }
      else { el.disabled = disabled; }
    }
  });
  if (dom.fileInput) dom.fileInput.disabled = disabled;
}

// ══════════════════════════════════════════════════════════════
// Tabs
// ══════════════════════════════════════════════════════════════
function setupTabHandlers() {
  dom.tabHeaders.forEach(header => {
    header.addEventListener('click', () => {
      const tabId = header.dataset.tab;
      state.activeTab = tabId;
      dom.tabHeaders.forEach(h => h.classList.remove('active'));
      header.classList.add('active');
      dom.tabContents.forEach(pane => {
        pane.classList.toggle('active', pane.id === tabId);
      });
    });
  });
}

// ══════════════════════════════════════════════════════════════
// Console
// ══════════════════════════════════════════════════════════════
function setupConsoleHandlers() {
  const sendInput = () => {
    const val = dom.terminalInput.value.trim();
    if (val) {
      logToTerminal(val, 'tx');
      logManager.serial(val, 'tx');
      state.connection.sendLine(val);
      dom.terminalInput.value = '';
    }
  };
  
  dom.btnTerminalSend.addEventListener('click', sendInput);
  dom.terminalInput.addEventListener('keydown', (e) => { if (e.key === 'Enter') sendInput(); });
  dom.btnTerminalClear.addEventListener('click', () => {
    dom.terminalLog.innerHTML = '<div class="terminal-line terminal-info">Console cleared.</div>';
  });
}

// ══════════════════════════════════════════════════════════════
// Settings ($$ System)
// ══════════════════════════════════════════════════════════════
function parseSettingsOutput(line) {
  const match = line.match(/^\$(\d+)=([\d\.-]+)(?:\s*\((.*)\))?/);
  if (match) {
    const num = parseInt(match[1]);
    const val = parseFloat(match[2]);
    const desc = match[3] || getSettingDescription(num);
    state.settings[num] = { val, desc };
    renderSettingsForm();
  }
}

function getSettingDescription(num) {
  const d = {
    100: 'X steps/mm', 101: 'Y steps/mm', 102: 'Z steps/mm', 103: 'C steps/deg',
    110: 'X max rate mm/min', 111: 'Y max rate mm/min', 112: 'Z max rate mm/min', 113: 'C max rate deg/min',
    120: 'Acceleration mm/s²', 130: 'Junction deviation mm',
    140: 'X max travel mm', 141: 'Y max travel mm', 142: 'Z max travel mm',
    150: 'Homing seek rate', 151: 'Homing feed rate', 152: 'Homing pull-off mm',
    160: 'Homing method', 161: 'StallGuard threshold', 162: 'TMC run current mA',
    163: 'TMC hold current mA', 164: 'TMC microsteps', 165: 'TMC mode',
    170: 'Servo up angle', 171: 'Servo down angle', 172: 'Servo delay ms',
    173: 'Blade pressure', 174: 'Tool type', 180: 'Invert X', 181: 'Invert Y',
    182: 'Invert Z', 183: 'Invert C', 190: 'WiFi mode'
  };
  return d[num] || `Setting $${num}`;
}

function renderSettingsForm() {
  if (dom.settingsLoading) dom.settingsLoading.style.display = 'none';
  if (dom.settingsList) dom.settingsList.style.display = 'flex';
  if (dom.settingsActions) dom.settingsActions.style.display = 'grid';
  
  dom.settingsList.innerHTML = '';
  const keys = Object.keys(state.settings).map(Number).sort((a, b) => a - b);
  
  keys.forEach(key => {
    const setting = state.settings[key];
    const row = document.createElement('div');
    row.className = 'form-group';
    row.style.marginBottom = '8px';
    row.style.borderBottom = '1px solid rgba(255,255,255,0.03)';
    row.style.paddingBottom = '6px';
    
    const label = document.createElement('label');
    label.innerText = `$${key} — ${setting.desc}`;
    
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
  if (dom.btnSettingsLoad) {
    dom.btnSettingsLoad.addEventListener('click', () => {
      state.settings = {};
      dom.settingsList.innerHTML = '';
      if (dom.settingsLoading) dom.settingsLoading.innerText = 'Loading...';
      logManager.system('Loading machine settings ($$)');
      state.connection.sendLine('$$');
    });
  }
  
  if (dom.btnSettingsSave) {
    dom.btnSettingsSave.addEventListener('click', () => {
      const inputs = dom.settingsList.querySelectorAll('input');
      let commands = [];
      inputs.forEach(input => {
        const num = parseInt(input.dataset.settingNum);
        const oldVal = state.settings[num].val;
        const newVal = parseFloat(input.value);
        if (oldVal !== newVal) {
          commands.push(`$${num}=${newVal}`);
          state.settings[num].val = newVal;
        }
      });
      
      if (commands.length === 0) { showToast('No changes to save.', 'info'); return; }
      
      logManager.system(`Saving ${commands.length} settings to EEPROM`);
      commands.forEach(cmd => state.connection.sendLine(cmd));
      state.connection.sendLine('M500');
      showToast(`Saved ${commands.length} settings`, 'success');
    });
  }
  
  if (dom.btnSettingsReset) {
    dom.btnSettingsReset.addEventListener('click', () => {
      if (confirm('Restore factory defaults?')) {
        logManager.system('Factory reset ($RST=*)');
        state.connection.sendLine('$RST=*');
        setTimeout(() => { state.settings = {}; state.connection.sendLine('$$'); }, 1000);
      }
    });
  }
}

// ══════════════════════════════════════════════════════════════
// Plotter Config (head count, tools, drivers)
// ══════════════════════════════════════════════════════════════
function setupWorkflowUI() {
  if (!dom.workflowCards) return;
  const heads = dom.cfgHeads ? parseInt(dom.cfgHeads.value) : 1;
  const tool1 = dom.cfgToolHead1 ? dom.cfgToolHead1.options[dom.cfgToolHead1.selectedIndex].text : 'Tool 1';
  const tool2 = dom.cfgToolHead2 ? dom.cfgToolHead2.options[dom.cfgToolHead2.selectedIndex].text : 'Tool 2';
  
  if (heads === 1) {
    dom.workflowCards.innerHTML = '';
    return;
  }
  
  const workflows = [
    { id: 'head1-then-2', label: `${tool1} → ${tool2}` },
    { id: 'head2-then-1', label: `${tool2} → ${tool1}` },
    { id: 'map-colors', label: `Map by Color/Layer` }
  ];
  
  dom.workflowCards.innerHTML = '';
  workflows.forEach(wf => {
    const card = document.createElement('div');
    card.className = `profile-card ${state.selectedWorkflow === wf.id ? 'active' : ''}`;
    card.dataset.workflow = wf.id;
    card.innerHTML = `<div class="profile-name" style="font-size: 0.75rem;">${wf.label}</div>`;
    card.addEventListener('click', () => {
      state.selectedWorkflow = wf.id;
      Array.from(dom.workflowCards.children).forEach(c => c.classList.remove('active'));
      card.classList.add('active');
    });
    dom.workflowCards.appendChild(card);
  });
}

function setupPlotterConfigHandlers() {
  if (dom.cfgHeads) {
    dom.cfgHeads.addEventListener('change', (e) => {
      const isDual = e.target.value === '2';
      if (dom.head2Offsets) dom.head2Offsets.style.display = isDual ? 'block' : 'none';
      if (dom.groupToolHead2) dom.groupToolHead2.style.display = isDual ? 'block' : 'none';
      if (dom.workflowSelector) dom.workflowSelector.style.display = isDual ? 'block' : 'none';
      updateToolLabels();
    });
  }
}

// ══════════════════════════════════════════════════════════════
// G-code Preview
// ══════════════════════════════════════════════════════════════
function updateGcodePreview() {
  const panel = document.getElementById('gcode-preview-panel');
  const content = document.getElementById('gcode-preview-content');
  const count = document.getElementById('gcode-preview-count');
  const toggle = document.getElementById('gcode-preview-toggle');
  
  if (!panel || !content) return;
  
  if (!state.gcode) { panel.style.display = 'none'; return; }
  
  const lines = state.gcode.split('\n');
  count.innerText = `(${lines.length} lines)`;
  
  let previewText;
  if (lines.length <= 20) {
    previewText = state.gcode;
  } else {
    previewText = lines.slice(0, 8).join('\n') + `\n\n  ... ${lines.length - 12} lines ...\n\n` + lines.slice(-4).join('\n');
  }
  
  content.textContent = previewText;
  panel.style.display = 'block';
  
  if (!toggle._bound) {
    toggle.addEventListener('click', () => panel.classList.toggle('expanded'));
    toggle._bound = true;
  }
}

// ══════════════════════════════════════════════════════════════
// G-code Export
// ══════════════════════════════════════════════════════════════
function setupGcodeExport() {
  if (dom.btnExportGcode) {
    dom.btnExportGcode.addEventListener('click', () => {
      if (!state.gcode) { showToast('No G-code generated.', 'warning'); return; }
      const blob = new Blob([state.gcode], { type: 'text/plain' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `openplotter_${new Date().toISOString().slice(0, 10)}.gcode`;
      a.click();
      URL.revokeObjectURL(url);
      logManager.gcode('Exported G-code file');
      showToast('G-code downloaded.', 'success');
    });
  }
}

// ══════════════════════════════════════════════════════════════
// Keyboard Shortcuts
// ══════════════════════════════════════════════════════════════
function setupKeyboardShortcuts() {
  document.addEventListener('keydown', (e) => {
    const tag = e.target.tagName.toLowerCase();
    if (tag === 'input' || tag === 'textarea' || tag === 'select') return;
    
    if (e.key === 'Escape') { e.preventDefault(); dom.btnEstop.click(); return; }
    if (e.key === ' ' && state.connection?.sending) { e.preventDefault(); dom.btnPauseCut.click(); return; }
    if (e.key === 'ArrowUp') { e.preventDefault(); sendJogMove(0, 1); return; }
    if (e.key === 'ArrowDown') { e.preventDefault(); sendJogMove(0, -1); return; }
    if (e.key === 'ArrowLeft') { e.preventDefault(); sendJogMove(-1, 0); return; }
    if (e.key === 'ArrowRight') { e.preventDefault(); sendJogMove(1, 0); return; }
    if (e.ctrlKey && e.key === 'o') { e.preventDefault(); dom.fileInput.click(); return; }
    if (e.ctrlKey && e.key === 'g') { e.preventDefault(); if (!dom.btnGenGcode.disabled) dom.btnGenGcode.click(); return; }
    if (e.ctrlKey && e.key === 'h') { e.preventDefault(); dom.btnHome.click(); return; }
  });
}

// Beforeunload guard
window.addEventListener('beforeunload', (e) => {
  if (state.connection?.sending) {
    e.preventDefault();
    e.returnValue = 'A job is in progress. Leave?';
  }
});

// ══════════════════════════════════════════════════════════════
// Firmware Flasher
// ══════════════════════════════════════════════════════════════
function setupFirmwareFlasher() {
  const boardSelect = document.getElementById('flash-board-type');
  const compileTerminal = document.getElementById('compile-terminal');
  const btnCompileFlash = document.getElementById('btn-compile-flash');
  const compileStatus = document.getElementById('compile-status');
  const btnInstallTools = document.getElementById('btn-install-tools');

  // Auto-detect ports
  const flashPortSelect = document.getElementById('flash-port');
  const btnDetect = document.getElementById('btn-detect-ports');

  if (btnDetect && flashPortSelect && window.electronAPI) {
    btnDetect.addEventListener('click', async () => {
      btnDetect.disabled = true;
      flashPortSelect.innerHTML = '<option value="">Scanning...</option>';
      try {
        const ports = await window.electronAPI.detectBoards();
        flashPortSelect.innerHTML = '<option value="">Select a Port...</option>';
        if (ports?.length > 0) {
          ports.forEach(p => {
            const opt = document.createElement('option');
            opt.value = p.path;
            opt.textContent = `${p.path}${p.hint ? ` — ${p.hint}` : ''}`;
            if (p.isArduino) opt.selected = true;
            flashPortSelect.appendChild(opt);
          });
        } else {
          flashPortSelect.innerHTML = '<option value="">No ports found</option>';
        }
      } catch (err) {
        flashPortSelect.innerHTML = '<option value="">Error scanning</option>';
        logManager.error('Port scan failed', err);
      } finally {
        btnDetect.disabled = false;
      }
    });
    btnDetect.click(); // Auto-scan on load
  }

  // Toolchain status check
  if (window.electronAPI?.checkToolchain) {
    (async () => {
      try {
        const status = await window.electronAPI.checkToolchain();
        ['avrdude', 'arduino-cli', 'esptool'].forEach(tool => {
          const el = document.getElementById(`tc-${tool}`);
          if (el && status[tool]) {
            el.textContent = 'Found';
            el.className = 'toolchain-badge toolchain-found';
          } else if (el) {
            el.textContent = 'Not Found';
            el.className = 'toolchain-badge toolchain-missing';
          }
        });
      } catch (e) {
        logManager.warn('Could not check toolchain status');
      }
    })();
  } else {
    // Not in Electron — mark all as N/A
    ['avrdude', 'arduino-cli', 'esptool'].forEach(tool => {
      const el = document.getElementById(`tc-${tool}`);
      if (el) { el.textContent = 'Browser Mode'; el.className = 'toolchain-badge toolchain-missing'; }
    });
  }

  // Install missing tools
  if (btnInstallTools) {
    btnInstallTools.addEventListener('click', async () => {
      if (!window.electronAPI?.installToolchain) {
        showToast('Auto-install only available in desktop app', 'warning');
        return;
      }
      btnInstallTools.disabled = true;
      btnInstallTools.textContent = 'Installing...';
      logManager.flash('Installing missing toolchains...');
      try {
        const result = await window.electronAPI.installToolchain();
        logManager.flash(`Toolchain install: ${result}`);
        showToast(result + ' Reloading...', 'success');
        setTimeout(() => window.location.reload(), 1500);
      } catch (err) {
        logManager.error('Toolchain install failed', err);
        showToast(`Install failed: ${err}`, 'error');
      } finally {
        btnInstallTools.disabled = false;
        btnInstallTools.textContent = 'Auto-Install Missing Tools';
      }
    });
  }



  // Flash Orchestrator Modal
  const btnOpenFlashModal = document.getElementById('btn-open-flash-modal');
  const modalFlash = document.getElementById('modal-flash');
  const btnCloseFlash = document.getElementById('btn-close-flash');
  const btnFlashCancel = document.getElementById('btn-flash-cancel');
  const btnFlashStart = document.getElementById('btn-flash-start');
  const flashTerminal = document.getElementById('flash-terminal');
  const flashConsoleStatus = document.getElementById('flash-console-status');
  const flashSteps = {
    init: document.querySelector('.flash-step[data-step="init"]'),
    config: document.querySelector('.flash-step[data-step="config"]'),
    libs: document.querySelector('.flash-step[data-step="libs"]'),
    compile: document.querySelector('.flash-step[data-step="compile"]'),
    upload: document.querySelector('.flash-step[data-step="upload"]')
  };

  const updateFlashStep = (stepName, status) => {
    const el = flashSteps[stepName];
    if (!el) return;
    el.className = `flash-step ${status}`; // status: pending, active, done, error
  };

  const writeFlashTerminal = (text, type = 'info') => {
    const span = document.createElement('span');
    span.className = type;
    span.textContent = text;
    flashTerminal.appendChild(span);
    flashTerminal.scrollTop = flashTerminal.scrollHeight;
  };

  // Flash progress listener
  if (window.electronAPI?.onFlashProgress) {
    window.electronAPI.onFlashProgress((data) => {
      const text = data.trim();
      if (!text) return;

      logManager.flash(text);
      
      let type = 'info';
      if (text.startsWith('✕') || text.includes('Error')) type = 'error';
      else if (text.startsWith('✓')) type = 'success';
      else if (text.startsWith('>')) type = 'cmd';

      writeFlashTerminal(text + '\n', type);

      // Step transitions
      if (text.includes('Injecting configuration')) {
        updateFlashStep('init', 'done');
        updateFlashStep('config', 'active');
      } else if (text.includes('Checking/Installing Libraries')) {
        updateFlashStep('config', 'done');
        updateFlashStep('libs', 'active');
      } else if (text.includes('── Compiling ──')) {
        updateFlashStep('config', 'done'); // in case libs was skipped
        updateFlashStep('libs', 'done');
        updateFlashStep('compile', 'active');
      } else if (text.includes('── Uploading ──')) {
        updateFlashStep('compile', 'done');
        updateFlashStep('upload', 'active');
      }
    });
  }

  if (btnOpenFlashModal) {
    btnOpenFlashModal.addEventListener('click', () => {
      modalFlash.style.display = 'flex';
      flashTerminal.innerHTML = '';
      flashConsoleStatus.textContent = 'Ready';
      flashConsoleStatus.style.color = '#8b949e';
      Object.keys(flashSteps).forEach(k => updateFlashStep(k, 'pending'));
    });
  }

  const closeFlashModal = () => {
    if (btnFlashStart.disabled) {
      showToast('Wait for flash to complete before closing.', 'warning');
      return;
    }
    modalFlash.style.display = 'none';
  };

  if (btnCloseFlash) btnCloseFlash.addEventListener('click', closeFlashModal);
  if (btnFlashCancel) btnFlashCancel.addEventListener('click', closeFlashModal);

  // Compile & Flash
  if (btnFlashStart) {
    btnFlashStart.addEventListener('click', async () => {
      if (!window.electronAPI) {
        writeFlashTerminal('Error: Desktop app required.\n', 'error');
        return;
      }
      const portName = flashPortSelect?.value || '';
      if (!portName) {
        writeFlashTerminal('Error: Select a target port in the configuration tab.\n', 'error');
        return;
      }

      btnFlashStart.disabled = true;
      btnFlashCancel.disabled = true;
      btnCloseFlash.disabled = true;
      
      flashTerminal.innerHTML = '';
      flashConsoleStatus.textContent = 'Orchestrating...';
      flashConsoleStatus.style.color = '#a5d6ff';
      logManager.flash('Starting flash sequence...');

      if (state.connected && state.connection) {
        writeFlashTerminal('Disconnecting from active session...\n', 'info');
        await state.connection.disconnect();
        await new Promise(r => setTimeout(r, 500));
      }

      Object.keys(flashSteps).forEach(k => updateFlashStep(k, 'pending'));
      updateFlashStep('init', 'active');

      const config = {
        kinematics: dom.cfgKinematics?.value || 'cartesian',
        heads: dom.cfgHeads?.value || '1',
        tool1: dom.cfgToolHead1?.value || 'dragknife',
        driverType: document.getElementById('cfg-driver-type')?.value || 'tmc2209',
        motorCurrent: document.getElementById('cfg-motor-current')?.value || '800',
        invertX: document.getElementById('cfg-invert-x')?.checked || false,
        invertY: document.getElementById('cfg-invert-y')?.checked || false,
        stepsX: document.getElementById('cfg-steps-x')?.value || '80',
        stepsY: document.getElementById('cfg-steps-y')?.value || '80',
        maxSpeed: document.getElementById('cfg-max-speed')?.value || '5000',
        maxAccel: document.getElementById('cfg-max-accel')?.value || '500'
      };

      const advanced = {
        cleanBuild: document.getElementById('flash-opt-clean')?.checked || false,
        verifyUpload: document.getElementById('flash-opt-verify')?.checked || false,
        installLibraries: document.getElementById('flash-opt-libs')?.checked || false
      };

      try {
        const result = await window.electronAPI.compileAndFlash(boardSelect?.value || 'mega', portName, config, advanced);
        updateFlashStep('upload', 'done');
        flashConsoleStatus.textContent = 'Success';
        flashConsoleStatus.style.color = '#3fb950';
        writeFlashTerminal(`\n${result}\n`, 'success');
        logManager.flash(result);
        showToast('Firmware flashed successfully!', 'success');
      } catch (error) {
        flashConsoleStatus.textContent = 'Failed';
        flashConsoleStatus.style.color = '#f85149';
        writeFlashTerminal(`\nFatal Error: ${error}\n`, 'error');
        logManager.error('Flash failed', error);
        showToast('Firmware flash failed.', 'error');
        
        // Mark the active step as error
        for (const [key, el] of Object.entries(flashSteps)) {
          if (el && el.classList.contains('active')) {
            updateFlashStep(key, 'error');
            break;
          }
        }
      } finally {
        btnFlashStart.disabled = false;
        btnFlashCancel.disabled = false;
        btnCloseFlash.disabled = false;
      }
    });
  }


}

// ══════════════════════════════════════════════════════════════
// Bootstrap
// ══════════════════════════════════════════════════════════════
function init() {
  setupLogPanel();
  setupConnectionHandlers();
  registerConnectionCallbacks();
  setupFileImporter();
  setupCanvasInteractions();
  setupParamHandlers();
  setupCutterHandlers();
  setupTabHandlers();
  setupConsoleHandlers();
  setupSettingsHandlers();
  setupPlotterConfigHandlers();
  setupGcodeExport();
  setupKeyboardShortcuts();
  setupFirmwareFlasher();
  
  // Initial tool label update
  updateToolLabels();
  
  // Initial canvas draw
  drawCanvas();
  
  logManager.system('App ready');
}

init();

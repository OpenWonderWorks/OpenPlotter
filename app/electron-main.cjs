/**
 * OpenPlotter — Electron Main Process v3.1.0
 * Handles firmware flashing, toolchain auto-installation, port detection,
 * and native file dialogs.
 */

const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const path = require('path');
const { spawn, execSync, exec } = require('child_process');
const fs = require('fs');
const os = require('os');
const util = require('util');
const execAsync = util.promisify(exec);

const getFirmwareDir = () => app.isPackaged ? path.join(process.resourcesPath, 'OpenPlotter') : path.join(__dirname, '..', 'OpenPlotter');

let mainWindow;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    title: 'OpenPlotter',
    icon: path.join(__dirname, 'public', 'favicon.svg'),
    webPreferences: {
      preload: path.join(__dirname, 'electron-preload.cjs'),
      nodeIntegration: false,
      contextIsolation: true,
      sandbox: false,
    },
    autoHideMenuBar: true,
    backgroundColor: '#0b0d12',
  });

  // In development, load from Vite dev server
  if (process.env.VITE_DEV_SERVER_URL) {
    mainWindow.loadURL(process.env.VITE_DEV_SERVER_URL);
  } else {
    // Production
    mainWindow.loadFile(path.join(__dirname, 'dist', 'index.html'));
  }

  // Handle Web Serial API select-serial-port
  mainWindow.webContents.session.on('select-serial-port', (event, portList, webContents, callback) => {
    event.preventDefault();
    if (global.targetSerialPort) {
      const selectedPort = portList.find(port => port.portName === global.targetSerialPort || port.path === global.targetSerialPort);
      if (selectedPort) {
        callback(selectedPort.portId);
      } else {
        callback(''); // Port not found in list
      }
    } else {
      callback(''); // Cancel
    }
  });

  // Grant serial permissions automatically
  mainWindow.webContents.session.setPermissionCheckHandler((webContents, permission) => {
    if (permission === 'serial') return true;
    return false;
  });
  mainWindow.webContents.session.setDevicePermissionHandler((details) => {
    if (details.deviceType === 'serial') return true;
    return false;
  });
}

app.whenReady().then(createWindow);
app.on('window-all-closed', () => { if (process.platform !== 'darwin') app.quit(); });
app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createWindow(); });

// ══════════════════════════════════════════════════════════════
// Toolchain Paths & Auto-Detection
// ══════════════════════════════════════════════════════════════
const APP_DATA_DIR = path.join(app.getPath('userData'), 'toolchains');

// Common install locations per platform
function getToolSearchPaths(toolName) {
  const home = os.homedir();
  const isWin = process.platform === 'win32';
  const isMac = process.platform === 'darwin';

  const paths = [];

  if (toolName === 'avrdude') {
    if (isWin) {
      paths.push(
        path.join(home, 'AppData', 'Local', 'Arduino15', 'packages', 'arduino', 'tools', 'avrdude'),
        'C:\\Program Files (x86)\\Arduino\\hardware\\tools\\avr\\bin',
        'C:\\Program Files\\Arduino\\hardware\\tools\\avr\\bin',
        path.join(home, 'scoop', 'shims'),
        path.join(APP_DATA_DIR, 'avrdude'),
      );
    } else if (isMac) {
      paths.push(
        path.join(home, 'Library', 'Arduino15', 'packages', 'arduino', 'tools', 'avrdude'),
        '/usr/local/bin',
        '/opt/homebrew/bin',
        path.join(APP_DATA_DIR, 'avrdude'),
      );
    } else {
      paths.push('/usr/bin', '/usr/local/bin', path.join(APP_DATA_DIR, 'avrdude'));
    }
  }

  if (toolName === 'arduino-cli') {
    if (isWin) {
      paths.push(
        path.join(home, 'AppData', 'Local', 'Programs', 'arduino-cli'),
        path.join(home, 'scoop', 'shims'),
        path.join(APP_DATA_DIR, 'arduino-cli'),
      );
    } else {
      paths.push('/usr/local/bin', '/opt/homebrew/bin', path.join(APP_DATA_DIR, 'arduino-cli'));
    }
  }

  if (toolName === 'esptool') {
    if (isWin) {
      paths.push(
        path.join(home, 'AppData', 'Local', 'Programs', 'Python'),
        path.join(APP_DATA_DIR, 'esptool'),
      );
    } else {
      paths.push('/usr/local/bin', '/opt/homebrew/bin', path.join(home, '.local', 'bin'));
    }
  }

  return paths;
}

function findExecutable(name) {
  const isWin = process.platform === 'win32';
  const ext = isWin ? '.exe' : '';
  const fullName = name + ext;

  // Check PATH first
  try {
    const cmd = isWin ? `where ${name}` : `which ${name}`;
    const result = execSync(cmd, { encoding: 'utf-8', timeout: 3000 }).trim();
    if (result) return result.split('\n')[0].trim();
  } catch (e) {
    // not found in PATH
  }

  // Check known locations
  const searchPaths = getToolSearchPaths(name);
  for (const dir of searchPaths) {
    if (!fs.existsSync(dir)) continue;

    // Check directly
    const direct = path.join(dir, fullName);
    if (fs.existsSync(direct)) return direct;

    // Check subdirectories (Arduino15 has versioned folders)
    try {
      const entries = fs.readdirSync(dir, { withFileTypes: true });
      for (const entry of entries) {
        if (entry.isDirectory()) {
          const nested = path.join(dir, entry.name, 'bin', fullName);
          if (fs.existsSync(nested)) return nested;
          const nested2 = path.join(dir, entry.name, fullName);
          if (fs.existsSync(nested2)) return nested2;
        }
      }
    } catch (e) { /* ignore */ }
  }

  return null;
}

function findAvrdudeConf(avrdudePath) {
  if (!avrdudePath) return null;
  const dir = path.dirname(avrdudePath);

  // Check same directory
  const sameDir = path.join(dir, 'avrdude.conf');
  if (fs.existsSync(sameDir)) return sameDir;

  // Check parent/etc
  const etcDir = path.join(dir, '..', 'etc', 'avrdude.conf');
  if (fs.existsSync(etcDir)) return etcDir;

  // Check parent
  const parentDir = path.join(dir, '..', 'avrdude.conf');
  if (fs.existsSync(parentDir)) return parentDir;

  return null;
}

// ══════════════════════════════════════════════════════════════
// Board Definitions
// ══════════════════════════════════════════════════════════════
const BOARD_DEFS = {
  mega: {
    name: 'Arduino Mega 2560',
    mcu: 'atmega2560',
    programmer: 'wiring',
    baudRate: 115200,
    fqbn: 'arduino:avr:mega',
    flashTool: 'avrdude',
    protocol: 'wiring',
  },
  'nano-new': {
    name: 'Arduino Nano (New Bootloader)',
    mcu: 'atmega328p',
    programmer: 'arduino',
    baudRate: 115200,
    fqbn: 'arduino:avr:nano',
    flashTool: 'avrdude',
    protocol: 'arduino',
  },
  'nano-old': {
    name: 'Arduino Nano (Old Bootloader)',
    mcu: 'atmega328p',
    programmer: 'arduino',
    baudRate: 57600,
    fqbn: 'arduino:avr:nano:cpu=atmega328old',
    flashTool: 'avrdude',
    protocol: 'arduino',
  },
  uno: {
    name: 'Arduino Uno',
    mcu: 'atmega328p',
    programmer: 'arduino',
    baudRate: 115200,
    fqbn: 'arduino:avr:uno',
    flashTool: 'avrdude',
    protocol: 'arduino',
  },
  esp32: {
    name: 'ESP32 DevKit',
    flashTool: 'esptool',
    fqbn: 'esp32:esp32:esp32',
    flashBaud: 921600,
  },
  esp32s3: {
    name: 'ESP32-S3',
    flashTool: 'esptool',
    fqbn: 'esp32:esp32:esp32s3',
    flashBaud: 921600,
  },
  stm32: {
    name: 'STM32 BluePill',
    flashTool: 'stm32flash',
    fqbn: 'STMicroelectronics:stm32:GenF1',
  },
  pico: {
    name: 'Raspberry Pi Pico',
    flashTool: 'uf2',
    fqbn: 'rp2040:rp2040:rpipico',
  },
};

// USB VID/PID for board auto-identification
const KNOWN_BOARDS = {
  '2341:0042': { hint: 'Arduino Mega 2560', board: 'mega' },
  '2341:0043': { hint: 'Arduino Mega 2560 (clone)', board: 'mega' },
  '2341:0001': { hint: 'Arduino Uno', board: 'uno' },
  '2341:7523': { hint: 'Arduino Nano', board: 'nano-new' },
  '1A86:7523': { hint: 'CH340 (Arduino clone)', board: 'nano-new' },
  '0403:6001': { hint: 'FTDI (Arduino clone)', board: 'nano-new' },
  '10C4:EA60': { hint: 'CP2102 (ESP32)', board: 'esp32' },
  '303A:1001': { hint: 'ESP32-S3', board: 'esp32s3' },
};

// ══════════════════════════════════════════════════════════════
// IPC Handlers
// ══════════════════════════════════════════════════════════════

// Check toolchain availability
ipcMain.handle('check-toolchain', async () => {
  const result = {};
  result.avrdude = !!findExecutable('avrdude');
  result['arduino-cli'] = !!findExecutable('arduino-cli');
  result.esptool = !!findExecutable('esptool') || !!findExecutable('esptool.py');
  return result;
});

// Install missing toolchains
ipcMain.handle('install-toolchain', async () => {
  const results = [];

  if (!fs.existsSync(APP_DATA_DIR)) {
    fs.mkdirSync(APP_DATA_DIR, { recursive: true });
  }

  if (!findExecutable('arduino-cli')) {
    try {
      const isWin = process.platform === 'win32';
      if (isWin) {
        try {
          const cliDir = path.join(APP_DATA_DIR, 'arduino-cli');
          if (!fs.existsSync(cliDir)) fs.mkdirSync(cliDir, { recursive: true });
          const zipPath = path.join(APP_DATA_DIR, 'arduino-cli.zip');
          
          const script = `Invoke-WebRequest -Uri 'https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip' -OutFile '${zipPath}'; Expand-Archive -Path '${zipPath}' -DestinationPath '${cliDir}' -Force; Remove-Item -Path '${zipPath}'`;
          await execAsync(`powershell -Command "${script}"`, { timeout: 120000 });
          
          results.push('arduino-cli: installed via direct download');
        } catch (e) {
          results.push('arduino-cli: download failed, please install manually from https://arduino.github.io/arduino-cli/');
        }
      } else {
        await execAsync('curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=/usr/local/bin sh', { timeout: 120000 });
        results.push('arduino-cli: installed');
      }
    } catch (e) {
      results.push(`arduino-cli: install failed (${e.message})`);
    }
  } else {
    results.push('arduino-cli: already found');
  }

  // Install arduino:avr core and required libraries via arduino-cli
  const cliPath = findExecutable('arduino-cli');
  if (cliPath) {
    try {
      await execAsync(`"${cliPath}" core update-index`, { timeout: 120000 });
      await execAsync(`"${cliPath}" core install arduino:avr`, { timeout: 300000 });
      results.push('arduino:avr core: installed');
      
      await execAsync(`"${cliPath}" lib install TMCStepper`, { timeout: 120000 });
      results.push('TMCStepper library: installed');
    } catch (e) {
      results.push(`arduino-cli packages: ${e.message}`);
    }
  }

  // Check esptool
  if (!findExecutable('esptool') && !findExecutable('esptool.py')) {
    try {
      await execAsync('pip install esptool', { timeout: 120000 });
      results.push('esptool: installed via pip');
    } catch (e) {
      try {
        await execAsync('pip3 install esptool', { timeout: 120000 });
        results.push('esptool: installed via pip3');
      } catch (e2) {
        results.push('esptool: pip install failed, install Python and run: pip install esptool');
      }
    }
  } else {
    results.push('esptool: already found');
  }

  return results.join('\n');
});

// Detect serial ports with board identification
ipcMain.handle('detect-boards', async () => {
  const { SerialPort } = await import('serialport');
  const ports = await SerialPort.list();

  return ports.map(p => {
    const vidPid = `${(p.vendorId || '').toUpperCase()}:${(p.productId || '').toUpperCase()}`;
    const known = KNOWN_BOARDS[vidPid];

    return {
      path: p.path,
      manufacturer: p.manufacturer || 'Unknown',
      vendorId: p.vendorId,
      productId: p.productId,
      serialNumber: p.serialNumber,
      isArduino: !!known || (p.manufacturer || '').toLowerCase().includes('arduino'),
      hint: known?.hint || p.manufacturer || 'Unknown Device',
      detectedBoard: known?.board || null,
    };
  });
});

// Set target serial port for Web Serial
ipcMain.on('set-target-serial-port', (event, portName) => {
  console.log(`Target serial port set: ${portName}`);
  global.targetSerialPort = portName;
  event.returnValue = true;
});

// Flash pre-compiled firmware
ipcMain.handle('flash-firmware', async (event, { boardType, fileSource, hexContent, port }) => {
  const boardDef = BOARD_DEFS[boardType];
  if (!boardDef) throw new Error(`Unknown board type: ${boardType}`);
  if (!port) throw new Error('No port selected');

  const sendProgress = (msg) => {
    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('flash-progress', msg + '\n');
    }
  };

  sendProgress(`═══════════════════════════════════════`);
  sendProgress(`OpenPlotter Firmware Flash`);
  sendProgress(`Board: ${boardDef.name}`);
  sendProgress(`Port: ${port}`);
  sendProgress(`═══════════════════════════════════════`);

  if (boardDef.flashTool === 'avrdude') {
    return await flashWithAvrdude(boardDef, fileSource, hexContent, port, sendProgress);
  } else if (boardDef.flashTool === 'esptool') {
    return await flashWithEsptool(boardDef, fileSource, hexContent, port, sendProgress);
  } else if (boardDef.flashTool === 'uf2') {
    sendProgress('UF2 flashing: Put your Pico in BOOTSEL mode and copy the .uf2 file.');
    return 'UF2 flash requires drag-and-drop. Put Pico in BOOTSEL mode.';
  } else {
    throw new Error(`Unsupported flash tool: ${boardDef.flashTool}`);
  }
});

async function flashWithAvrdude(boardDef, fileSource, hexContent, port, sendProgress) {
  const avrdudePath = findExecutable('avrdude');
  if (!avrdudePath) {
    throw new Error('avrdude not found. Click "Auto-Install Missing Tools" or install arduino-cli.');
  }

  sendProgress(`Using avrdude: ${avrdudePath}`);

  let hexFilePath;
  if (fileSource === 'custom' && hexContent) {
    hexFilePath = path.join(APP_DATA_DIR, 'custom_firmware.hex');
    fs.writeFileSync(hexFilePath, hexContent);
    sendProgress(`Custom firmware file saved to ${hexFilePath}`);
  } else {
    hexFilePath = path.join(getFirmwareDir(), 'build', `${boardDef.mcu}.hex`);
    if (!fs.existsSync(hexFilePath)) {
      throw new Error(`Built-in firmware not found at ${hexFilePath}. Use "Apply Config & Flash" to compile from source.`);
    }
    sendProgress(`Using bundled firmware: ${hexFilePath}`);
  }

  const confPath = findAvrdudeConf(avrdudePath);
  const args = [
    ...(confPath ? ['-C', confPath] : []),
    '-p', boardDef.mcu,
    '-c', boardDef.protocol || boardDef.programmer,
    '-P', port,
    '-b', String(boardDef.baudRate),
    '-D',
    '-U', `flash:w:${hexFilePath}:i`,
    '-v',
  ];

  sendProgress(`> ${avrdudePath} ${args.join(' ')}`);

  return new Promise((resolve, reject) => {
    const proc = spawn(avrdudePath, args);
    let output = '';

    proc.stdout.on('data', (data) => {
      const text = data.toString();
      output += text;
      sendProgress(text);
    });

    proc.stderr.on('data', (data) => {
      const text = data.toString();
      output += text;
      sendProgress(text);
    });

    proc.on('close', (code) => {
      if (code === 0) {
        sendProgress('\n✓ Flash successful!');
        resolve('Firmware flashed successfully!');
      } else {
        sendProgress(`\n✕ Flash failed (exit code ${code})`);
        reject(new Error(`avrdude exited with code ${code}. Check the log for details.`));
      }
    });

    proc.on('error', (err) => {
      sendProgress(`\n✕ Failed to start avrdude: ${err.message}`);
      reject(new Error(`Failed to start avrdude: ${err.message}`));
    });
  });
}

async function flashWithEsptool(boardDef, fileSource, hexContent, port, sendProgress) {
  const esptoolPath = findExecutable('esptool') || findExecutable('esptool.py');
  if (!esptoolPath) {
    throw new Error('esptool not found. Run "pip install esptool" or click Auto-Install.');
  }

  sendProgress(`Using esptool: ${esptoolPath}`);

  let binFilePath;
  if (fileSource === 'custom' && hexContent) {
    binFilePath = path.join(APP_DATA_DIR, 'custom_firmware.bin');
    fs.writeFileSync(binFilePath, hexContent);
  } else {
    binFilePath = path.join(getFirmwareDir(), 'build', 'esp32_firmware.bin');
    if (!fs.existsSync(binFilePath)) {
      throw new Error('Built-in ESP32 firmware not found. Compile from source first.');
    }
  }

  const args = [
    '--chip', boardType === 'esp32s3' ? 'esp32s3' : 'esp32',
    '--port', port,
    '--baud', String(boardDef.flashBaud || 921600),
    'write_flash',
    '-z',
    '0x10000', binFilePath,
  ];

  sendProgress(`> ${esptoolPath} ${args.join(' ')}`);

  return new Promise((resolve, reject) => {
    const proc = spawn(esptoolPath, args);

    proc.stdout.on('data', (data) => sendProgress(data.toString()));
    proc.stderr.on('data', (data) => sendProgress(data.toString()));

    proc.on('close', (code) => {
      if (code === 0) {
        sendProgress('\n✓ ESP32 flash successful!');
        resolve('ESP32 firmware flashed successfully!');
      } else {
        sendProgress(`\n✕ Flash failed (exit code ${code})`);
        reject(new Error(`esptool exited with code ${code}`));
      }
    });

    proc.on('error', (err) => {
      reject(new Error(`Failed to start esptool: ${err.message}`));
    });
  });
}

// Compile and flash from source
ipcMain.handle('compile-and-flash-firmware', async (event, { boardType, port, config }) => {
  const boardDef = BOARD_DEFS[boardType];
  if (!boardDef) throw new Error(`Unknown board: ${boardType}`);

  const sendProgress = (msg) => {
    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('flash-progress', msg + '\n');
    }
  };

  const cliPath = findExecutable('arduino-cli');
  if (!cliPath) {
    throw new Error('arduino-cli not found. Click "Auto-Install Missing Tools".');
  }

  sendProgress(`═══════════════════════════════════════`);
  sendProgress(`Compile & Flash: ${boardDef.name}`);
  sendProgress(`Port: ${port}`);
  sendProgress(`═══════════════════════════════════════`);

  // Step 1: Modify openplotter_config.h with the user's settings
  // Step 1: Copy firmware source to a writable directory
  const sourceDir = getFirmwareDir();
  const buildDir = path.join(APP_DATA_DIR, 'firmware_src');
  
  if (fs.existsSync(buildDir)) {
    fs.rmSync(buildDir, { recursive: true, force: true });
  }
  fs.cpSync(sourceDir, buildDir, { recursive: true });
  sendProgress(`Copied firmware source to writable directory: ${buildDir}`);

  // Step 2: Modify openplotter_config.h with the user's settings
  const configPath = path.join(buildDir, 'openplotter_config.h');
  let originalConfig = '';

  if (fs.existsSync(configPath)) {
    originalConfig = fs.readFileSync(configPath, 'utf-8');

    // Generate #defines based on config
    let modified = originalConfig;

    // Set board profile based on boardType
    const boardProfiles = {
      mega: 'BOARD_MEGA_RAMPS14',
      'nano-new': 'BOARD_NANO_STANDALONE',
      'nano-old': 'BOARD_NANO_STANDALONE',
      uno: 'BOARD_MEGA_STANDALONE',
      esp32: 'BOARD_ESP32_STANDALONE',
    };

    const profile = boardProfiles[boardType] || 'BOARD_MEGA_RAMPS14';

    // Uncomment the right board profile, comment out the rest
    Object.values(boardProfiles).forEach(bp => {
      modified = modified.replace(new RegExp(`^(\\s*)#define ${bp}`, 'm'), `$1// #define ${bp}`);
      modified = modified.replace(new RegExp(`^(\\s*)//\\s*#define ${bp}`, 'm'), `$1// #define ${bp}`);
    });
    modified = modified.replace(new RegExp(`^(\\s*)//\\s*#define ${profile}`, 'm'), `$1#define ${profile}`);

    // Apply numeric settings
    const settingsMap = {
      stepsX: ['DEFAULT_STEPS_PER_MM_X', 'f'],
      stepsY: ['DEFAULT_STEPS_PER_MM_Y', 'f'],
      maxSpeed: ['DEFAULT_MAX_RATE_X', 'f'],
      maxAccel: ['DEFAULT_ACCELERATION', 'f'],
      motorCurrent: ['DEFAULT_TMC_RUN_CURRENT_MA', ''],
    };

    Object.entries(settingsMap).forEach(([key, [define, suffix]]) => {
      if (config[key]) {
        const val = config[key] + (suffix ? suffix : '');
        const regex = new RegExp(`#define ${define}\\s+[\\d\\.]+[f]?`, 'g');
        modified = modified.replace(regex, `#define ${define}${' '.repeat(Math.max(1, 24 - define.length))}${val}`);
      }
    });

    // Apply boolean settings (invert)
    if (config.invertX !== undefined) {
      modified = modified.replace(/#define DEFAULT_INVERT_X\s+\w+/, `#define DEFAULT_INVERT_X            ${config.invertX ? 'true' : 'false'}`);
    }
    if (config.invertY !== undefined) {
      modified = modified.replace(/#define DEFAULT_INVERT_Y\s+\w+/, `#define DEFAULT_INVERT_Y            ${config.invertY ? 'true' : 'false'}`);
    }

    fs.writeFileSync(configPath, modified);
    sendProgress(`Config updated for ${boardDef.name}`);
  }

  try {
    // Step 3: Compile using arduino-cli
    const compileArgs = [
      'compile',
      '--fqbn', boardDef.fqbn || 'arduino:avr:mega',
      buildDir,
      '--verbose',
    ];

    sendProgress(`\n── Compiling ──`);
    sendProgress(`> ${cliPath} ${compileArgs.join(' ')}`);

    await new Promise((resolve, reject) => {
      const proc = spawn(cliPath, compileArgs);

      proc.stdout.on('data', (data) => sendProgress(data.toString()));
      proc.stderr.on('data', (data) => sendProgress(data.toString()));

      proc.on('close', (code) => {
        if (code === 0) {
          sendProgress('\n✓ Compilation successful');
          resolve();
        } else {
          sendProgress(`\n✕ Compilation failed (exit code ${code})`);
          reject(new Error(`Compilation failed with exit code ${code}`));
        }
      });

      proc.on('error', (err) => reject(err));
    });

    // Step 4: Upload
    const uploadArgs = [
      'upload',
      '--fqbn', boardDef.fqbn || 'arduino:avr:mega',
      '--port', port,
      buildDir,
      '--verbose',
    ];

    sendProgress(`\n── Uploading ──`);
    sendProgress(`> ${cliPath} ${uploadArgs.join(' ')}`);

    await new Promise((resolve, reject) => {
      const proc = spawn(cliPath, uploadArgs);

      proc.stdout.on('data', (data) => sendProgress(data.toString()));
      proc.stderr.on('data', (data) => sendProgress(data.toString()));

      proc.on('close', (code) => {
        if (code === 0) {
          sendProgress('\n✓ Upload successful!');
          resolve();
        } else {
          sendProgress(`\n✕ Upload failed (exit code ${code})`);
          reject(new Error(`Upload failed with exit code ${code}`));
        }
      });

      proc.on('error', (err) => reject(err));
    });

    return 'Compiled and flashed successfully!';
  } finally {
    // Step 4: Restore original config.h
    if (originalConfig && fs.existsSync(configPath)) {
      fs.writeFileSync(configPath, originalConfig);
      sendProgress('Config restored to original.');
    }
  }
});

// File dialogs
ipcMain.handle('open-file-dialog', async (event, options) => {
  const result = await dialog.showOpenDialog(mainWindow, {
    properties: ['openFile'],
    filters: [
      { name: 'All Supported', extensions: ['svg', 'png', 'jpg', 'jpeg', 'gcode', 'hex', 'bin', 'uf2'] },
      { name: 'Design Files', extensions: ['svg', 'png', 'jpg', 'jpeg'] },
      { name: 'G-code', extensions: ['gcode', 'nc', 'ngc'] },
      { name: 'Firmware', extensions: ['hex', 'bin', 'uf2'] },
    ],
    ...options,
  });

  if (result.canceled || result.filePaths.length === 0) return null;
  return { path: result.filePaths[0], content: fs.readFileSync(result.filePaths[0], 'utf-8') };
});

ipcMain.handle('save-file-dialog', async (event, options) => {
  const result = await dialog.showSaveDialog(mainWindow, {
    filters: [
      { name: 'G-code', extensions: ['gcode'] },
      { name: 'All Files', extensions: ['*'] },
    ],
    ...options,
  });

  if (result.canceled) return null;
  return result.filePath;
});

ipcMain.handle('get-app-version', () => {
  return app.getVersion();
});

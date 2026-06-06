const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const os = require('os');
const fs = require('fs');
const { spawn } = require('child_process');
const { SerialPort } = require('serialport');
let mainWindow;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1280,
    height: 800,
    title: "OpenPlotter",
    backgroundColor: '#0a0d16',
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      preload: path.join(__dirname, 'electron-preload.js')
    }
  });

  // Enable Web Serial API permissions and port selection
  mainWindow.webContents.session.on('select-serial-port', (event, portList, webContents, callback) => {
    event.preventDefault();
    if (portList && portList.length > 0) {
      // Automatically select the first port, or we can send it to the UI to select.
      // For a native feel, we select the first one, or let the user choose.
      // In electron, if we don't preventDefault, it will wait.
      // Let's select the first port for simplicity, or implement a simple choice.
      callback(portList[0].portId);
    } else {
      callback(''); // No ports available
    }
  });

  mainWindow.webContents.session.setPermissionCheckHandler((webContents, permission, requestingOrigin, details) => {
    if (permission === 'serial') {
      return true;
    }
    return false;
  });

  mainWindow.webContents.session.setDevicePermissionHandler((details) => {
    if (details.deviceType === 'serial') {
      return true;
    }
    return false;
  });

  // In development, load from Vite dev server. In production, load the built dist/index.html.
  const isDev = process.env.NODE_ENV === 'development';
  if (isDev) {
    mainWindow.loadURL('http://localhost:5173');
    mainWindow.webContents.openDevTools();
  } else {
    mainWindow.loadFile(path.join(__dirname, 'dist', 'index.html'));
  }

  mainWindow.webContents.on('console-message', (event, level, message, line, sourceId) => {
    console.log(`[Renderer] ${message} (${sourceId}:${line})`);
  });

  mainWindow.on('closed', () => {
    mainWindow = null;
  });
}

app.whenReady().then(() => {
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

ipcMain.handle('flash-firmware', async (event, { boardType, hexContent, port }) => {
  return new Promise((resolve, reject) => {
    try {
      if (!port) return reject("Please select a COM port first.");
      
      const tempPath = path.join(os.tmpdir(), `openplotter_${boardType}.hex`);
      fs.writeFileSync(tempPath, hexContent);
      
      let avrdudeExe = 'avrdude';
      
      // On Windows, try to find Arduino's bundled avrdude
      if (process.platform === 'win32') {
        const arduino15 = path.join(process.env.LOCALAPPDATA, 'Arduino15', 'packages', 'arduino', 'tools', 'avrdude');
        if (fs.existsSync(arduino15)) {
          const versions = fs.readdirSync(arduino15);
          if (versions.length > 0) {
            const avrdudePath = path.join(arduino15, versions[0], 'bin', 'avrdude.exe');
            const confPath = path.join(arduino15, versions[0], 'etc', 'avrdude.conf');
            if (fs.existsSync(avrdudePath)) {
              avrdudeExe = `"${avrdudePath}" -C "${confPath}"`;
            }
          }
        }
      }

      let cmdPath = avrdudeExe;
      let cmdArgs = [];
      if (boardType === 'mega') {
        if (avrdudeExe.includes(' -C ')) {
            const parts = avrdudeExe.split(' -C ');
            cmdPath = parts[0].replace(/"/g, '');
            cmdArgs = ['-C', parts[1].replace(/"/g, ''), '-v', '-patmega2560', '-cwiring', `-P${port}`, '-b115200', '-D', `-Uflash:w:${tempPath}:i`];
        } else {
            cmdArgs = ['-v', '-patmega2560', '-cwiring', `-P${port}`, '-b115200', '-D', `-Uflash:w:${tempPath}:i`];
        }
      } else if (boardType === 'nano') {
        if (avrdudeExe.includes(' -C ')) {
            const parts = avrdudeExe.split(' -C ');
            cmdPath = parts[0].replace(/"/g, '');
            cmdArgs = ['-C', parts[1].replace(/"/g, ''), '-v', '-patmega328p', '-carduino', `-P${port}`, '-b115200', '-D', `-Uflash:w:${tempPath}:i`];
        } else {
            cmdArgs = ['-v', '-patmega328p', '-carduino', `-P${port}`, '-b115200', '-D', `-Uflash:w:${tempPath}:i`];
        }
      } else {
        return reject("Unsupported board type");
      }
      
      event.sender.send('flash-progress', `Running: ${cmdPath} ${cmdArgs.join(' ')}\n`);
      const child = spawn(cmdPath, cmdArgs);
      
      child.stdout.on('data', (data) => {
        event.sender.send('flash-progress', data.toString());
      });
      
      child.stderr.on('data', (data) => {
        event.sender.send('flash-progress', data.toString());
      });
      
      child.on('error', (error) => {
        event.sender.send('flash-progress', `\nError launching avrdude: ${error.message}\n`);
        reject("avrdude failed or not found. Please install Arduino IDE/avrdude and ensure it is in your system PATH.");
      });
      
      child.on('close', (code) => {
        if (code !== 0) {
          reject(`avrdude exited with code ${code}`);
        } else {
          resolve('Firmware flashed successfully!');
        }
      });
    } catch (e) {
      reject(e.message);
    }
  });
});

ipcMain.handle('compile-and-flash-firmware', async (event, { boardType, port, config }) => {
  return new Promise((resolve, reject) => {
    if (!port) return reject("Please select a COM port first.");
    event.sender.send('flash-progress', `Starting dynamic compilation for ${boardType}...\n`);
    event.sender.send('flash-progress', `Updating openplotter_config.h with UI settings...\n`);
    
    const projectDir = path.join(__dirname, '..', 'OpenPlotter');
    const configPath = path.join(projectDir, 'openplotter_config.h');
    
    try {
      // (Mock) config modifications
      event.sender.send('flash-progress', `(Mock) Applied settings: Tool=${config.tool1}, DriverX=${config.driverX}, Sensorless=${config.sensorless}\n`);
      
      // We'll run arduino-cli directly!
      let fqbn = boardType === 'mega' ? 'arduino:avr:mega' : 'arduino:avr:nano';
      const cliPath = path.join(projectDir, 'arduino-cli.exe');
      
      event.sender.send('flash-progress', `\nCompiling firmware... This may take a minute.\n`);
      const buildDir = path.join(os.tmpdir(), `openplotter_build_${boardType}`);
      
      const compileArgs = ['compile', '--fqbn', fqbn, 'OpenPlotter.ino', '--output-dir', buildDir];
      const compileChild = spawn(cliPath, compileArgs, { cwd: projectDir });
      
      compileChild.stdout.on('data', (data) => event.sender.send('flash-progress', data.toString()));
      compileChild.stderr.on('data', (data) => event.sender.send('flash-progress', data.toString()));
      
      compileChild.on('close', (code) => {
         if (code !== 0) {
           reject(`arduino-cli exited with code ${code}. Compilation failed.`);
         } else {
           event.sender.send('flash-progress', `\nCompilation successful! Flashing now...\n`);
           
           const hexPath = path.join(buildDir, 'OpenPlotter.ino.hex');
           if (!fs.existsSync(hexPath)) {
              return reject("Hex file not found after compilation.");
           }
           
           const hexContent = fs.readFileSync(hexPath, 'utf8');
           
           ipcMain.emit('flash-firmware-internal', event, event, { boardType, hexContent, port }, resolve, reject);
         }
      });
    } catch (e) {
      reject(`Compilation pipeline failed: ${e.message}`);
    }
  });
});

ipcMain.on('flash-firmware-internal', (event, originalEvent, args, resolve, reject) => {
  const { boardType, hexContent, port } = args;
  const tempPath = path.join(os.tmpdir(), `openplotter_${boardType}.hex`);
  fs.writeFileSync(tempPath, hexContent);
  
  let cmdPath = 'avrdude';
  let cmdArgs = [];
  
  if (process.platform === 'win32') {
    const arduino15 = path.join(process.env.LOCALAPPDATA, 'Arduino15', 'packages', 'arduino', 'tools', 'avrdude');
    if (fs.existsSync(arduino15)) {
      const versions = fs.readdirSync(arduino15);
      if (versions.length > 0) {
        const avrdudePath = path.join(arduino15, versions[0], 'bin', 'avrdude.exe');
        const confPath = path.join(arduino15, versions[0], 'etc', 'avrdude.conf');
        if (fs.existsSync(avrdudePath)) {
          cmdPath = avrdudePath;
          cmdArgs.push('-C', confPath);
        }
      }
    }
  }
  
  if (boardType === 'mega') {
    cmdArgs.push('-v', '-patmega2560', '-cwiring', `-P${port}`, '-b115200', '-D', `-Uflash:w:${tempPath}:i`);
  } else if (boardType === 'nano') {
    cmdArgs.push('-v', '-patmega328p', '-carduino', `-P${port}`, '-b115200', '-D', `-Uflash:w:${tempPath}:i`);
  }
  
  originalEvent.sender.send('flash-progress', `Running: ${cmdPath} ${cmdArgs.join(' ')}\n`);
  const child = spawn(cmdPath, cmdArgs);
  
  child.stdout.on('data', (data) => originalEvent.sender.send('flash-progress', data.toString()));
  child.stderr.on('data', (data) => originalEvent.sender.send('flash-progress', data.toString()));
  
  child.on('error', (error) => reject(`Error launching avrdude: ${error.message}`));
  child.on('close', (code) => {
    if (code !== 0) reject(`avrdude exited with code ${code}`);
    else resolve('Firmware compiled and flashed successfully!');
  });
});

ipcMain.handle('detect-boards', async () => {
  try {
    const ports = await SerialPort.list();
    // Known Arduino VIDs
    // Mega 2560 typically: 2341
    // Nano (CH340) typically: 1A86
    return ports.map(port => {
      let isArduino = false;
      let hint = '';
      if (port.vendorId) {
        const vid = port.vendorId.toLowerCase();
        if (vid.includes('2341') || vid.includes('2a03')) {
          isArduino = true;
          hint = 'Arduino Uno/Mega';
        } else if (vid.includes('1a86')) {
          isArduino = true;
          hint = 'Arduino Nano (CH340)';
        } else if (vid.includes('10c4') && port.productId && port.productId.toLowerCase().includes('ea60')) {
           isArduino = true;
           hint = 'ESP32 / CP210x';
        }
      }
      return {
        path: port.path,
        vendorId: port.vendorId,
        productId: port.productId,
        isArduino,
        hint
      };
    });
  } catch (error) {
    console.error("Error listing serial ports:", error);
    return [];
  }
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

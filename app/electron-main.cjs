const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const os = require('os');
const fs = require('fs');
const { exec } = require('child_process');
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
      
      let cmd = "";
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

      if (boardType === 'mega') {
        cmd = `${avrdudeExe} -v -patmega2560 -cwiring -P${port} -b115200 -D "-Uflash:w:${tempPath}:i"`;
      } else if (boardType === 'nano') {
        cmd = `${avrdudeExe} -v -patmega328p -carduino -P${port} -b115200 -D "-Uflash:w:${tempPath}:i"`;
      } else {
        return reject("Unsupported board type");
      }
      
      exec(cmd, (error, stdout, stderr) => {
        if (error) {
          console.error(stderr);
          reject("avrdude failed or not found. Please install Arduino IDE/avrdude and ensure it is in your system PATH.");
        } else {
          resolve('Firmware flashed successfully!');
        }
      });
    } catch (e) {
      reject(e.message);
    }
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

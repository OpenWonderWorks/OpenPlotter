const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  // Firmware flashing
  flashFirmware: (boardType, fileSource, hexContent, port) => ipcRenderer.invoke('flash-firmware', { boardType, fileSource, hexContent, port }),
  compileAndFlash: (boardType, port, config, advanced) => ipcRenderer.invoke('compile-and-flash-firmware', { boardType, port, config, advanced }),
  detectBoards: () => ipcRenderer.invoke('detect-boards'),
  onFlashProgress: (callback) => ipcRenderer.on('flash-progress', (event, data) => callback(data)),
  
  // Native file dialogs
  openFileDialog: (options) => ipcRenderer.invoke('open-file-dialog', options || {}),
  saveFileDialog: (options) => ipcRenderer.invoke('save-file-dialog', options || {}),
  
  // Toolchain management
  checkToolchain: () => ipcRenderer.invoke('check-toolchain'),
  installToolchain: () => ipcRenderer.invoke('install-toolchain'),
  
  // App info
  getAppVersion: () => ipcRenderer.invoke('get-app-version'),
  
  // Connection Configuration
  setTargetSerialPort: (portName) => ipcRenderer.sendSync('set-target-serial-port', portName)
});

window.addEventListener('DOMContentLoaded', () => {
  console.log('OpenPlotter Electron Preload Loaded');
});

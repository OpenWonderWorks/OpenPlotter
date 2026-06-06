const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  flashFirmware: (boardType, hexContent, port) => ipcRenderer.invoke('flash-firmware', { boardType, hexContent, port }),
  compileAndFlash: (boardType, port, config) => ipcRenderer.invoke('compile-and-flash-firmware', { boardType, port, config }),
  detectBoards: () => ipcRenderer.invoke('detect-boards'),
  onFlashProgress: (callback) => ipcRenderer.on('flash-progress', (event, data) => callback(data))
});

window.addEventListener('DOMContentLoaded', () => {
  console.log('OpenPlotter Electron Preload Loaded');
});

const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  flashFirmware: (boardType, hexContent, port) => ipcRenderer.invoke('flash-firmware', { boardType, hexContent, port })
});

window.addEventListener('DOMContentLoaded', () => {
  console.log('OpenPlotter Electron Preload Loaded');
});

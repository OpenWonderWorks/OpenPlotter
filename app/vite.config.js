import { defineConfig } from 'vite';

export default defineConfig({
  // Use relative paths so the app works under file:// protocols (Electron)
  base: './',
  server: {
    port: 5173
  }
});

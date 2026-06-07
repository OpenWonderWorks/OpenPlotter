const { app } = require('electron');
const path = require('path');
const fs = require('fs');

app.whenReady().then(async () => {
  try {
    const buildDir = path.join(app.getPath('userData'), 'firmware_build');
    const sketchPath = path.join(buildDir, 'OpenPlotter');
    const firmwareSrc = path.join(__dirname, '..', 'OpenPlotter');

    console.log("Firmware Src:", firmwareSrc);
    if (!fs.existsSync(firmwareSrc)) console.log("firmwareSrc missing!");

    if (fs.existsSync(sketchPath)) {
      fs.rmSync(sketchPath, { recursive: true, force: true });
    }
    if (!fs.existsSync(buildDir)) {
      fs.mkdirSync(buildDir, { recursive: true });
    }
    fs.cpSync(firmwareSrc, sketchPath, { recursive: true });
    console.log("Copied successfully to", sketchPath);
    
    // Simulate config modification
    const configPath = path.join(sketchPath, 'openplotter_config.h');
    let modified = fs.readFileSync(configPath, 'utf-8');
    // regex logic here
    const boardProfiles = {
      mega: 'BOARD_MEGA_RAMPS14',
    };
    Object.values(boardProfiles).forEach(bp => {
      modified = modified.replace(new RegExp(`^(\\s*)#define ${bp}`, 'm'), `$1// #define ${bp}`);
    });
    fs.writeFileSync(configPath, modified);
    
    console.log("Config modified successfully");
    
    app.quit();
  } catch(e) {
    console.error("FATAL CRASH:", e);
    app.exit(1);
  }
});

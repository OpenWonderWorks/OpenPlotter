/**
 * OpenPlotter — G-code Generator
 * Converts parsed polyline paths into cutter-optimized G-code commands.
 */

export function generateGcode(paths, options = {}) {
  const {
    feedRateLinear = 1500,     // mm/min for cutting
    feedRateRapid = 3000,      // mm/min for travel
    bladePressure = 120,       // 0-255 pressure (M3 S parameter)
    toolDownDelay = 150,       // ms wait after tool down
    toolUpDelay = 100,         // ms wait after tool up
    passCount = 1,             // Number of cutting passes
    bedSizeX = 300,            // Plotter bed X size (mm)
    bedSizeY = 300,            // Plotter bed Y size (mm)
    scale = 1.0,               // Additional scale factor
    offsetX = 0.0,             // Offset X (mm)
    offsetY = 0.0,             // Offset Y (mm)
    invertY = true,            // Invert SVG Y-axis (SVG top-left to G-code bottom-left)
    svgSize = { w: 300, h: 300 }
  } = options;

  const gcode = [];
  
  // Header
  gcode.push(`; OpenPlotter G-code Generator v1.0.0`);
  gcode.push(`; Generated: ${new Date().toISOString()}`);
  gcode.push(`; Bed Size: ${bedSizeX}x${bedSizeY} mm`);
  gcode.push(`; Pass Count: ${passCount}`);
  gcode.push(`; Pressure: ${bladePressure}`);
  gcode.push(``);
  gcode.push(`G21 ; Set units to millimeters`);
  gcode.push(`G90 ; Set to absolute positioning`);
  gcode.push(`M5  ; Ensure tool is up`);
  gcode.push(`G4 P200 ; Dwell 200ms`);
  
  if (paths.length === 0) {
    gcode.push(`; Warning: No paths provided`);
    return gcode.join('\n');
  }

  // Transformation helper
  // SVG Y goes down, Gcode Y goes up.
  function transform(pt) {
    let x = pt.x * scale + offsetX;
    let y = pt.y * scale + offsetY;
    
    if (invertY) {
      // SVG coordinate to G-code coordinate mapping
      // Flip relative to svgSize height, or scale to bed
      y = (svgSize.h - pt.y) * scale + offsetY;
    }
    
    // Clamp to bed dimensions for safety
    x = Math.max(0, Math.min(bedSizeX, x));
    y = Math.max(0, Math.min(bedSizeY, y));
    
    return {
      x: parseFloat(x.toFixed(3)),
      y: parseFloat(y.toFixed(3))
    };
  }

  // Generate movement commands
  for (let pass = 1; pass <= passCount; pass++) {
    gcode.push(``);
    gcode.push(`; ──── START PASS ${pass} ────`);
    
    paths.forEach((path, pathIdx) => {
      if (path.length === 0) return;
      
      gcode.push(``);
      gcode.push(`; Path ${pathIdx + 1} (points: ${path.length})`);
      
      // Move to first point (tool up)
      const startPt = transform(path[0]);
      gcode.push(`G0 X${startPt.x} Y${startPt.y} F${feedRateRapid} ; Rapid travel`);
      
      // Engage tool down
      gcode.push(`M3 S${bladePressure} ; Tool down`);
      if (toolDownDelay > 0) {
        gcode.push(`G4 P${toolDownDelay} ; Wait for tool to drop`);
      }
      
      // Draw/cut paths
      for (let i = 1; i < path.length; i++) {
        const pt = transform(path[i]);
        gcode.push(`G1 X${pt.x} Y${pt.y} F${feedRateLinear} ; Cut/draw`);
      }
      
      // Disengage tool up
      gcode.push(`M5 ; Tool up`);
      if (toolUpDelay > 0) {
        gcode.push(`G4 P${toolUpDelay} ; Wait for tool to lift`);
      }
    });
  }
  
  // Footer
  gcode.push(``);
  gcode.push(`; ──── FINISH ────`);
  gcode.push(`G0 X0 Y0 F${feedRateRapid} ; Return to home`);
  gcode.push(`M5 ; Tool up`);
  gcode.push(`M18 ; Disable motors`);
  gcode.push(`; End of G-code`);
  
  return gcode.join('\n');
}

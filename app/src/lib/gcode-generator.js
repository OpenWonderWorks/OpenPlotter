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
    passCount = 1,             // Number of cutting passes per head
    bedSizeX = 300,            // Plotter bed X size (mm)
    bedSizeY = 300,            // Plotter bed Y size (mm)
    scale = 1.0,               // Additional scale factor
    offsetX = 0.0,             // Offset X (mm)
    offsetY = 0.0,             // Offset Y (mm)
    invertY = true,            // Invert SVG Y-axis (SVG top-left to G-code bottom-left)
    svgSize = { w: 300, h: 300 },
    kinematics = 'cartesian',
    heads = 1,
    head2Offset = { x: 0, y: 0 },
    toolHead1 = 'dragknife',
    toolHead2 = 'pen'
  } = options;

  const gcode = [];
  
  // Header
  gcode.push(`; OpenPlotter G-code Generator v1.1.0`);
  gcode.push(`; Generated: ${new Date().toISOString()}`);
  gcode.push(`; Kinematics: ${kinematics}`);
  gcode.push(`; Heads: ${heads}`);
  gcode.push(`; Tool 1: ${toolHead1}`);
  if (heads > 1) {
    gcode.push(`; Tool 2: ${toolHead2} (Offset X:${head2Offset.x} Y:${head2Offset.y})`);
  }
  gcode.push(`; Bed Size: ${bedSizeX}x${bedSizeY} mm`);
  gcode.push(`; Pass Count per Head: ${passCount}`);
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
  function transform(pt, headOffset = {x: 0, y: 0}) {
    let x = pt.x * scale + offsetX + headOffset.x;
    let y = pt.y * scale + offsetY + headOffset.y;
    
    if (invertY) {
      // SVG coordinate to G-code coordinate mapping
      // Flip relative to svgSize height, or scale to bed
      y = (svgSize.h - pt.y) * scale + offsetY + headOffset.y;
    }
    
    // Clamp to bed dimensions for safety
    x = Math.max(0, Math.min(bedSizeX, x));
    y = Math.max(0, Math.min(bedSizeY, y));
    
    return {
      x: parseFloat(x.toFixed(3)),
      y: parseFloat(y.toFixed(3))
    };
  }

  // Generate movement commands for each head
  for (let headIdx = 0; headIdx < heads; headIdx++) {
    const currentTool = headIdx === 0 ? toolHead1 : toolHead2;
    const currentOffset = headIdx === 0 ? {x: 0, y: 0} : head2Offset;
    
    gcode.push(``);
    gcode.push(`; ═════════ HEAD ${headIdx + 1} (${currentTool}) ═════════`);
    gcode.push(`T${headIdx} ; Select Tool ${headIdx}`);
    
    for (let pass = 1; pass <= passCount; pass++) {
      gcode.push(``);
      gcode.push(`; ──── START PASS ${pass} (Head ${headIdx + 1}) ────`);
      
      paths.forEach((path, pathIdx) => {
        if (path.length === 0) return;
        
        gcode.push(``);
        gcode.push(`; Path ${pathIdx + 1} (points: ${path.length})`);
        
        // Move to first point (tool up)
        const startPt = transform(path[0], currentOffset);
        gcode.push(`G0 X${startPt.x} Y${startPt.y} F${feedRateRapid} ; Rapid travel`);
        
        // Engage tool down
        gcode.push(`M3 S${bladePressure} ; Tool down (${currentTool})`);
        if (toolDownDelay > 0) {
          gcode.push(`G4 P${toolDownDelay} ; Wait for tool to drop`);
        }
        
        // Draw/cut paths
        for (let i = 1; i < path.length; i++) {
          const pt = transform(path[i], currentOffset);
          gcode.push(`G1 X${pt.x} Y${pt.y} F${feedRateLinear} ; Cut/draw`);
        }
        
        // Disengage tool up
        gcode.push(`M5 ; Tool up`);
        if (toolUpDelay > 0) {
          gcode.push(`G4 P${toolUpDelay} ; Wait for tool to lift`);
        }
      });
    }
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

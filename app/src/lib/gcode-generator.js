/**
 * OpenPlotter G-code Generator v3.1.0
 * Converts SVG/traced paths to optimized OpenPlotter G-code.
 * Supports dual-head workflows and tool-specific M-codes.
 */

/**
 * Get the human-readable verb for a tool type
 */
export function getToolVerb(toolType) {
  switch (toolType) {
    case 'pen': return 'Draw';
    case 'dragknife': return 'Cut';
    case 'cutter': return 'Cut';
    case 'controllerknife': return 'Cut';
    default: return 'Process';
  }
}

/**
 * Get the tool action name (for labels)
 */
export function getToolAction(toolType) {
  switch (toolType) {
    case 'pen': return 'Drawing';
    case 'dragknife': return 'Cutting';
    case 'cutter': return 'Cutting';
    case 'controllerknife': return 'Cutting';
    default: return 'Processing';
  }
}

/**
 * Get CSS class suffix for tool-colored buttons
 */
export function getToolColorClass(toolType) {
  switch (toolType) {
    case 'pen': return 'pen';
    case 'dragknife':
    case 'cutter':
    case 'controllerknife': return 'knife';
    default: return 'pen';
  }
}

/**
 * Get pressure/parameter label for a tool type
 */
export function getToolPressureLabel(toolType) {
  switch (toolType) {
    case 'pen': return 'Pen Pressure (0-255)';
    case 'dragknife': return 'Blade Pressure (0-255)';
    case 'cutter': return 'Spindle Speed (RPM)';
    case 'controllerknife': return 'Blade Pressure (0-255)';
    default: return 'Pressure (0-255)';
  }
}

/**
 * Get speed label for a tool type
 */
export function getToolSpeedLabel(toolType) {
  switch (toolType) {
    case 'pen': return 'Draw Speed (mm/min)';
    case 'dragknife': return 'Cut Speed (mm/min)';
    case 'cutter': return 'Cut Speed (mm/min)';
    case 'controllerknife': return 'Cut Speed (mm/min)';
    default: return 'Speed (mm/min)';
  }
}

/**
 * Get workflow options for dual-head setups
 */
export function getWorkflowOptions(tool1, tool2) {
  const verb1 = getToolVerb(tool1);
  const verb2 = getToolVerb(tool2);
  const sameTool = tool1 === tool2;

  if (sameTool) {
    const verb = verb1;
    return [
      { id: 'head1-then-2', label: `${verb} Head 1 → Head 2`, icon: '➡️' },
      { id: 'head2-then-1', label: `${verb} Head 2 → Head 1`, icon: '⬅️' },
      { id: 'head1-only', label: `${verb} Head 1 Only`, icon: '1️⃣' },
      { id: 'head2-only', label: `${verb} Head 2 Only`, icon: '2️⃣' },
    ];
  }

  return [
    { id: 'head1-then-2', label: `${verb1} → ${verb2}`, icon: '➡️', desc: `Head 1 (${tool1}) then Head 2 (${tool2})` },
    { id: 'head2-then-1', label: `${verb2} → ${verb1}`, icon: '⬅️', desc: `Head 2 (${tool2}) then Head 1 (${tool1})` },
    { id: 'head1-only', label: `${verb1} Only (Head 1)`, icon: '1️⃣' },
    { id: 'head2-only', label: `${verb2} Only (Head 2)`, icon: '2️⃣' },
  ];
}

/**
 * Get the combined start button label for dual heads
 */
export function getDualHeadLabel(tool1, tool2, workflow) {
  const verb1 = getToolVerb(tool1);
  const verb2 = getToolVerb(tool2);

  if (tool1 === tool2) {
    return `Start Dual ${verb1}`;
  }

  switch (workflow) {
    case 'head1-then-2': return `Start ${verb1} & ${verb2}`;
    case 'head2-then-1': return `Start ${verb2} & ${verb1}`;
    case 'head1-only': return `Start ${verb1}`;
    case 'head2-only': return `Start ${verb2}`;
    default: return `Start ${verb1} & ${verb2}`;
  }
}

/**
 * Estimate total job time in seconds
 */
export function estimateJobTime(paths, options = {}) {
  const { scale = 1.0, feedRateLinear = 1500, feedRateRapid = 3000, passCount = 1 } = options;
  
  let linearDist = 0;
  let rapidDist = 0;

  paths.forEach(path => {
    if (path.length < 2) return;
    
    // Rapid to first point (estimate from origin)
    rapidDist += Math.sqrt(
      Math.pow(path[0].x * scale, 2) + Math.pow(path[0].y * scale, 2)
    );

    // Linear moves along path
    for (let i = 1; i < path.length; i++) {
      const dx = (path[i].x - path[i - 1].x) * scale;
      const dy = (path[i].y - path[i - 1].y) * scale;
      linearDist += Math.sqrt(dx * dx + dy * dy);
    }
  });

  // mm/min → mm/sec
  const linearTime = (linearDist / (feedRateLinear / 60)) * passCount;
  const rapidTime = rapidDist / (feedRateRapid / 60);

  return Math.round(linearTime + rapidTime);
}

/**
 * Format seconds to human-readable time
 */
export function formatTime(seconds) {
  if (seconds < 60) return `${seconds}s`;
  const min = Math.floor(seconds / 60);
  const sec = seconds % 60;
  if (min < 60) return `${min}m ${sec}s`;
  const hr = Math.floor(min / 60);
  const rm = min % 60;
  return `${hr}h ${rm}m`;
}

/**
 * Main G-code generation function
 */
export function generateGcode(paths, options = {}) {
  const {
    feedRateLinear = 1500,
    feedRateRapid = 3000,
    bladePressure = 120,
    toolDownDelay = 150,
    toolUpDelay = 100,
    passCount = 1,
    bedSizeX = 300,
    bedSizeY = 300,
    scale = 1.0,
    offsetX = 0.0,
    offsetY = 0.0,
    invertY = true,
    svgSize = { w: 300, h: 300 },
    kinematics = 'cartesian',
    heads = 1,
    head2Offset = { x: 0, y: 0 },
    toolHead1 = 'dragknife',
    toolHead2 = 'pen',
    workflow = 'head1-then-2'
  } = options;

  const gcode = [];
  const verb1 = getToolVerb(toolHead1);
  const verb2 = getToolVerb(toolHead2);
  const estTime = estimateJobTime(paths, { scale, feedRateLinear, feedRateRapid, passCount });

  // Header
  gcode.push(`; OpenPlotter G-code Generator v3.1.0`);
  gcode.push(`; Generated: ${new Date().toISOString()}`);
  gcode.push(`; Kinematics: ${kinematics}`);
  gcode.push(`; Heads: ${heads}`);
  gcode.push(`; Head 1: ${toolHead1} (${verb1})`);
  if (heads > 1) {
    gcode.push(`; Head 2: ${toolHead2} (${verb2}) — Offset X:${head2Offset.x} Y:${head2Offset.y}`);
    gcode.push(`; Workflow: ${workflow}`);
  }
  gcode.push(`; Bed: ${bedSizeX}×${bedSizeY} mm`);
  gcode.push(`; Passes: ${passCount}`);
  gcode.push(`; Pressure: ${bladePressure}`);
  gcode.push(`; Est. Time: ~${formatTime(estTime)}`);
  gcode.push(``);
  gcode.push(`G21 ; Units: millimeters`);
  gcode.push(`G90 ; Absolute positioning`);
  gcode.push(`M5  ; Tool up (safe start)`);
  gcode.push(`G4 P200 ; Dwell`);

  if (paths.length === 0) {
    gcode.push(`; Warning: No paths`);
    return gcode.join('\n');
  }

  // Transform helper
  function transform(pt, headOffset = { x: 0, y: 0 }) {
    let x = pt.x * scale + offsetX + headOffset.x;
    let y = pt.y * scale + offsetY + headOffset.y;

    if (invertY) {
      y = (svgSize.h - pt.y) * scale + offsetY + headOffset.y;
    }

    x = Math.max(0, Math.min(bedSizeX, x));
    y = Math.max(0, Math.min(bedSizeY, y));

    return {
      x: parseFloat(x.toFixed(3)),
      y: parseFloat(y.toFixed(3))
    };
  }

  // Generate paths for a single head
  function generateHeadPaths(headIdx, tool, offset) {
    const verb = getToolVerb(tool);
    const action = getToolAction(tool);

    gcode.push(``);
    gcode.push(`; ═══════════ HEAD ${headIdx + 1}: ${tool} (${verb}) ═══════════`);
    gcode.push(`T${headIdx} ; Select Tool ${headIdx}`);

    for (let pass = 1; pass <= passCount; pass++) {
      gcode.push(``);
      gcode.push(`; ── Pass ${pass}/${passCount} (Head ${headIdx + 1} — ${action}) ──`);

      paths.forEach((path, pathIdx) => {
        if (path.length === 0) return;

        gcode.push(``);
        gcode.push(`; Path ${pathIdx + 1} (${path.length} pts)`);

        // Rapid to first point
        const startPt = transform(path[0], offset);
        gcode.push(`G0 X${startPt.x} Y${startPt.y} F${feedRateRapid} ; Travel`);

        // Tool down
        gcode.push(`M3 S${bladePressure} ; Tool down — ${action.toLowerCase()}`);
        if (toolDownDelay > 0) {
          gcode.push(`G4 P${toolDownDelay} ; Wait for engagement`);
        }

        // Move along path
        for (let i = 1; i < path.length; i++) {
          const pt = transform(path[i], offset);
          gcode.push(`G1 X${pt.x} Y${pt.y} F${feedRateLinear}`);
        }

        // Tool up
        gcode.push(`M5 ; Tool up`);
        if (toolUpDelay > 0) {
          gcode.push(`G4 P${toolUpDelay} ; Wait for retraction`);
        }
      });
    }
  }

  // Determine head execution order based on workflow
  if (heads === 1) {
    generateHeadPaths(0, toolHead1, { x: 0, y: 0 });
  } else {
    switch (workflow) {
      case 'head1-then-2':
        generateHeadPaths(0, toolHead1, { x: 0, y: 0 });
        generateHeadPaths(1, toolHead2, head2Offset);
        break;
      case 'head2-then-1':
        generateHeadPaths(1, toolHead2, head2Offset);
        generateHeadPaths(0, toolHead1, { x: 0, y: 0 });
        break;
      case 'head1-only':
        generateHeadPaths(0, toolHead1, { x: 0, y: 0 });
        break;
      case 'head2-only':
        generateHeadPaths(1, toolHead2, head2Offset);
        break;
      default:
        generateHeadPaths(0, toolHead1, { x: 0, y: 0 });
        generateHeadPaths(1, toolHead2, head2Offset);
    }
  }

  // Footer
  gcode.push(``);
  gcode.push(`; ── FINISH ──`);
  gcode.push(`G0 X0 Y0 F${feedRateRapid} ; Return home`);
  gcode.push(`M5 ; Tool up`);
  gcode.push(`M18 ; Motors off`);
  gcode.push(`; End of G-code`);

  return gcode.join('\n');
}

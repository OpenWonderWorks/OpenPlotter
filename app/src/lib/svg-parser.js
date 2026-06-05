/**
 * OpenPlotter — SVG Parser
 * Parses SVG files and extracts linear paths for the cutting plotter.
 * Handles transforms and converts arcs, circles, rectangles, and curves to polylines.
 */

import { quadraticToPoints, cubicToPoints } from '../utils/bezier-to-lines.js';

// Simple 2D Matrix class for transforms
class Matrix {
  constructor(a = 1, b = 0, c = 0, d = 1, e = 0, f = 0) {
    this.a = a; this.b = b; this.c = c;
    this.d = d; this.e = e; this.f = f;
  }
  
  multiply(m) {
    return new Matrix(
      this.a * m.a + this.c * m.b,
      this.b * m.a + this.d * m.b,
      this.a * m.c + this.c * m.d,
      this.b * m.c + this.d * m.d,
      this.a * m.e + this.c * m.f + this.e,
      this.b * m.e + this.d * m.f + this.f
    );
  }
  
  translate(x, y) {
    return this.multiply(new Matrix(1, 0, 0, 1, x, y));
  }
  
  scale(sx, sy) {
    return this.multiply(new Matrix(sx, 0, 0, sy, 0, 0));
  }
  
  rotate(deg) {
    const rad = (deg * Math.PI) / 180;
    const cos = Math.cos(rad);
    const sin = Math.sin(rad);
    return this.multiply(new Matrix(cos, sin, -sin, cos, 0, 0));
  }
  
  apply(x, y) {
    return {
      x: this.a * x + this.c * y + this.e,
      y: this.b * x + this.d * y + this.f
    };
  }
}

/**
 * Parses transform attribute string (e.g., "translate(10, 20) scale(2)")
 */
function parseTransform(transformStr) {
  let mat = new Matrix();
  if (!transformStr) return mat;
  
  const regex = /([a-zA-Z]+)\s*\(([^)]+)\)/g;
  let match;
  
  while ((match = regex.exec(transformStr)) !== null) {
    const type = match[1].toLowerCase();
    const args = match[2].split(/[,\s]+/).map(parseFloat);
    
    if (type === 'translate') {
      mat = mat.translate(args[0], args[1] || 0);
    } else if (type === 'scale') {
      mat = mat.scale(args[0], args[1] !== undefined ? args[1] : args[0]);
    } else if (type === 'rotate') {
      mat = mat.rotate(args[0]); // Ignore optional cx, cy for simple matrix rotation or combine later
    } else if (type === 'matrix' && args.length === 6) {
      mat = mat.multiply(new Matrix(...args));
    }
  }
  
  return mat;
}

/**
 * Parses SVG path data string (d attribute) into raw commands
 */
function parsePathData(d) {
  const commands = [];
  const regex = /([a-df-zAz-DF-Z])|([-+]?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?)/g;
  let match;
  let currentCmd = null;
  let params = [];
  
  while ((match = regex.exec(d)) !== null) {
    if (match[1]) { // It's a command letter
      if (currentCmd) {
        commands.push({ code: currentCmd, params });
      }
      currentCmd = match[1];
      params = [];
    } else { // It's a number
      params.push(parseFloat(match[2]));
    }
  }
  if (currentCmd) {
    commands.push({ code: currentCmd, params });
  }
  return commands;
}

/**
 * Converts path commands into list of polylines
 */
function pathCommandsToPolylines(commands, tolerance = 0.15) {
  const polylines = [];
  let currentPath = [];
  let cx = 0, cy = 0; // Current position
  let sx = 0, sy = 0; // Start of current sub-path
  let px = 0, py = 0; // Last control point (for smooth curves)
  
  for (let i = 0; i < commands.length; i++) {
    const cmd = commands[i];
    const code = cmd.code;
    const p = cmd.params;
    let idx = 0;
    
    switch (code) {
      case 'M':
      case 'm':
        while (idx < p.length) {
          const x = p[idx++] + (code === 'm' ? cx : 0);
          const y = p[idx++] + (code === 'm' ? cy : 0);
          if (currentPath.length > 0) {
            polylines.push(currentPath);
          }
          cx = sx = x;
          cy = sy = y;
          currentPath = [{ x, y }];
          // For subsequent coordinates, treat as implicit 'L' or 'l'
          while (idx < p.length) {
            const lx = p[idx++] + (code === 'm' ? cx : 0);
            const ly = p[idx++] + (code === 'm' ? cy : 0);
            currentPath.push({ x: lx, y: ly });
            cx = lx;
            cy = ly;
          }
        }
        break;
        
      case 'L':
      case 'l':
        while (idx < p.length) {
          const x = p[idx++] + (code === 'l' ? cx : 0);
          const y = p[idx++] + (code === 'l' ? cy : 0);
          currentPath.push({ x, y });
          cx = x;
          cy = y;
        }
        break;
        
      case 'H':
      case 'h':
        while (idx < p.length) {
          const x = p[idx++] + (code === 'h' ? cx : 0);
          currentPath.push({ x, y: cy });
          cx = x;
        }
        break;
        
      case 'V':
      case 'v':
        while (idx < p.length) {
          const y = p[idx++] + (code === 'v' ? cy : 0);
          currentPath.push({ x: cx, y });
          cy = y;
        }
        break;
        
      case 'C':
      case 'c': // Cubic Bezier
        while (idx < p.length) {
          const isRel = code === 'c';
          const p1 = { x: p[idx++] + (isRel ? cx : 0), y: p[idx++] + (isRel ? cy : 0) };
          const p2 = { x: p[idx++] + (isRel ? cx : 0), y: p[idx++] + (isRel ? cy : 0) };
          const p3 = { x: p[idx++] + (isRel ? cx : 0), y: p[idx++] + (isRel ? cy : 0) };
          
          const pts = cubicToPoints({ x: cx, y: cy }, p1, p2, p3, tolerance);
          currentPath.push(...pts.slice(1));
          
          px = p2.x;
          py = p2.y;
          cx = p3.x;
          cy = p3.y;
        }
        break;
        
      case 'S':
      case 's': // Smooth Cubic Bezier
        while (idx < p.length) {
          const isRel = code === 's';
          const p2 = { x: p[idx++] + (isRel ? cx : 0), y: p[idx++] + (isRel ? cy : 0) };
          const p3 = { x: p[idx++] + (isRel ? cx : 0), y: p[idx++] + (isRel ? cy : 0) };
          
          // Determine reflection of control point px,py across cx,cy
          let p1;
          const prevCmd = commands[i-1] ? commands[i-1].code.toUpperCase() : '';
          if (prevCmd === 'C' || prevCmd === 'S') {
            p1 = { x: 2 * cx - px, y: 2 * cy - py };
          } else {
            p1 = { x: cx, y: cy };
          }
          
          const pts = cubicToPoints({ x: cx, y: cy }, p1, p2, p3, tolerance);
          currentPath.push(...pts.slice(1));
          
          px = p2.x;
          py = p2.y;
          cx = p3.x;
          cy = p3.y;
        }
        break;
        
      case 'Q':
      case 'q': // Quadratic Bezier
        while (idx < p.length) {
          const isRel = code === 'q';
          const p1 = { x: p[idx++] + (isRel ? cx : 0), y: p[idx++] + (isRel ? cy : 0) };
          const p2 = { x: p[idx++] + (isRel ? cx : 0), y: p[idx++] + (isRel ? cy : 0) };
          
          const pts = quadraticToPoints({ x: cx, y: cy }, p1, p2, tolerance);
          currentPath.push(...pts.slice(1));
          
          px = p1.x;
          py = p1.y;
          cx = p2.x;
          cy = p2.y;
        }
        break;
        
      case 'T':
      case 't': // Smooth Quadratic Bezier
        while (idx < p.length) {
          const isRel = code === 't';
          const p2 = { x: p[idx++] + (isRel ? cx : 0), y: p[idx++] + (isRel ? cy : 0) };
          
          let p1;
          const prevCmd = commands[i-1] ? commands[i-1].code.toUpperCase() : '';
          if (prevCmd === 'Q' || prevCmd === 'T') {
            p1 = { x: 2 * cx - px, y: 2 * cy - py };
          } else {
            p1 = { x: cx, y: cy };
          }
          
          const pts = quadraticToPoints({ x: cx, y: cy }, p1, p2, tolerance);
          currentPath.push(...pts.slice(1));
          
          px = p1.x;
          py = p1.y;
          cx = p2.x;
          cy = p2.y;
        }
        break;
        
      case 'Z':
      case 'z':
        if (currentPath.length > 0) {
          // Close path
          currentPath.push({ x: sx, y: sy });
          polylines.push(currentPath);
          currentPath = [];
        }
        cx = sx;
        cy = sy;
        break;
        
      default:
        // Ignore unsupported commands like arcs (A/a) for simplicity
        // or approximate them. Usually SVGs can be converted to bezier curves.
        break;
    }
  }
  
  if (currentPath.length > 1) {
    polylines.push(currentPath);
  }
  
  return polylines;
}

/**
 * Extracts and linearizes all shapes from an SVG document
 */
export function parseSVG(svgText, tolerance = 0.15) {
  const parser = new DOMParser();
  const doc = parser.parseFromString(svgText, 'image/svg+xml');
  
  // Root SVG element details
  const svgEl = doc.documentElement;
  const viewBoxStr = svgEl.getAttribute('viewBox');
  const widthAttr = svgEl.getAttribute('width');
  const heightAttr = svgEl.getAttribute('height');
  
  let width = 300; // default in mm or pixels
  let height = 300;
  
  if (widthAttr) width = parseFloat(widthAttr);
  if (heightAttr) height = parseFloat(heightAttr);
  
  let viewBox = null;
  if (viewBoxStr) {
    const parts = viewBoxStr.split(/[,\s]+/).map(parseFloat);
    if (parts.length === 4) {
      viewBox = { x: parts[0], y: parts[1], w: parts[2], h: parts[3] };
      if (!widthAttr) width = viewBox.w;
      if (!heightAttr) height = viewBox.h;
    }
  }
  
  const allPaths = [];
  
  function traverse(node, currentMatrix) {
    // Combine transforms
    const nodeTransform = node.getAttribute ? node.getAttribute('transform') : '';
    const m = currentMatrix.multiply(parseTransform(nodeTransform));
    
    const tagName = node.tagName ? node.tagName.toLowerCase() : '';
    
    if (tagName === 'path') {
      const d = node.getAttribute('d');
      if (d) {
        const cmds = parsePathData(d);
        const polys = pathCommandsToPolylines(cmds, tolerance);
        polys.forEach(poly => {
          const transformed = poly.map(pt => m.apply(pt.x, pt.y));
          allPaths.push(transformed);
        });
      }
    } else if (tagName === 'rect') {
      const x = parseFloat(node.getAttribute('x') || 0);
      const y = parseFloat(node.getAttribute('y') || 0);
      const w = parseFloat(node.getAttribute('width') || 0);
      const h = parseFloat(node.getAttribute('height') || 0);
      
      const poly = [
        m.apply(x, y),
        m.apply(x + w, y),
        m.apply(x + w, y + h),
        m.apply(x, y + h),
        m.apply(x, y)
      ];
      allPaths.push(poly);
    } else if (tagName === 'circle') {
      const cx = parseFloat(node.getAttribute('cx') || 0);
      const cy = parseFloat(node.getAttribute('cy') || 0);
      const r = parseFloat(node.getAttribute('r') || 0);
      
      const poly = [];
      const steps = Math.max(12, Math.floor(2 * Math.PI * r / tolerance / 4));
      for (let i = 0; i <= steps; i++) {
        const theta = (i / steps) * 2 * Math.PI;
        poly.push(m.apply(cx + r * Math.cos(theta), cy + r * Math.sin(theta)));
      }
      allPaths.push(poly);
    } else if (tagName === 'ellipse') {
      const cx = parseFloat(node.getAttribute('cx') || 0);
      const cy = parseFloat(node.getAttribute('cy') || 0);
      const rx = parseFloat(node.getAttribute('rx') || 0);
      const ry = parseFloat(node.getAttribute('ry') || 0);
      
      const poly = [];
      const steps = Math.max(12, Math.floor(2 * Math.PI * Math.max(rx, ry) / tolerance / 4));
      for (let i = 0; i <= steps; i++) {
        const theta = (i / steps) * 2 * Math.PI;
        poly.push(m.apply(cx + rx * Math.cos(theta), cy + ry * Math.sin(theta)));
      }
      allPaths.push(poly);
    } else if (tagName === 'line') {
      const x1 = parseFloat(node.getAttribute('x1') || 0);
      const y1 = parseFloat(node.getAttribute('y1') || 0);
      const x2 = parseFloat(node.getAttribute('x2') || 0);
      const y2 = parseFloat(node.getAttribute('y2') || 0);
      
      allPaths.push([
        m.apply(x1, y1),
        m.apply(x2, y2)
      ]);
    } else if (tagName === 'polyline' || tagName === 'polygon') {
      const pointsStr = node.getAttribute('points') || '';
      const parts = pointsStr.trim().split(/[,\s]+/).map(parseFloat);
      const poly = [];
      for (let i = 0; i < parts.length; i += 2) {
        if (!isNaN(parts[i]) && !isNaN(parts[i+1])) {
          poly.push(m.apply(parts[i], parts[i+1]));
        }
      }
      if (tagName === 'polygon' && poly.length > 0) {
        poly.push({ ...poly[0] });
      }
      if (poly.length > 0) {
        allPaths.push(poly);
      }
    }
    
    // Traverse children
    for (let i = 0; i < node.childNodes.length; i++) {
      traverse(node.childNodes[i], m);
    }
  }
  
  traverse(svgEl, new Matrix());
  
  // Normalize paths to mm based on viewBox / scaling
  // A cutting plotter usually operates in millimeters. 
  // Standard SVG resolution is often 96 DPI (1 inch = 25.4 mm, 1 mm = 3.7795 px)
  // Let's analyze SVG attributes and scale to a target bed size (e.g., 300x300mm).
  
  return {
    paths: allPaths,
    width,
    height,
    viewBox
  };
}

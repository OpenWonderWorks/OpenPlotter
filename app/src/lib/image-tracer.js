/**
 * OpenPlotter — Raster Image Tracer
 * Converts PNG/JPG images to vector paths for the cutting plotter.
 * Uses canvas-based contour detection (Suzuki-Abe inspired).
 */

/**
 * Load an image file and convert to grayscale pixel data
 */
function loadImageToCanvas(file) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    const url = URL.createObjectURL(file);

    img.onload = () => {
      const canvas = document.createElement('canvas');
      canvas.width = img.width;
      canvas.height = img.height;
      const ctx = canvas.getContext('2d');
      ctx.drawImage(img, 0, 0);
      URL.revokeObjectURL(url);

      const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
      resolve({ imageData, width: canvas.width, height: canvas.height, canvas });
    };

    img.onerror = () => {
      URL.revokeObjectURL(url);
      reject(new Error('Failed to load image'));
    };

    img.src = url;
  });
}

/**
 * Convert RGBA image data to grayscale array
 */
function toGrayscale(imageData) {
  const { data, width, height } = imageData;
  const gray = new Uint8Array(width * height);

  for (let i = 0; i < data.length; i += 4) {
    // Luminance formula
    gray[i / 4] = Math.round(0.299 * data[i] + 0.587 * data[i + 1] + 0.114 * data[i + 2]);
  }

  return gray;
}

/**
 * Apply binary threshold to grayscale image
 * Returns binary image: 1 = foreground (dark), 0 = background (light)
 */
function threshold(gray, width, height, thresh = 128, invert = false) {
  const binary = new Uint8Array(width * height);

  for (let i = 0; i < gray.length; i++) {
    if (invert) {
      binary[i] = gray[i] >= thresh ? 1 : 0;
    } else {
      binary[i] = gray[i] < thresh ? 1 : 0;
    }
  }

  return binary;
}

/**
 * Apply Gaussian blur to grayscale image (3x3 kernel)
 */
function gaussianBlur(gray, width, height) {
  const kernel = [1, 2, 1, 2, 4, 2, 1, 2, 1];
  const kSum = 16;
  const out = new Uint8Array(width * height);

  for (let y = 1; y < height - 1; y++) {
    for (let x = 1; x < width - 1; x++) {
      let sum = 0;
      let ki = 0;
      for (let ky = -1; ky <= 1; ky++) {
        for (let kx = -1; kx <= 1; kx++) {
          sum += gray[(y + ky) * width + (x + kx)] * kernel[ki++];
        }
      }
      out[y * width + x] = Math.round(sum / kSum);
    }
  }

  return out;
}

/**
 * Find contours using a simplified Suzuki-Abe border following algorithm.
 * Returns an array of contours, where each contour is an array of {x, y} points.
 */
function findContours(binary, width, height) {
  // Work on a padded copy so border pixels are handled
  const pad = 1;
  const w = width + 2 * pad;
  const h = height + 2 * pad;
  const img = new Int8Array(w * h);

  // Copy binary data into padded image
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      img[(y + pad) * w + (x + pad)] = binary[y * width + x];
    }
  }

  const contours = [];
  // 8-connectivity direction vectors (clockwise from right)
  const dx = [1, 1, 0, -1, -1, -1, 0, 1];
  const dy = [0, 1, 1, 1, 0, -1, -1, -1];

  // Track which pixels have been traced
  const visited = new Uint8Array(w * h);

  for (let y = 1; y < h - 1; y++) {
    for (let x = 1; x < w - 1; x++) {
      const idx = y * w + x;
      // Look for border pixels: foreground with at least one background neighbor
      if (img[idx] !== 1 || visited[idx]) continue;

      // Check if it's a border pixel
      let isBorder = false;
      for (let d = 0; d < 8; d++) {
        const ni = (y + dy[d]) * w + (x + dx[d]);
        if (img[ni] === 0) { isBorder = true; break; }
      }
      if (!isBorder) continue;

      // Trace contour using Moore-neighbor tracing
      const contour = [];
      let cx = x, cy = y;
      let startDir = 0;

      // Find first background neighbor to start
      for (let d = 0; d < 8; d++) {
        if (img[(cy + dy[d]) * w + (cx + dx[d])] === 0) {
          startDir = d;
          break;
        }
      }

      const startX = cx, startY = cy;
      let steps = 0;
      const maxSteps = w * h; // Safety limit

      do {
        contour.push({ x: cx - pad, y: cy - pad });
        visited[cy * w + cx] = 1;

        // Search clockwise from (startDir + 5) mod 8 (or the direction we came from + 1)
        let searchDir = (startDir + 5) % 8;
        let found = false;

        for (let i = 0; i < 8; i++) {
          const d = (searchDir + i) % 8;
          const nx = cx + dx[d];
          const ny = cy + dy[d];
          const ni = ny * w + nx;

          if (nx >= 0 && nx < w && ny >= 0 && ny < h && img[ni] === 1) {
            cx = nx;
            cy = ny;
            startDir = d;
            found = true;
            break;
          }
        }

        if (!found) break;
        steps++;
      } while ((cx !== startX || cy !== startY) && steps < maxSteps);

      // Close the contour
      if (contour.length > 2) {
        contour.push({ ...contour[0] });
        contours.push(contour);
      }
    }
  }

  return contours;
}

/**
 * Simplify a polyline using Ramer-Douglas-Peucker algorithm
 */
function simplifyPath(points, epsilon = 1.0) {
  if (points.length <= 2) return points;

  // Find the point with the maximum distance from the line segment (start-end)
  const start = points[0];
  const end = points[points.length - 1];

  let maxDist = 0;
  let maxIdx = 0;

  for (let i = 1; i < points.length - 1; i++) {
    const d = perpendicularDistance(points[i], start, end);
    if (d > maxDist) {
      maxDist = d;
      maxIdx = i;
    }
  }

  if (maxDist > epsilon) {
    const left = simplifyPath(points.slice(0, maxIdx + 1), epsilon);
    const right = simplifyPath(points.slice(maxIdx), epsilon);
    return left.slice(0, -1).concat(right);
  }

  return [start, end];
}

function perpendicularDistance(point, lineStart, lineEnd) {
  const dx = lineEnd.x - lineStart.x;
  const dy = lineEnd.y - lineStart.y;
  const lenSq = dx * dx + dy * dy;

  if (lenSq === 0) {
    const ex = point.x - lineStart.x;
    const ey = point.y - lineStart.y;
    return Math.sqrt(ex * ex + ey * ey);
  }

  const num = Math.abs(dy * point.x - dx * point.y + lineEnd.x * lineStart.y - lineEnd.y * lineStart.x);
  return num / Math.sqrt(lenSq);
}

/**
 * Smooth a path using Chaikin's corner-cutting algorithm
 */
function smoothPath(points, iterations = 2) {
  let result = points;

  for (let iter = 0; iter < iterations; iter++) {
    const smoothed = [];
    for (let i = 0; i < result.length - 1; i++) {
      const p0 = result[i];
      const p1 = result[i + 1];
      smoothed.push({
        x: 0.75 * p0.x + 0.25 * p1.x,
        y: 0.75 * p0.y + 0.25 * p1.y
      });
      smoothed.push({
        x: 0.25 * p0.x + 0.75 * p1.x,
        y: 0.25 * p0.y + 0.75 * p1.y
      });
    }
    result = smoothed;
  }

  return result;
}

/**
 * Main tracing function — takes a File object and options, returns paths
 * 
 * @param {File} file - The image file (PNG, JPG)
 * @param {Object} options - Tracing options
 * @param {number} options.threshold - Brightness threshold (0-255, default 128)
 * @param {boolean} options.invert - Invert black/white (default false)
 * @param {number} options.smoothing - Smoothing iterations (0-5, default 1)
 * @param {number} options.simplifyTolerance - RDP simplification tolerance (default 1.5)
 * @param {number} options.minPathLength - Minimum contour points to keep (default 5)
 * @param {boolean} options.blur - Apply Gaussian blur before threshold (default true)
 * @returns {Promise<{paths: Array, width: number, height: number, originalCanvas: HTMLCanvasElement}>}
 */
export async function traceImage(file, options = {}) {
  const {
    threshold: threshVal = 128,
    invert = false,
    smoothing = 1,
    simplifyTolerance = 1.5,
    minPathLength = 5,
    blur = true
  } = options;

  // Load image
  const { imageData, width, height, canvas } = await loadImageToCanvas(file);

  // Convert to grayscale
  let gray = toGrayscale(imageData);

  // Optional blur to reduce noise
  if (blur) {
    gray = gaussianBlur(gray, width, height);
  }

  // Binary threshold
  const binary = threshold(gray, width, height, threshVal, invert);

  // Find contours
  const rawContours = findContours(binary, width, height);

  // Post-process contours
  const paths = rawContours
    .filter(c => c.length >= minPathLength)
    .map(contour => {
      // Simplify
      let path = simplifyPath(contour, simplifyTolerance);
      // Smooth
      if (smoothing > 0) {
        path = smoothPath(path, smoothing);
      }
      return path;
    })
    .filter(p => p.length >= 3);

  return {
    paths,
    width,
    height,
    originalCanvas: canvas
  };
}

/**
 * Generate a preview canvas showing the traced paths overlaid on the original image
 */
export function renderTracePreview(originalCanvas, paths, targetCanvas) {
  const ctx = targetCanvas.getContext('2d');
  targetCanvas.width = originalCanvas.width;
  targetCanvas.height = originalCanvas.height;

  // Draw dimmed original
  ctx.globalAlpha = 0.3;
  ctx.drawImage(originalCanvas, 0, 0);
  ctx.globalAlpha = 1.0;

  // Draw traced paths
  ctx.strokeStyle = '#00e6f3';
  ctx.lineWidth = 1.5;
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';

  paths.forEach(path => {
    if (path.length < 2) return;
    ctx.beginPath();
    ctx.moveTo(path[0].x, path[0].y);
    for (let i = 1; i < path.length; i++) {
      ctx.lineTo(path[i].x, path[i].y);
    }
    ctx.stroke();
  });

  return targetCanvas;
}

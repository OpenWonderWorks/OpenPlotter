/**
 * OpenPlotter — Bézier Curve Linearization Utility
 * Converts cubic and quadratic Bézier curves into flat line segments
 */

/**
 * Calculates a point on a quadratic Bezier curve at t (0 to 1)
 */
export function getQuadraticPoint(p0, p1, p2, t) {
  const mt = 1 - t;
  return {
    x: mt * mt * p0.x + 2 * mt * t * p1.x + t * t * p2.x,
    y: mt * mt * p0.y + 2 * mt * t * p1.y + t * t * p2.y
  };
}

/**
 * Calculates a point on a cubic Bezier curve at t (0 to 1)
 */
export function getCubicPoint(p0, p1, p2, p3, t) {
  const mt = 1 - t;
  const mt2 = mt * mt;
  const mt3 = mt2 * mt;
  const t2 = t * t;
  const t3 = t2 * t;
  
  return {
    x: mt3 * p0.x + 3 * mt2 * t * p1.x + 3 * mt * t2 * p2.x + t3 * p3.x,
    y: mt3 * p0.y + 3 * mt2 * t * p1.y + 3 * mt * t2 * p2.y + t3 * p3.y
  };
}

/**
 * Linearizes a quadratic Bezier curve into points with adaptive subdivision
 * tolerance: maximum allowable distance from the chord to the curve
 */
export function quadraticToPoints(p0, p1, p2, tolerance = 0.15) {
  const points = [];
  
  function subdivide(t0, t1, pt0, pt1) {
    const tm = (t0 + t1) / 2;
    const ptm = getQuadraticPoint(p0, p1, p2, tm);
    
    // Calculate distance from ptm to the line segment pt0 -> pt1
    const dx = pt1.x - pt0.x;
    const dy = pt1.y - pt0.y;
    const lenSq = dx * dx + dy * dy;
    let distSq;
    
    if (lenSq < 1e-6) {
      const mx = ptm.x - pt0.x;
      const my = ptm.y - pt0.y;
      distSq = mx * mx + my * my;
    } else {
      const t = ((ptm.x - pt0.x) * dx + (ptm.y - pt0.y) * dy) / lenSq;
      const clampedT = Math.max(0, Math.min(1, t));
      const projX = pt0.x + clampedT * dx;
      const projY = pt0.y + clampedT * dy;
      const mx = ptm.x - projX;
      const my = ptm.y - projY;
      distSq = mx * mx + my * my;
    }
    
    if (distSq > tolerance * tolerance) {
      subdivide(t0, tm, pt0, ptm);
      subdivide(tm, t1, ptm, pt1);
    } else {
      points.push(pt1);
    }
  }
  
  points.push(p0);
  subdivide(0, 1, p0, p2);
  return points;
}

/**
 * Linearizes a cubic Bezier curve into points with adaptive subdivision
 */
export function cubicToPoints(p0, p1, p2, p3, tolerance = 0.15) {
  const points = [];
  
  function subdivide(t0, t1, pt0, pt1) {
    const tm = (t0 + t1) / 2;
    const ptm = getCubicPoint(p0, p1, p2, p3, tm);
    
    // Distance from midpoint to chord
    const dx = pt1.x - pt0.x;
    const dy = pt1.y - pt0.y;
    const lenSq = dx * dx + dy * dy;
    let distSq;
    
    if (lenSq < 1e-6) {
      const mx = ptm.x - pt0.x;
      const my = ptm.y - pt0.y;
      distSq = mx * mx + my * my;
    } else {
      const t = ((ptm.x - pt0.x) * dx + (ptm.y - pt0.y) * dy) / lenSq;
      const clampedT = Math.max(0, Math.min(1, t));
      const projX = pt0.x + clampedT * dx;
      const projY = pt0.y + clampedT * dy;
      const mx = ptm.x - projX;
      const my = ptm.y - projY;
      distSq = mx * mx + my * my;
    }
    
    if (distSq > tolerance * tolerance) {
      subdivide(t0, tm, pt0, ptm);
      subdivide(tm, t1, ptm, pt1);
    } else {
      points.push(pt1);
    }
  }
  
  points.push(p0);
  subdivide(0, 1, p0, p3);
  return points;
}

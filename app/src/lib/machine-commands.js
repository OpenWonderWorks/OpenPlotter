export function buildJogCommands(directionX, directionY, step, feedRate) {
  const distanceX = Number(directionX) * Number(step);
  const distanceY = Number(directionY) * Number(step);

  return [
    'G21',
    'G91',
    `G0 X${distanceX} Y${distanceY} F${feedRate}`,
    'G90'
  ];
}

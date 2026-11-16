const COMPILE_BOARD_PROFILES = Object.freeze({
  mega: 'BOARD_MEGA_RAMPS14',
  'mega-standalone': 'BOARD_MEGA_STANDALONE',
  esp32: 'BOARD_ESP32_STANDALONE'
});

function getCompileBoardProfile(boardType) {
  return Object.hasOwn(COMPILE_BOARD_PROFILES, boardType)
    ? COMPILE_BOARD_PROFILES[boardType]
    : null;
}

module.exports = { getCompileBoardProfile };

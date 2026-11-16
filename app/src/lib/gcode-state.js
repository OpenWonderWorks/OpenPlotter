export function invalidateGcode(state) {
  state.gcode = '';
}

export function isCommandQueueBusy(connection) {
  return Boolean(connection?.sending || connection?.waitingForOk || connection?.awaitingIdle);
}

export function canSendManualCommand(state) {
  return Boolean(state?.connected && state.connection && !isCommandQueueBusy(state.connection));
}

export function isCutReady(state) {
  return Boolean(
    state?.connected &&
    typeof state.gcode === 'string' &&
    state.gcode.trim() &&
    !isCommandQueueBusy(state.connection)
  );
}

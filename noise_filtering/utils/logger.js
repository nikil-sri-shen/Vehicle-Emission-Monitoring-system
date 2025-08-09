export function logInfo(msg) {
  console.log(`[INFO] ${new Date().toISOString()} - ${msg}`);
}

export function logError(msg) {
  console.error(`[ERROR] ${new Date().toISOString()} - ${msg}`);
}

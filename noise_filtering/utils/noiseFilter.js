export function filterNoise(data) {
  // Clamp values to reasonable ranges (example ranges)
  return {
    vehicleId: data.vehicleId,
    region: data.region,
    timestamp: data.timestamp,
    co2: clamp(data.co2, 250, 600),
    nox: clamp(data.nox, 0, 5),
    pm25: clamp(data.pm25, 0, 100),
  };
}

function clamp(value, min, max) {
  if (value < min) return min;
  if (value > max) return max;
  return value;
}

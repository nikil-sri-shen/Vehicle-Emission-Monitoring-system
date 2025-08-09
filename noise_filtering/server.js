import express from "express";
import { logInfo, logError } from "./utils/logger.js";
import { filterNoise } from "./utils/noiseFilter.js";
import { z } from "zod";
import fs from "fs";

const app = express();
const port = 5001;

app.use(express.json());

// ===== Schema Validation =====
const emissionSchema = z.object({
  vehicleId: z.string().min(1),
  region: z.string().min(1),
  timestamp: z.string(),
  co2: z.number().positive(),
  nox: z.number().positive(),
  pm25: z.number().positive(),
});

// ===== Storage =====
const saveData = (data) => {
  fs.appendFileSync("emissions.log", JSON.stringify(data) + "\n");
};

// ===== Routes =====
app.post("/submitEmission", (req, res) => {
  const timestamp = new Date().toISOString();
  logInfo(`📥 Emission data received at ${timestamp}`);

  const validation = emissionSchema.safeParse(req.body);
  if (!validation.success) {
    logError(
      `❌ Validation failed: ${JSON.stringify(validation.error.format())}`
    );
    return res.status(400).json({ error: "Invalid emission data" });
  }

  try {
    const cleanedData = filterNoise(validation.data);

    // Convert keys to uppercase for response (optional)
    const responseData = {
      vehicleId: cleanedData.vehicleId,
      region: cleanedData.region,
      timestamp: cleanedData.timestamp,
      CO2: cleanedData.co2,
      NOx: cleanedData.nox,
      HC: cleanedData.pm25,
    };

    saveData(responseData);

    logInfo(`🧹 Cleaned Data: ${JSON.stringify(responseData)}`);
    res.status(200).json({
      status: "Data received and cleaned successfully",
      cleanedEmission: responseData,
    });
  } catch (error) {
    logError(`❌ Processing error: ${error.message}`);
    res.status(500).json({ error: "Internal Server Error" });
  }
});

// ===== Start Server =====
app.listen(port, () => {
  logInfo(`🚀 Server running at http://localhost:${port}`);
});

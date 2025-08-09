#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <ctime>
#include <random>
#include <csignal>
#include <atomic>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

// ===== CONFIG =====
const std::string SERVER_URL = "http://localhost:5001/submitEmission";
const int INTERVAL_SECONDS = 5;
const std::vector<std::string> REGIONS = {"Region-1", "Region-2", "Region-3", "Region-4"};

// ===== Shutdown Flag =====
std::atomic<bool> running(true);

void signalHandler(int signum) {
    std::cout << "\n[INFO] Caught signal, shutting down..." << std::endl;
    running = false;
}

// ===== Timestamp =====
std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    char buf[30];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now_time));
    return std::string(buf);
}

// ===== Logging =====
void logInfo(const std::string &msg) {
    std::cout << "[INFO] " << currentTimestamp() << " - " << msg << std::endl;
}

void logError(const std::string &msg) {
    std::cerr << "[ERROR] " << currentTimestamp() << " - " << msg << std::endl;
}

// ===== Data Simulation =====
nlohmann::json generateEmissionData(const std::string& vehicleId, const std::string& region) {
    static thread_local std::default_random_engine eng(static_cast<unsigned>(std::time(nullptr)));
    static std::uniform_real_distribution<double> co2(300.0, 500.0);
    static std::uniform_real_distribution<double> nox(0.1, 2.5);
    static std::uniform_real_distribution<double> pm25(5.0, 50.0);

    return {
        {"vehicleId", vehicleId},
        {"region", region},
        {"timestamp", currentTimestamp()},
        {"co2", co2(eng)},
        {"nox", nox(eng)},
        {"pm25", pm25(eng)}
    };
}

// ===== HTTP POST =====
bool sendEmissionData(const nlohmann::json &data) {
    CURL *curl;
    CURLcode res;
    bool success = false;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl) {
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        std::string payload = data.dump();

        curl_easy_setopt(curl, CURLOPT_URL, SERVER_URL.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L); // Timeout in seconds

        logInfo("📤 Sending emission data: " + payload);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            logError("Failed to send data: " + std::string(curl_easy_strerror(res)));
        } else {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code == 200) {
                logInfo("✅ Data sent successfully");
                success = true;
            } else {
                logError("Server responded with status code: " + std::to_string(http_code));
            }
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();

    return success;
}

// ===== Generate random vehicle ID =====
std::string generateVehicleId(std::default_random_engine& eng) {
    static std::uniform_int_distribution<int> dist(1, 1000);
    return "VH-" + std::to_string(dist(eng));
}

// ===== MAIN =====
int main() {
    signal(SIGINT, signalHandler);
    logInfo("🚀 Emission client started");

    std::default_random_engine eng(static_cast<unsigned>(std::time(nullptr)));
    size_t regionIndex = 0;

    while (running) {
        // generate a random vehicle ID every time
        std::string vehicleId = generateVehicleId(eng);

        // cycle through regions
        std::string region = REGIONS[regionIndex];
        regionIndex = (regionIndex + 1) % REGIONS.size();

        auto data = generateEmissionData(vehicleId, region);
        if (!sendEmissionData(data)) {
            logError("Retrying in 3 seconds...");
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
        std::this_thread::sleep_for(std::chrono::seconds(INTERVAL_SECONDS));
    }

    logInfo("🛑 Client stopped");
    return 0;
}

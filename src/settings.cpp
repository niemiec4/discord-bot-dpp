#include "settings.h"

#include <dpp/json.h>

#include <fstream>
#include <iostream>
#include <sys/stat.h>

namespace settings {

namespace {
const std::string DATA_DIR = "data";
const std::string FILE_PATH = DATA_DIR + "/settings.json";

std::mutex mtx;
nlohmann::json db = nlohmann::json::object();
bool loaded = false;

void ensure_dir() {
#ifdef _WIN32
    _mkdir(DATA_DIR.c_str());
#else
    mkdir(DATA_DIR.c_str(), 0755);
#endif
}

std::string key(dpp::snowflake guild_id) {
    return std::to_string(static_cast<uint64_t>(guild_id));
}

guild_settings parse_guild(const nlohmann::json& j) {
    guild_settings s;
    s.log_channel_id      = dpp::snowflake{j.value("log_channel_id", uint64_t(0))};
    s.welcome_channel_id  = dpp::snowflake{j.value("welcome_channel_id", uint64_t(0))};
    s.welcome_message     = j.value("welcome_message", std::string());
    s.leveling_enabled    = j.value("leveling_enabled", true);
    s.levelup_channel_id  = dpp::snowflake{j.value("levelup_channel_id", uint64_t(0))};
    s.levelup_message     = j.value("levelup_message", std::string());

    if (j.contains("role_rewards") && j["role_rewards"].is_object()) {
        for (auto it = j["role_rewards"].begin(); it != j["role_rewards"].end(); ++it) {
            try {
                uint64_t level = std::stoull(it.key());
                s.role_rewards[level] = dpp::snowflake{it.value().get<uint64_t>()};
            } catch (...) {
                // ignore malformed entries
            }
        }
    }
    return s;
}

nlohmann::json dump_guild(const guild_settings& s) {
    nlohmann::json rewards = nlohmann::json::object();
    for (const auto& [level, role] : s.role_rewards) {
        rewards[std::to_string(level)] = static_cast<uint64_t>(role);
    }
    return nlohmann::json{
        {"log_channel_id",     static_cast<uint64_t>(s.log_channel_id)},
        {"welcome_channel_id", static_cast<uint64_t>(s.welcome_channel_id)},
        {"welcome_message",    s.welcome_message},
        {"leveling_enabled",   s.leveling_enabled},
        {"levelup_channel_id", static_cast<uint64_t>(s.levelup_channel_id)},
        {"levelup_message",    s.levelup_message},
        {"role_rewards",       rewards}
    };
}
} // namespace

void load() {
    std::lock_guard<std::mutex> lock(mtx);
    if (loaded) {
        return;
    }
    loaded = true;

    std::ifstream file(FILE_PATH);
    if (!file.is_open()) {
        return;
    }
    try {
        file >> db;
    } catch (const std::exception& e) {
        std::cerr << "[settings] Failed to parse " << FILE_PATH << ": " << e.what() << "\n";
        db = nlohmann::json::object();
    }
}

void save() {
    std::lock_guard<std::mutex> lock(mtx);
    ensure_dir();
    std::ofstream file(FILE_PATH, std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "[settings] Could not open " << FILE_PATH << " for writing.\n";
        return;
    }
    file << db.dump(4);
}

guild_settings get(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = db.find(key(guild_id));
    if (it == db.end() || !it->is_object()) {
        return guild_settings{};
    }
    return parse_guild(*it);
}

void set(dpp::snowflake guild_id, const guild_settings& s) {
    std::lock_guard<std::mutex> lock(mtx);
    db[key(guild_id)] = dump_guild(s);
    save();
}

} // namespace settings

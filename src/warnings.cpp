#include "warnings.h"

#include <dpp/json.h>

#include <fstream>
#include <iostream>
#include <sys/stat.h>

namespace warns {

namespace {
const std::string DATA_DIR = "data";
const std::string FILE_PATH = DATA_DIR + "/warnings.json";

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

std::string key(dpp::snowflake guild_id, dpp::snowflake user_id) {
    return std::to_string(static_cast<uint64_t>(guild_id)) + ":" +
           std::to_string(static_cast<uint64_t>(user_id));
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
        return; // no data yet, start empty
    }
    try {
        file >> db;
    } catch (const std::exception& e) {
        std::cerr << "[warnings] Failed to parse " << FILE_PATH << ": " << e.what() << "\n";
        db = nlohmann::json::object();
    }
}

void save() {
    std::lock_guard<std::mutex> lock(mtx);
    ensure_dir();
    std::ofstream file(FILE_PATH, std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "[warnings] Could not open " << FILE_PATH << " for writing.\n";
        return;
    }
    file << db.dump(4);
}

size_t add(dpp::snowflake guild_id, dpp::snowflake user_id,
           const std::string& reason, dpp::snowflake moderator) {
    std::lock_guard<std::mutex> lock(mtx);

    nlohmann::json& guild_arr = db[key(guild_id, user_id)];

    uint64_t next_id = 1;
    if (guild_arr.is_array()) {
        for (const auto& w : guild_arr) {
            next_id = std::max(next_id, w.value("id", uint64_t(0)) + 1);
        }
    } else {
        guild_arr = nlohmann::json::array();
    }

    guild_arr.push_back(nlohmann::json{
        {"id", next_id},
        {"moderator", static_cast<uint64_t>(moderator)},
        {"reason", reason},
        {"date", static_cast<uint64_t>(time(nullptr))}
    });

    save();
    return guild_arr.size();
}

std::vector<entry> get(dpp::snowflake guild_id, dpp::snowflake user_id) {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<entry> out;

    auto it = db.find(key(guild_id, user_id));
    if (it == db.end() || !it->is_array()) {
        return out;
    }
    for (const auto& w : *it) {
        out.push_back(entry{
            w.value("id", uint64_t(0)),
            dpp::snowflake{w.value("moderator", uint64_t(0))},
            w.value("reason", std::string()),
            static_cast<time_t>(w.value("date", uint64_t(0)))
        });
    }
    return out;
}

size_t count(dpp::snowflake guild_id, dpp::snowflake user_id) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = db.find(key(guild_id, user_id));
    if (it == db.end() || !it->is_array()) {
        return 0;
    }
    return it->size();
}

size_t clear(dpp::snowflake guild_id, dpp::snowflake user_id) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = db.find(key(guild_id, user_id));
    if (it == db.end()) {
        return 0;
    }
    size_t removed = it->is_array() ? it->size() : 0;
    db.erase(it);
    save();
    return removed;
}

} // namespace warns

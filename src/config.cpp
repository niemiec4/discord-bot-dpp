#include "config.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <cstdlib> // _putenv_s
#endif

namespace cfg {

void load_env(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[config] File \"" << filename << "\" not found. "
                  << "Continuing with existing environment variables.\n";
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Strip surrounding whitespace
        auto first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            continue; // empty line
        }
        auto last = line.find_last_not_of(" \t\r\n");
        line = line.substr(first, last - first + 1);

        if (line.empty() || line[0] == '#') {
            continue; // comment
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue; // not a KEY=VALUE line
        }

        std::string key   = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        // Trim whitespace around key and value
        auto trim = [](std::string& s) {
            auto b = s.find_first_not_of(" \t");
            auto e = s.find_last_not_of(" \t");
            if (b == std::string::npos) { s.clear(); } else { s = s.substr(b, e - b + 1); }
        };
        trim(key);
        trim(value);

        // Strip optional surrounding quotes
        if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                                  (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }

        // Do not clobber variables that are already set in the environment
        const char* existing = std::getenv(key.c_str());
        if (existing != nullptr) {
            continue;
        }

#ifdef _WIN32
        _putenv_s(key.c_str(), value.c_str());
#else
        setenv(key.c_str(), value.c_str(), 0);
#endif
    }
}

std::string get(const std::string& key, const std::string& fallback) {
    const char* value = std::getenv(key.c_str());
    return value != nullptr ? std::string(value) : fallback;
}

int64_t get_int(const std::string& key, int64_t fallback) {
    const char* value = std::getenv(key.c_str());
    if (value == nullptr) {
        return fallback;
    }
    try {
        std::stringstream ss(value);
        int64_t parsed = 0;
        ss >> parsed;
        if (ss.fail()) {
            return fallback;
        }
        return parsed;
    } catch (...) {
        return fallback;
    }
}

uint64_t get_id(const std::string& key, uint64_t fallback) {
    const char* value = std::getenv(key.c_str());
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    try {
        return static_cast<uint64_t>(std::stoull(value));
    } catch (...) {
        return fallback;
    }
}

} // namespace cfg

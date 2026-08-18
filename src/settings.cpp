#include "settings.h"

#include "db.h"

#include <dpp/json.h>

#include <iostream>

namespace settings {

namespace {

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

    s.warn_threshold        = j.value("warn_threshold", uint64_t(0));
    s.warn_action           = j.value("warn_action", 0);
    s.warn_timeout_minutes  = j.value("warn_timeout_minutes", uint64_t(60));
    s.booster_multiplier    = j.value("booster_multiplier", 1.0);

    if (j.contains("role_multipliers") && j["role_multipliers"].is_object()) {
        for (auto it = j["role_multipliers"].begin(); it != j["role_multipliers"].end(); ++it) {
            try {
                dpp::snowflake role_id{std::stoull(it.key())};
                s.role_multipliers[role_id] = it.value().get<double>();
            } catch (...) {
                // ignore malformed entries
            }
        }
    }

    s.autorole_id         = dpp::snowflake{j.value("autorole_id", uint64_t(0))};
    s.audit_channel_id    = dpp::snowflake{j.value("audit_channel_id", uint64_t(0))};
    return s;
}

nlohmann::json dump_guild(const guild_settings& s) {
    nlohmann::json rewards = nlohmann::json::object();
    for (const auto& [level, role] : s.role_rewards) {
        rewards[std::to_string(level)] = static_cast<uint64_t>(role);
    }
    nlohmann::json multipliers = nlohmann::json::object();
    for (const auto& [role, mult] : s.role_multipliers) {
        multipliers[std::to_string(static_cast<uint64_t>(role))] = mult;
    }
    return nlohmann::json{
        {"log_channel_id",       static_cast<uint64_t>(s.log_channel_id)},
        {"welcome_channel_id",   static_cast<uint64_t>(s.welcome_channel_id)},
        {"welcome_message",      s.welcome_message},
        {"leveling_enabled",     s.leveling_enabled},
        {"levelup_channel_id",   static_cast<uint64_t>(s.levelup_channel_id)},
        {"levelup_message",      s.levelup_message},
        {"role_rewards",         rewards},
        {"warn_threshold",       s.warn_threshold},
        {"warn_action",          s.warn_action},
        {"warn_timeout_minutes", s.warn_timeout_minutes},
        {"booster_multiplier",   s.booster_multiplier},
        {"role_multipliers",     multipliers},
        {"autorole_id",          static_cast<uint64_t>(s.autorole_id)},
        {"audit_channel_id",     static_cast<uint64_t>(s.audit_channel_id)}
    };
}
} // namespace

void load() {
    // Data now lives in SQLite (initialised by db::init in main).
}

void save() {
    // Data now lives in SQLite (initialised by db::init in main).
}

guild_settings get(dpp::snowflake guild_id) {
    db::stmt s("SELECT data FROM guild_settings WHERE guild_id = ?");
    s.bind(1, static_cast<int64_t>(guild_id));
    if (!s.step() || s.col_is_null(0)) {
        return guild_settings{};
    }
    try {
        nlohmann::json j = nlohmann::json::parse(s.col_text(0));
        return parse_guild(j);
    } catch (const std::exception& e) {
        std::cerr << "[settings] Failed to parse settings for guild " << static_cast<uint64_t>(guild_id)
                  << ": " << e.what() << "\n";
        return guild_settings{};
    }
}

void set(dpp::snowflake guild_id, const guild_settings& s) {
    db::stmt upsert("INSERT OR REPLACE INTO guild_settings (guild_id, data) VALUES (?,?)");
    upsert.bind(1, static_cast<int64_t>(guild_id));
    upsert.bind(2, dump_guild(s).dump());
    upsert.step();
}

} // namespace settings

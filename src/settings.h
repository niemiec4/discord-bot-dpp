#pragma once

#include <dpp/dpp.h>

#include <cstdint>
#include <map>
#include <mutex>
#include <string>

/**
 * @brief Per-guild configuration persisted to data/settings.json.
 * Every server can set its own log channel, welcome message, leveling
 * options and level-up role rewards.
 */
namespace settings {

struct guild_settings {
    dpp::snowflake log_channel_id{0};        // moderation logs
    dpp::snowflake welcome_channel_id{0};    // welcome messages
    std::string welcome_message;             // empty = default template
    bool leveling_enabled{true};             // XP gain on/off
    dpp::snowflake levelup_channel_id{0};    // 0 = same channel as the message
    std::string levelup_message;             // empty = default template
    std::map<uint64_t, dpp::snowflake> role_rewards; // level -> role to grant

    // Auto-punishment when a member reaches warn_threshold warnings.
    uint64_t warn_threshold{0};              // 0 = disabled
    int warn_action{0};                      // 0 = none, 1 = timeout, 2 = kick, 3 = ban
    uint64_t warn_timeout_minutes{60};       // duration used when action == timeout

    // XP multipliers.
    std::map<dpp::snowflake, double> role_multipliers; // role id -> XP multiplier
    double booster_multiplier{1.0};          // XP multiplier for server boosters (1.0 = off)
};

/** @brief Load settings from disk (idempotent). */
void load();

/** @brief Write the current settings to disk. */
void save();

/** @brief Get a copy of a guild's settings (defaults when unset). */
guild_settings get(dpp::snowflake guild_id);

/** @brief Replace a guild's settings and persist them. */
void set(dpp::snowflake guild_id, const guild_settings& s);

} // namespace settings

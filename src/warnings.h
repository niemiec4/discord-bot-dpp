#pragma once

#include <dpp/dpp.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

/**
 * @brief Persistent per-guild warning system backed by a JSON file
 * (data/warnings.json). Safe to call from any thread.
 */
namespace warns {

struct entry {
    uint64_t id;               // warning number within the guild
    dpp::snowflake moderator;  // who issued the warning
    std::string reason;        // why (empty when not provided)
    time_t date;               // unix timestamp
};

/** @brief Load warnings from disk (idempotent). */
void load();

/** @brief Save the current state to disk. */
void save();

/** @brief Add a warning for a user. @return the new warning count for the user. */
size_t add(dpp::snowflake guild_id, dpp::snowflake user_id,
           const std::string& reason, dpp::snowflake moderator);

/** @brief All warnings for a user, oldest first. */
std::vector<entry> get(dpp::snowflake guild_id, dpp::snowflake user_id);

/** @brief Number of warnings a user currently has. */
size_t count(dpp::snowflake guild_id, dpp::snowflake user_id);

/** @brief Remove every warning for a user. @return number of warnings removed. */
size_t clear(dpp::snowflake guild_id, dpp::snowflake user_id);

} // namespace warns

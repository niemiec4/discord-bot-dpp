#pragma once

#include <dpp/dpp.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief Per-guild XP/leveling system.
 *
 * XP is awarded for chatting (one gain per user per minute) and stored in
 * data/levels.json as `"guild_id:user_id" -> total_xp`. Levels are derived
 * from XP with a simple formula, so level-ups only need a save when XP changes.
 */
namespace levels {

struct user_level {
    uint64_t xp{0};
    uint64_t level{0};
    uint64_t xp_into_level{0};   // XP earned within the current level
    uint64_t xp_to_next{0};      // total XP needed to reach the next level
    double progress{0.0};        // 0.0 .. 1.0 towards the next level
};

/** @brief Load XP data from disk (idempotent). */
void load();

/** @brief Write XP data to disk. */
void save();

/** @brief Level for a given total XP amount. */
uint64_t level_from_xp(uint64_t xp);

/** @brief Total XP needed to reach (not exceed) the given level. */
uint64_t xp_for_level(uint64_t level);

/** @brief Snapshot of a user's progress on a guild. */
user_level get(dpp::snowflake guild_id, dpp::snowflake user_id);

/**
 * @brief Award XP to a user.
 * @return the user's new level (0 if they had no level yet).
 */
uint64_t add_xp(dpp::snowflake guild_id, dpp::snowflake user_id, uint64_t amount);

/** @brief Top `limit` users of a guild by total XP, highest first. */
std::vector<std::pair<dpp::snowflake, user_level>> top(dpp::snowflake guild_id, size_t limit);

/**
 * @brief Handle a chat message: award XP, detect level-ups, post the level-up
 * message and grant role rewards. Call from on_message_create.
 */
void handle_message(dpp::cluster& bot, const dpp::message& msg);

/** @brief Render a progress bar like "██████░░░░ 60%". */
std::string progress_bar(double fraction, int width = 10);

} // namespace levels

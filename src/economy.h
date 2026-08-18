#pragma once

#include <dpp/dpp.h>

#include <cstdint>
#include <ctime>

/**
 * @brief Per-guild wallet economy: balance, daily reward, work and gambling.
 * Stored in SQLite (data/bot.db).
 */
namespace economy {

struct account {
    int64_t wallet{0};
    time_t last_daily{0};
    time_t last_work{0};
};

/** @brief Current account of a user on a guild (defaults to zero). */
account get(dpp::snowflake guild_id, dpp::snowflake user_id);

/** @brief Add coins (can be negative). Resulting balance never goes below 0. */
void add_wallet(dpp::snowflake guild_id, dpp::snowflake user_id, int64_t amount);

/**
 * @brief Try to spend coins.
 * @return true and deducts when the balance covers the amount, false otherwise.
 */
bool spend(dpp::snowflake guild_id, dpp::snowflake user_id, int64_t amount);

/** @brief Seconds until the user may claim the daily reward again (0 = ready). */
time_t daily_remaining(dpp::snowflake guild_id, dpp::snowflake user_id);

/** @brief Claim the daily reward. @return false if already claimed today. */
bool claim_daily(dpp::snowflake guild_id, dpp::snowflake user_id);

/** @brief Seconds until the user may work again (0 = ready). */
time_t work_remaining(dpp::snowflake guild_id, dpp::snowflake user_id);

/** @brief Do a work shift and earn a random reward. @return 0 on cooldown. */
int64_t do_work(dpp::snowflake guild_id, dpp::snowflake user_id);

} // namespace economy

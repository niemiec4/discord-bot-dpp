#pragma once

#include <dpp/dpp.h>

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

/**
 * @brief Giveaway system. Giveaways persist in SQLite (including
 * participants) so they survive bot restarts. A periodic tick (see
 * tick()) ends giveaways whose time is up, picks winners and announces.
 */
namespace giveaways {

struct giveaway {
    int64_t id{0};
    dpp::snowflake guild_id{0};
    dpp::snowflake channel_id{0};
    dpp::snowflake message_id{0};
    std::string prize;
    int64_t winners{1};
    time_t end_time{0};
    dpp::snowflake host_id{0};
};

/** @brief Create a giveaway. @return giveaway id. */
int64_t create(dpp::snowflake guild_id, dpp::snowflake channel_id, dpp::snowflake message_id,
               const std::string& prize, int64_t winners, time_t end_time, dpp::snowflake host_id);

/** @brief Mark a giveaway as ended (no more entries allowed). */
void mark_ended(int64_t id);

/** @brief Delete a giveaway entirely (used by reroll/end commands). */
void remove(int64_t id);

/** @brief Add a participant. @return false if already joined. */
bool join(int64_t id, dpp::snowflake user_id);

/** @brief Whether a user already joined. */
bool has_joined(int64_t id, dpp::snowflake user_id);

/** @brief Participant ids of a giveaway. */
std::vector<dpp::snowflake> entries(int64_t id);

/** @brief Find a giveaway by its message id. @return false if unknown. */
bool find_by_message(dpp::snowflake message_id, giveaway& out);

/** @brief Whether the giveaway still exists and is not ended. */
bool is_active(int64_t id);

/** @brief End a giveaway now: pick winners and post the result. */
void end_now(dpp::cluster& bot, const giveaway& g);

/**
 * @brief Periodic sweep: end every giveaway whose time has passed.
 * Call from a timer (e.g. every 10 seconds).
 */
void tick(dpp::cluster& bot);

} // namespace giveaways

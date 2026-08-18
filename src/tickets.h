#pragma once

#include <dpp/dpp.h>

#include <cstdint>
#include <ctime>
#include <string>

/**
 * @brief Support tickets. Each ticket is a private text channel only the
 * opener (and the bot) can see; created via a button posted with
 * `/ticket setup`. Stored in SQLite.
 */
namespace tickets {

struct ticket {
    int64_t id{0};
    dpp::snowflake guild_id{0};
    dpp::snowflake channel_id{0};
    dpp::snowflake user_id{0};
    time_t created{0};
    bool open{true};
};

/** @brief Register a newly created ticket channel. @return ticket id. */
int64_t create(dpp::snowflake guild_id, dpp::snowflake channel_id, dpp::snowflake user_id);

/** @brief Whether the channel is a tracked ticket. */
bool is_ticket_channel(dpp::snowflake channel_id);

/** @brief Ticket info for a channel. @return false when not a ticket. */
bool get_by_channel(dpp::snowflake channel_id, ticket& out);

/**
 * @brief Whether the user already has an open ticket on the guild.
 * @param[out] channel_id the existing ticket channel when true.
 */
bool has_open_ticket(dpp::snowflake guild_id, dpp::snowflake user_id, dpp::snowflake& channel_id);

/** @brief Mark a ticket closed (kept in DB for record keeping). */
void close(dpp::snowflake channel_id);

} // namespace tickets

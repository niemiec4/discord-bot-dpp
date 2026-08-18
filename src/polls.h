#pragma once

#include <dpp/dpp.h>

#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief Slash command polls with per-option buttons. Votes are stored in
 * SQLite (one vote per user, toggleable). The author can end a poll early
 * with the end button.
 */
namespace polls {

/** @brief Create a poll. @return poll id. */
int64_t create(dpp::snowflake guild_id, dpp::snowflake channel_id, dpp::snowflake author_id,
               const std::string& question, const std::vector<std::string>& options);

/** @brief Store the message id once the poll message is posted. */
void set_message_id(int64_t id, dpp::snowflake message_id);

/** @brief Look up a poll by message id. @return false if unknown. */
bool find_by_message(dpp::snowflake message_id, int64_t& id);

/** @brief Poll metadata. @return false if unknown. */
bool info(int64_t id, std::string& question, std::vector<std::string>& options, bool& ended);

/** @brief Whether the poll exists and is still open. */
bool is_open(int64_t id);

/** @brief Author of the poll (0 when unknown). */
dpp::snowflake author_id(int64_t id);

/** @brief Whether a user already voted. */
bool has_voted(int64_t id, dpp::snowflake user_id);

/**
 * @brief Register a vote (toggles when the user voted before).
 * @return new vote count for the chosen option, or -1 when the poll is closed.
 */
int64_t vote(int64_t id, dpp::snowflake user_id, int option_index);

/** @brief Vote totals per option, in option order. */
std::vector<int64_t> counts(int64_t id);

/** @brief End the poll (if still open). @return true when it was ended by this call. */
bool end(int64_t id);

} // namespace polls

#pragma once

#include <dpp/dpp.h>

#include <ctime>
#include <string>

/**
 * @brief In-memory snipe cache: remembers recent messages per channel so a
 * deleted message can be shown with /snipe. Data is lost on restart
 * (by design — snipes are ephemeral by nature).
 */
namespace snipes {

struct entry {
    std::string content;
    std::string author;
    std::string author_id;
    std::string avatar;
    std::string attachment; // first image attachment url, if any
    time_t time{0};
};

/** @brief Remember a message. Call from on_message_create. */
void track(const dpp::message& msg);

/** @brief On message delete: store the deleted message as a snipe. */
void on_delete(dpp::snowflake channel_id, dpp::snowflake message_id);

/** @brief Pop the latest snipe of a channel. @return false when nothing to show. */
bool get(dpp::snowflake channel_id, entry& out);

} // namespace snipes

#pragma once

#include <dpp/dpp.h>

#include <string>

/**
 * @brief Full server audit log: moderation events, message edits/deletes,
 * member joins/leaves, bans and channel changes. Every server picks its own
 * channel with `/settings auditlog`; nothing is sent when unset.
 */
namespace audit {

/**
 * @brief Send an audit embed to the guild's audit channel (no-op when unset).
 * The embed gets a consistent footer and timestamp.
 */
void send(dpp::cluster& bot, dpp::snowflake guild_id, const std::string& title,
          const std::string& description, uint32_t color, const std::string& extra_field_name = "",
          const std::string& extra_field_value = "");

/** @brief Audit channel for a guild (0 when unset). */
dpp::snowflake channel_for(dpp::snowflake guild_id);

} // namespace audit

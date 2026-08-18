#pragma once

#include <dpp/dpp.h>

#include <string>
#include <utility>
#include <vector>

/**
 * @brief Per-guild custom slash commands (``/cc``). Custom commands are not
 * registered with Discord — they are matched at dispatch time by name.
 * Stored in SQLite.
 */
namespace custom_cmds {

/** @brief Add or overwrite a custom command. @return false when invalid. */
bool set(dpp::snowflake guild_id, const std::string& name, const std::string& response);

/** @brief Remove a custom command. @return false if it did not exist. */
bool remove(dpp::snowflake guild_id, const std::string& name);

/** @brief Look up a custom command. @return false if it does not exist. */
bool get(dpp::snowflake guild_id, const std::string& name, std::string& response);

/** @brief All custom commands of a guild (name, response). */
std::vector<std::pair<std::string, std::string>> list(dpp::snowflake guild_id);

} // namespace custom_cmds

#pragma once

#include <dpp/dpp.h>

#include <cstdint>
#include <string>

/**
 * @brief Shared helpers: embeds, permission checks, parameter extraction
 * and small formatting utilities used across all command modules.
 */
namespace util {

// Embed color palette (embed colors are plain uint32_t RGB values in DPP 10)
static constexpr uint32_t COLOR_PRIMARY{0x5865F2}; // Discord blurple
static constexpr uint32_t COLOR_SUCCESS{0x57F287}; // green
static constexpr uint32_t COLOR_WARNING{0xFEE75C}; // yellow
static constexpr uint32_t COLOR_ERROR{0xED4245};   // red

/**
 * @brief Build a nicely styled embed with a footer (bot name + icon)
 * and a timestamp, ready to be filled with fields.
 */
dpp::embed base_embed(const dpp::cluster& bot,
                      const std::string& title,
                      uint32_t color = COLOR_PRIMARY,
                      const std::string& description = "");

/** @brief Red error embed wrapped in an (ephemeral by default) message. */
dpp::message error(const std::string& text, bool ephemeral = true);

/** @brief Green success embed wrapped in an (ephemeral by default) message. */
dpp::message success(const std::string& text, bool ephemeral = true);

/** @brief Yellow warning embed wrapped in an (ephemeral by default) message. */
dpp::message warning(const std::string& text, bool ephemeral = true);

/**
 * @brief Check that the command author has the required permission.
 * Replies with an ephemeral error message when the check fails.
 * @return true if the caller may proceed.
 */
bool require_permission(const dpp::slashcommand_t& event, dpp::permission required,
                        const std::string& perm_name);

/**
 * @brief Check that the bot itself has the required permission on the guild.
 * Replies with an ephemeral error message when the check fails.
 * @return true if the bot may proceed.
 */
bool require_bot_permission(dpp::cluster& bot, const dpp::slashcommand_t& event,
                            dpp::permission required, const std::string& perm_name);

/** @brief Extract a user option as a snowflake (0 when missing/invalid). */
dpp::snowflake get_user(const dpp::slashcommand_t& event, const std::string& name);

/** @brief Extract a string option (empty when missing). */
std::string get_string(const dpp::slashcommand_t& event, const std::string& name);

/** @brief Extract an integer option (fallback when missing/invalid). */
int64_t get_integer(const dpp::slashcommand_t& event, const std::string& name, int64_t fallback = 0);

/** @brief Extract a boolean option (false when missing). */
bool get_boolean(const dpp::slashcommand_t& event, const std::string& name);

/** @brief Position of the member's highest role (0 if the member has none). */
int64_t highest_role_position(const dpp::guild& guild, const dpp::guild_member& member);

/**
 * @brief Role hierarchy check for moderation actions.
 * @return true if actor can moderate target (target is not the owner and
 *         actor is the owner or has a strictly higher top role).
 */
bool hierarchy_allows(const dpp::guild& guild, const dpp::guild_member& actor,
                      const dpp::guild_member& target);

/** @brief Send an embed to the configured moderation log channel (no-op if unset). */
void send_log(dpp::cluster& bot, dpp::snowflake guild_id, const dpp::embed& embed);

/**
 * @brief Build a polished welcome embed for a new member.
 * Used both for real joins and for the `/settings welcomepreview` command,
 * so previews always match the real message.
 *
 * @param bot          cluster (for the bot avatar in the footer)
 * @param guild_name   server name
 * @param member       the new member
 * @param text         welcome text ({user}, {server} already replaced)
 * @param member_count current member count (shown as "Member #N")
 */
dpp::embed welcome_embed(const dpp::cluster& bot, const std::string& guild_name,
                         const dpp::guild_member& member, const std::string& text,
                         uint64_t member_count);

/** @brief Average gateway latency of all shards (ms). */
double gateway_ping(dpp::cluster& bot);

/**
 * @brief The "about me" embed (uptime, servers, latency, version).
 * Used by `/botinfo` and when the bot is mentioned.
 */
dpp::embed bot_info_embed(dpp::cluster& bot);

/** @brief Escape Discord markdown in user-provided text. */
std::string escape(const std::string& text);

/** @brief Format a duration in seconds as e.g. "2d 3h 4m 5s". */
std::string format_duration(uint64_t seconds);

/** @brief Discord relative timestamp ("<t:1234567890:R>"). */
std::string rel_time(time_t t);

/** @brief Discord full timestamp ("<t:1234567890:f>"). */
std::string full_time(time_t t);

/**
 * @brief Parse a human readable duration like "90s", "30m", "2h", "1d",
 * or combined "1h30m" into seconds.
 * @return seconds, or 0 if the input is invalid.
 */
uint64_t parse_duration(const std::string& input);

} // namespace util

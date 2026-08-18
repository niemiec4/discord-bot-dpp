#include "commands.h"

#include "settings.h"
#include "warnings.h"

#include <algorithm>
#include <ctime>
#include <string>

namespace cmd {

namespace {

constexpr uint64_t MAX_BULK_AGE = 14ULL * 86400ULL; // Discord refuses bulk deletes older than 14 days

dpp::task<void> cmd_kick(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!util::require_permission(event, dpp::p_kick_members, "Kick Members")) co_return;
    if (!util::require_bot_permission(bot, event, dpp::p_kick_members, "Kick Members")) co_return;

    dpp::snowflake target_id = util::get_user(event, "user");
    if (!target_id) { co_await event.co_reply(util::error("You must specify a user to kick.")); co_return; }
    std::string reason = util::get_string(event, "reason");

    const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) { co_await event.co_reply(util::error("Server not found in cache.")); co_return; }

    if (target_id == event.command.usr.id) { co_await event.co_reply(util::error("You cannot kick yourself.")); co_return; }
    if (target_id == bot.me.id)             { co_await event.co_reply(util::error("I cannot kick myself.")); co_return; }
    if (target_id == guild->owner_id)       { co_await event.co_reply(util::error("You cannot kick the server owner.")); co_return; }

    dpp::guild_member target_member = dpp::find_guild_member(guild->id, target_id);
    if (!target_member.user_id) {
        dpp::confirmation_callback_t fetch = co_await bot.co_guild_get_member(guild->id, target_id);
        if (fetch.is_error()) { co_await event.co_reply(util::error("That user is not a member of this server.")); co_return; }
        target_member = std::get<dpp::guild_member>(fetch.value);
    }

    if (!util::hierarchy_allows(*guild, event.command.member, target_member)) {
        co_await event.co_reply(util::error("You cannot moderate that user — their top role is equal to or higher than yours."));
        co_return;
    }

    std::string name = util::escape(target_member.get_nickname());

    co_await event.co_thinking(true);
    dpp::confirmation_callback_t res = co_await bot.co_guild_member_delete(guild->id, target_id);
    if (res.is_error()) {
        co_await event.co_edit_original_response(util::error("Failed to kick **" + name + "**: " + res.get_error().human_readable));
        co_return;
    }

    dpp::embed e = util::base_embed(bot, "Member Kicked", util::COLOR_WARNING)
        .set_description("**" + name + "** has been kicked from the server.")
        .add_field("Moderator", event.command.usr.get_mention(), true)
        .add_field("Reason", reason.empty() ? "No reason provided" : util::escape(reason), true);

    util::send_log(bot, guild->id, e);
    co_await event.co_edit_original_response(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_ban(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!util::require_permission(event, dpp::p_ban_members, "Ban Members")) co_return;
    if (!util::require_bot_permission(bot, event, dpp::p_ban_members, "Ban Members")) co_return;

    dpp::snowflake target_id = util::get_user(event, "user");
    if (!target_id) { co_await event.co_reply(util::error("You must specify a user to ban.")); co_return; }
    std::string reason = util::get_string(event, "reason");
    int64_t days = std::clamp<int64_t>(util::get_integer(event, "days", 0), 0, 7);

    const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) { co_await event.co_reply(util::error("Server not found in cache.")); co_return; }

    if (target_id == event.command.usr.id) { co_await event.co_reply(util::error("You cannot ban yourself.")); co_return; }
    if (target_id == bot.me.id)             { co_await event.co_reply(util::error("I cannot ban myself.")); co_return; }
    if (target_id == guild->owner_id)       { co_await event.co_reply(util::error("You cannot ban the server owner.")); co_return; }

    dpp::guild_member target_member = dpp::find_guild_member(guild->id, target_id);
    if (!target_member.user_id) {
        dpp::confirmation_callback_t fetch = co_await bot.co_guild_get_member(guild->id, target_id);
        if (fetch.is_error()) { co_await event.co_reply(util::error("That user is not a member of this server.")); co_return; }
        target_member = std::get<dpp::guild_member>(fetch.value);
    }

    if (!util::hierarchy_allows(*guild, event.command.member, target_member)) {
        co_await event.co_reply(util::error("You cannot moderate that user — their top role is equal to or higher than yours."));
        co_return;
    }

    std::string name = util::escape(target_member.get_nickname());

    co_await event.co_thinking(true);
    dpp::confirmation_callback_t res = co_await bot.co_guild_ban_add(guild->id, target_id,
                                                                     static_cast<uint32_t>(days * 86400));
    if (res.is_error()) {
        co_await event.co_edit_original_response(util::error("Failed to ban **" + name + "**: " + res.get_error().human_readable));
        co_return;
    }

    dpp::embed e = util::base_embed(bot, "Member Banned", util::COLOR_ERROR)
        .set_description("**" + name + "** has been banned from the server.")
        .add_field("Moderator", event.command.usr.get_mention(), true)
        .add_field("Reason", reason.empty() ? "No reason provided" : util::escape(reason), true);
    if (days > 0) {
        e.add_field("Messages deleted", std::to_string(days) + " day(s) of messages", true);
    }

    util::send_log(bot, guild->id, e);
    co_await event.co_edit_original_response(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_unban(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!util::require_permission(event, dpp::p_ban_members, "Ban Members")) co_return;
    if (!util::require_bot_permission(bot, event, dpp::p_ban_members, "Ban Members")) co_return;

    std::string id_str = util::get_string(event, "user_id");
    if (id_str.empty()) { co_await event.co_reply(util::error("You must provide the user's ID.")); co_return; }

    dpp::snowflake target_id{id_str};
    if (!target_id) { co_await event.co_reply(util::error("That doesn't look like a valid user ID.")); co_return; }

    const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) { co_await event.co_reply(util::error("Server not found in cache.")); co_return; }

    co_await event.co_thinking(true);
    dpp::confirmation_callback_t res = co_await bot.co_guild_ban_delete(guild->id, target_id);
    if (res.is_error()) {
        co_await event.co_edit_original_response(util::error("Failed to unban that user: " + res.get_error().human_readable));
        co_return;
    }

    dpp::embed e = util::base_embed(bot, "User Unbanned", util::COLOR_SUCCESS)
        .set_description("<@" + std::to_string(static_cast<uint64_t>(target_id)) + "> has been unbanned.")
        .add_field("Moderator", event.command.usr.get_mention(), true);

    util::send_log(bot, guild->id, e);
    co_await event.co_edit_original_response(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_timeout(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!util::require_permission(event, dpp::p_moderate_members, "Moderate Members")) co_return;
    if (!util::require_bot_permission(bot, event, dpp::p_moderate_members, "Moderate Members")) co_return;

    dpp::snowflake target_id = util::get_user(event, "user");
    if (!target_id) { co_await event.co_reply(util::error("You must specify a user to timeout.")); co_return; }

    uint64_t seconds = util::parse_duration(util::get_string(event, "duration"));
    if (seconds == 0) {
        co_await event.co_reply(util::error("Invalid duration. Examples: `30m`, `2h`, `1d`, `1h30m` (max 28 days)."));
        co_return;
    }
    std::string reason = util::get_string(event, "reason");

    const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) { co_await event.co_reply(util::error("Server not found in cache.")); co_return; }

    if (target_id == event.command.usr.id) { co_await event.co_reply(util::error("You cannot timeout yourself.")); co_return; }
    if (target_id == bot.me.id)             { co_await event.co_reply(util::error("I cannot timeout myself.")); co_return; }
    if (target_id == guild->owner_id)       { co_await event.co_reply(util::error("You cannot timeout the server owner.")); co_return; }

    dpp::guild_member target_member = dpp::find_guild_member(guild->id, target_id);
    if (!target_member.user_id) {
        dpp::confirmation_callback_t fetch = co_await bot.co_guild_get_member(guild->id, target_id);
        if (fetch.is_error()) { co_await event.co_reply(util::error("That user is not a member of this server.")); co_return; }
        target_member = std::get<dpp::guild_member>(fetch.value);
    }

    if (!util::hierarchy_allows(*guild, event.command.member, target_member)) {
        co_await event.co_reply(util::error("You cannot moderate that user — their top role is equal to or higher than yours."));
        co_return;
    }

    std::string name = util::escape(target_member.get_nickname());
    time_t until = time(nullptr) + static_cast<time_t>(seconds);

    co_await event.co_thinking(true);
    dpp::confirmation_callback_t res = co_await bot.co_guild_member_timeout(guild->id, target_id, until);
    if (res.is_error()) {
        co_await event.co_edit_original_response(util::error("Failed to timeout **" + name + "**: " + res.get_error().human_readable));
        co_return;
    }

    dpp::embed e = util::base_embed(bot, "User Timed Out", util::COLOR_WARNING)
        .set_description("**" + name + "** has been timed out.")
        .add_field("Duration", util::format_duration(seconds), true)
        .add_field("Expires", util::rel_time(until), true)
        .add_field("Moderator", event.command.usr.get_mention(), true)
        .add_field("Reason", reason.empty() ? "No reason provided" : util::escape(reason), true);

    util::send_log(bot, guild->id, e);
    co_await event.co_edit_original_response(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_purge(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!util::require_permission(event, dpp::p_manage_messages, "Manage Messages")) co_return;
    if (!util::require_bot_permission(bot, event, dpp::p_manage_messages, "Manage Messages")) co_return;

    int64_t amount = util::get_integer(event, "amount", 1);
    amount = std::clamp<int64_t>(amount, 1, 100);
    dpp::snowflake author = util::get_user(event, "user");

    dpp::snowflake channel_id = event.command.channel_id;
    if (!channel_id) { co_await event.co_reply(util::error("This command can only be used in a channel.")); co_return; }

    co_await event.co_thinking(true);

    std::vector<dpp::snowflake> ids;
    dpp::snowflake before = 0;
    int pages = 0;

    // Bulk delete only works on messages younger than 14 days; keep scanning
    // backwards until we have enough recent messages or the channel is exhausted.
    while (static_cast<int64_t>(ids.size()) < amount) {
        if (++pages > 10) {
            break; // safety cap: at most 1000 messages scanned
        }

        dpp::confirmation_callback_t res = co_await bot.co_messages_get(channel_id, 0, before, 0, 100);
        if (res.is_error()) {
            co_await event.co_edit_original_response(util::error("Failed to fetch messages: " + res.get_error().human_readable));
            co_return;
        }

        const auto& messages = std::get<dpp::message_map>(res.value);
        if (messages.empty()) {
            break;
        }

        time_t now = time(nullptr);
        dpp::snowflake oldest = 0;
        for (const auto& [message_id, msg] : messages) {
            if (message_id == 0) continue;
            if (oldest == 0 || message_id < oldest) {
                oldest = message_id; // snowflake ids grow with time, so min id == oldest
            }
            if (now - static_cast<time_t>(msg.sent) > static_cast<time_t>(MAX_BULK_AGE)) continue;
            if (author && msg.author.id != author) continue;
            ids.push_back(message_id);
            if (static_cast<int64_t>(ids.size()) >= amount) {
                break;
            }
        }

        if (messages.size() < 100) {
            break; // no older messages exist
        }
        before = oldest;
    }

    if (ids.empty()) {
        co_await event.co_edit_original_response(util::error("No messages matched the filter."));
        co_return;
    }

    dpp::confirmation_callback_t res;
    if (ids.size() == 1) {
        res = co_await bot.co_message_delete(ids.front(), channel_id);
    } else {
        res = co_await bot.co_message_delete_bulk(ids, channel_id);
    }
    if (res.is_error()) {
        co_await event.co_edit_original_response(util::error("Failed to delete messages: " + res.get_error().human_readable));
        co_return;
    }

    dpp::embed e = util::base_embed(bot, "Messages Purged", util::COLOR_SUCCESS)
        .set_description("Deleted **" + std::to_string(ids.size()) + "** message(s).")
        .add_field("Channel", "<#" + std::to_string(static_cast<uint64_t>(channel_id)) + ">", true)
        .add_field("Moderator", event.command.usr.get_mention(), true);
    if (author) {
        e.add_field("Filter", "Only from <@" + std::to_string(static_cast<uint64_t>(author)) + ">", true);
    }

    util::send_log(bot, event.command.guild_id, e);
    co_await event.co_edit_original_response(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_warn(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!util::require_permission(event, dpp::p_moderate_members, "Moderate Members")) co_return;

    dpp::snowflake target_id = util::get_user(event, "user");
    if (!target_id) { co_await event.co_reply(util::error("You must specify a user to warn.")); co_return; }
    std::string reason = util::get_string(event, "reason");

    if (target_id == event.command.usr.id) { co_await event.co_reply(util::error("You cannot warn yourself.")); co_return; }
    if (target_id == bot.me.id)             { co_await event.co_reply(util::error("I cannot warn myself.")); co_return; }

    const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) { co_await event.co_reply(util::error("Server not found in cache.")); co_return; }

    dpp::guild_member target_member = dpp::find_guild_member(guild->id, target_id);
    if (!target_member.user_id) {
        dpp::confirmation_callback_t fetch = co_await bot.co_guild_get_member(guild->id, target_id);
        if (fetch.is_error()) { co_await event.co_reply(util::error("That user is not a member of this server.")); co_return; }
        target_member = std::get<dpp::guild_member>(fetch.value);
    }

    if (!util::hierarchy_allows(*guild, event.command.member, target_member)) {
        co_await event.co_reply(util::error("You cannot moderate that user — their top role is equal to or higher than yours."));
        co_return;
    }

    std::string name = util::escape(target_member.get_nickname());
    size_t total = warns::add(guild->id, target_id, reason, event.command.usr.id);

    // Auto-punishment: when the member reaches the configured warning threshold.
    settings::guild_settings s = settings::get(guild->id);
    std::string punishment;
    if (s.warn_threshold > 0 && total >= s.warn_threshold) {
        switch (s.warn_action) {
            case 1: { // timeout
                uint64_t minutes = s.warn_timeout_minutes > 0 ? s.warn_timeout_minutes : 60;
                time_t until = time(nullptr) + static_cast<time_t>(minutes * 60);
                dpp::confirmation_callback_t res = co_await bot.co_guild_member_timeout(guild->id, target_id, until);
                punishment = res.is_error()
                    ? "auto-timeout failed (" + res.get_error().human_readable + ")"
                    : "auto-timed out for " + std::to_string(minutes) + " minute(s)";
                break;
            }
            case 2: { // kick
                dpp::confirmation_callback_t res = co_await bot.co_guild_member_delete(guild->id, target_id);
                punishment = res.is_error()
                    ? "auto-kick failed (" + res.get_error().human_readable + ")"
                    : "auto-kicked";
                break;
            }
            case 3: { // ban
                dpp::confirmation_callback_t res = co_await bot.co_guild_ban_add(guild->id, target_id);
                punishment = res.is_error()
                    ? "auto-ban failed (" + res.get_error().human_readable + ")"
                    : "auto-banned";
                break;
            }
            default:
                break;
        }
    }

    dpp::embed e = util::base_embed(bot, "User Warned", util::COLOR_WARNING)
        .set_description("**" + name + "** has been warned.")
        .add_field("Moderator", event.command.usr.get_mention(), true)
        .add_field("Reason", reason.empty() ? "No reason provided" : util::escape(reason), true)
        .add_field("Total warnings", std::to_string(total), true);
    if (!punishment.empty()) {
        e.add_field("Auto-punishment", punishment);
    }

    util::send_log(bot, guild->id, e);
    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_warnings(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!util::require_permission(event, dpp::p_moderate_members, "Moderate Members")) co_return;

    dpp::snowflake target_id = util::get_user(event, "user");
    if (!target_id) {
        target_id = event.command.usr.id;
    }
    const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) { co_await event.co_reply(util::error("Server not found in cache.")); co_return; }

    const dpp::user* user = dpp::find_user(target_id);
    std::string name = user ? util::escape(user->format_username())
                            : "<@" + std::to_string(static_cast<uint64_t>(target_id)) + ">";
    std::vector<warns::entry> entries = warns::get(guild->id, target_id);

    dpp::embed e = util::base_embed(bot, "Warnings", util::COLOR_PRIMARY,
                                    "**" + name + "** has **" + std::to_string(entries.size()) +
                                    "** warning(s).");

    if (entries.empty()) {
        e.set_description(e.description + "\n\nNo warnings — clean record.");
    } else {
        std::string fields;
        for (const auto& w : entries) {
            fields += "`#" + std::to_string(w.id) + "` " +
                      (w.reason.empty() ? "*No reason provided*" : util::escape(w.reason)) +
                      " — by <@" + std::to_string(static_cast<uint64_t>(w.moderator)) + "> · " +
                      util::rel_time(w.date) + "\n";
        }
        e.add_field("History", fields);
    }

    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_clearwarns(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!util::require_permission(event, dpp::p_moderate_members, "Moderate Members")) co_return;

    dpp::snowflake target_id = util::get_user(event, "user");
    if (!target_id) { co_await event.co_reply(util::error("You must specify a user.")); co_return; }

    const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) { co_await event.co_reply(util::error("Server not found in cache.")); co_return; }

    size_t removed = warns::clear(guild->id, target_id);
    if (removed == 0) {
        co_await event.co_reply(util::warning("That user has no warnings."));
        co_return;
    }

    dpp::embed e = util::base_embed(bot, "Warnings Cleared", util::COLOR_SUCCESS)
        .set_description("Removed **" + std::to_string(removed) + "** warning(s) from <@" +
                         std::to_string(static_cast<uint64_t>(target_id)) + ">.")
        .add_field("Moderator", event.command.usr.get_mention(), true);

    util::send_log(bot, guild->id, e);
    co_await event.co_reply(dpp::message(e));
    co_return;
}

} // namespace

void add_moderation_commands(const dpp::cluster& bot,
                             std::vector<dpp::slashcommand>& definitions,
                             std::unordered_map<std::string, handler_t>& handlers) {
    auto& kick = definitions.emplace_back(dpp::slashcommand("kick", "Kick a member from the server.", bot.me.id));
    kick.add_option(dpp::command_option(dpp::co_user, "user", "The member to kick", true));
    kick.add_option(dpp::command_option(dpp::co_string, "reason", "Why are you kicking them?", false));
    kick.set_default_permissions(dpp::p_kick_members);
    handlers["kick"] = make_handler(cmd_kick);

    auto& ban = definitions.emplace_back(dpp::slashcommand("ban", "Ban a member from the server.", bot.me.id));
    ban.add_option(dpp::command_option(dpp::co_user, "user", "The member to ban", true));
    ban.add_option(dpp::command_option(dpp::co_integer, "days", "Days of messages to delete (0-7)", false).set_min_value(0).set_max_value(7));
    ban.add_option(dpp::command_option(dpp::co_string, "reason", "Why are you banning them?", false));
    ban.set_default_permissions(dpp::p_ban_members);
    handlers["ban"] = make_handler(cmd_ban);

    auto& unban = definitions.emplace_back(dpp::slashcommand("unban", "Unban a user by their ID.", bot.me.id));
    unban.add_option(dpp::command_option(dpp::co_string, "user_id", "The ID of the user to unban", true));
    unban.set_default_permissions(dpp::p_ban_members);
    handlers["unban"] = make_handler(cmd_unban);

    auto& timeout = definitions.emplace_back(dpp::slashcommand("timeout", "Timeout (mute) a member.", bot.me.id));
    timeout.add_option(dpp::command_option(dpp::co_user, "user", "The member to timeout", true));
    timeout.add_option(dpp::command_option(dpp::co_string, "duration", "Duration e.g. 30m, 2h, 1d, 1h30m", true));
    timeout.add_option(dpp::command_option(dpp::co_string, "reason", "Why are you timing them out?", false));
    timeout.set_default_permissions(dpp::p_moderate_members);
    handlers["timeout"] = make_handler(cmd_timeout);

    auto& purge = definitions.emplace_back(dpp::slashcommand("purge", "Bulk delete recent messages.", bot.me.id));
    purge.add_option(dpp::command_option(dpp::co_integer, "amount", "Number of messages to delete (1-100)", true).set_min_value(1).set_max_value(100));
    purge.add_option(dpp::command_option(dpp::co_user, "user", "Only delete messages from this user", false));
    purge.set_default_permissions(dpp::p_manage_messages);
    handlers["purge"] = make_handler(cmd_purge);

    auto& warn = definitions.emplace_back(dpp::slashcommand("warn", "Add a warning to a member.", bot.me.id));
    warn.add_option(dpp::command_option(dpp::co_user, "user", "The member to warn", true));
    warn.add_option(dpp::command_option(dpp::co_string, "reason", "Why are you warning them?", false));
    warn.set_default_permissions(dpp::p_moderate_members);
    handlers["warn"] = make_handler(cmd_warn);

    auto& warnings = definitions.emplace_back(dpp::slashcommand("warnings", "Show a member's warnings.", bot.me.id));
    warnings.add_option(dpp::command_option(dpp::co_user, "user", "The member to check (defaults to you)", false));
    warnings.set_default_permissions(dpp::p_moderate_members);
    handlers["warnings"] = make_handler(cmd_warnings);

    auto& clearwarns = definitions.emplace_back(dpp::slashcommand("clearwarns", "Clear all warnings for a member.", bot.me.id));
    clearwarns.add_option(dpp::command_option(dpp::co_user, "user", "The member to clear warnings for", true));
    clearwarns.set_default_permissions(dpp::p_moderate_members);
    handlers["clearwarns"] = make_handler(cmd_clearwarns);
}

} // namespace cmd

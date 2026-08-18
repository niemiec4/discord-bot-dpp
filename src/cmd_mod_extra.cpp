#include "commands.h"

#include "snipes.h"

#include <string>

namespace cmd {

namespace {

dpp::task<void> cmd_snipe(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!event.command.guild_id) {
        co_await event.co_reply(util::error("This command can only be used inside a server."));
        co_return;
    }
    if (!util::require_permission(event, dpp::p_manage_messages, "Manage Messages")) co_return;

    snipes::entry e;
    if (!snipes::get(event.command.channel_id, e)) {
        co_await event.co_reply(util::warning("No recently deleted message in this channel."));
        co_return;
    }

    std::string content = e.content.empty() ? "*no text content*" : util::escape(e.content);
    dpp::embed embed = dpp::embed()
        .set_color(util::COLOR_PRIMARY)
        .set_author(e.author, "", e.avatar)
        .set_description(content)
        .set_timestamp(e.time);
    if (!e.attachment.empty()) {
        embed.set_image(e.attachment);
    }

    co_await event.co_reply(dpp::message(embed));
    co_return;
}

dpp::task<void> cmd_lock(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!util::require_permission(event, dpp::p_manage_channels, "Manage Channels")) co_return;
    if (!util::require_bot_permission(bot, event, dpp::p_manage_channels, "Manage Channels")) co_return;

    dpp::snowflake channel_id = event.command.channel_id;
    dpp::command_value raw = event.get_parameter("channel");
    if (std::holds_alternative<dpp::snowflake>(raw)) {
        channel_id = std::get<dpp::snowflake>(raw);
    }
    if (!channel_id) {
        co_await event.co_reply(util::error("This command can only be used in a channel."));
        co_return;
    }
    const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) {
        co_await event.co_reply(util::error("Server not found in cache."));
        co_return;
    }

    // Deny Send Messages for @everyone.
    dpp::confirmation_callback_t res = co_await bot.co_channel_edit_permissions(
        channel_id, guild->id, 0, dpp::p_send_messages, false);
    if (res.is_error()) {
        co_await event.co_reply(util::error("Failed to lock the channel: " + res.get_error().human_readable));
        co_return;
    }

    dpp::embed e = util::base_embed(bot, "Channel Locked", util::COLOR_WARNING,
                                    "<#" + std::to_string(static_cast<uint64_t>(channel_id)) +
                                    "> is now locked. Use `/unlock` to re-open it.")
        .add_field("Moderator", event.command.usr.get_mention(), true);

    util::send_log(bot, event.command.guild_id, e);
    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_unlock(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!util::require_permission(event, dpp::p_manage_channels, "Manage Channels")) co_return;
    if (!util::require_bot_permission(bot, event, dpp::p_manage_channels, "Manage Channels")) co_return;

    dpp::snowflake channel_id = event.command.channel_id;
    dpp::command_value raw = event.get_parameter("channel");
    if (std::holds_alternative<dpp::snowflake>(raw)) {
        channel_id = std::get<dpp::snowflake>(raw);
    }
    if (!channel_id) {
        co_await event.co_reply(util::error("This command can only be used in a channel."));
        co_return;
    }
    const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) {
        co_await event.co_reply(util::error("Server not found in cache."));
        co_return;
    }

    dpp::confirmation_callback_t res = co_await bot.co_channel_edit_permissions(
        channel_id, guild->id, 0, 0, false);
    if (res.is_error()) {
        co_await event.co_reply(util::error("Failed to unlock the channel: " + res.get_error().human_readable));
        co_return;
    }

    dpp::embed e = util::base_embed(bot, "Channel Unlocked", util::COLOR_SUCCESS,
                                    "<#" + std::to_string(static_cast<uint64_t>(channel_id)) +
                                    "> is open again.")
        .add_field("Moderator", event.command.usr.get_mention(), true);

    util::send_log(bot, event.command.guild_id, e);
    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_slowmode(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!util::require_permission(event, dpp::p_manage_channels, "Manage Channels")) co_return;
    if (!util::require_bot_permission(bot, event, dpp::p_manage_channels, "Manage Channels")) co_return;

    dpp::snowflake channel_id = event.command.channel_id;
    dpp::command_value raw = event.get_parameter("channel");
    if (std::holds_alternative<dpp::snowflake>(raw)) {
        channel_id = std::get<dpp::snowflake>(raw);
    }
    if (!channel_id) {
        co_await event.co_reply(util::error("This command can only be used in a channel."));
        co_return;
    }

    int64_t seconds = std::clamp<int64_t>(util::get_integer(event, "seconds", 0), 0, 21600);

    dpp::channel ch;
    ch.id = channel_id;
    ch.rate_limit_per_user = static_cast<uint16_t>(seconds);

    dpp::confirmation_callback_t res = co_await bot.co_channel_edit(ch);
    if (res.is_error()) {
        co_await event.co_reply(util::error("Failed to set slowmode: " + res.get_error().human_readable));
        co_return;
    }

    std::string text = seconds == 0
        ? "Slowmode **disabled** for <#" + std::to_string(static_cast<uint64_t>(channel_id)) + ">."
        : "Slowmode set to **" + std::to_string(seconds) + " second(s)** for <#" +
          std::to_string(static_cast<uint64_t>(channel_id)) + ">.";

    dpp::embed e = util::base_embed(bot, "Slowmode Updated", util::COLOR_SUCCESS, text)
        .add_field("Moderator", event.command.usr.get_mention(), true);

    util::send_log(bot, event.command.guild_id, e);
    co_await event.co_reply(dpp::message(e));
    co_return;
}

} // namespace

void add_mod_extra_commands(const dpp::cluster& bot,
                            std::vector<dpp::slashcommand>& definitions,
                            std::unordered_map<std::string, handler_t>& handlers) {
    definitions.emplace_back(dpp::slashcommand("snipe", "Show the last deleted message in this channel.", bot.me.id));

    auto& lock = definitions.emplace_back(dpp::slashcommand("lock", "Lock a channel (deny sending messages).", bot.me.id));
    lock.add_option(dpp::command_option(dpp::co_channel, "channel", "Channel to lock (defaults to current)", false));
    lock.set_default_permissions(dpp::p_manage_channels);
    handlers["lock"] = make_handler(cmd_lock);

    auto& unlock = definitions.emplace_back(dpp::slashcommand("unlock", "Unlock a locked channel.", bot.me.id));
    unlock.add_option(dpp::command_option(dpp::co_channel, "channel", "Channel to unlock (defaults to current)", false));
    unlock.set_default_permissions(dpp::p_manage_channels);
    handlers["unlock"] = make_handler(cmd_unlock);

    auto& slowmode = definitions.emplace_back(dpp::slashcommand("slowmode", "Set the slowmode of a channel.", bot.me.id));
    slowmode.add_option(dpp::command_option(dpp::co_integer, "seconds", "Seconds between messages (0 = off, max 21600)", true).set_min_value(0).set_max_value(21600));
    slowmode.add_option(dpp::command_option(dpp::co_channel, "channel", "Channel (defaults to current)", false));
    slowmode.set_default_permissions(dpp::p_manage_channels);
    handlers["slowmode"] = make_handler(cmd_slowmode);
}

} // namespace cmd

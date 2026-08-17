#include "commands.h"

#include "settings.h"

#include <string>

namespace cmd {

namespace {

std::string fmt_yes_no(bool v) { return v ? "✅ Yes" : "❌ No"; }

std::string fmt_channel(dpp::snowflake id) {
    return id ? "<#" + std::to_string(static_cast<uint64_t>(id)) + ">" : "*(not set)*";
}

dpp::task<void> sub_show(dpp::cluster& bot, const dpp::slashcommand_t& event, dpp::snowflake guild_id) {
    settings::guild_settings s = settings::get(guild_id);

    std::string rewards;
    if (s.role_rewards.empty()) {
        rewards = "*(no rewards configured)*";
    } else {
        for (const auto& [level, role] : s.role_rewards) {
            rewards += "Level **" + std::to_string(level) + "** → <@&" +
                       std::to_string(static_cast<uint64_t>(role)) + ">\n";
        }
    }

    dpp::embed e = util::base_embed(bot, "⚙️ Server Settings", util::COLOR_PRIMARY)
        .add_field("Moderation log channel", fmt_channel(s.log_channel_id), true)
        .add_field("Welcome channel", fmt_channel(s.welcome_channel_id), true)
        .add_field("Welcome message", s.welcome_message.empty() ? "*(default)*" : util::escape(s.welcome_message))
        .add_field("Leveling", fmt_yes_no(s.leveling_enabled), true)
        .add_field("Level-up channel", fmt_channel(s.levelup_channel_id), true)
        .add_field("Level-up message", s.levelup_message.empty() ? "*(default)*" : util::escape(s.levelup_message))
        .add_field("Role rewards", rewards);

    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> sub_logchannel(const dpp::slashcommand_t& event, dpp::snowflake guild_id) {
    settings::guild_settings s = settings::get(guild_id);

    if (util::get_boolean(event, "clear")) {
        s.log_channel_id = 0;
        settings::set(guild_id, s);
        co_await event.co_reply(util::success("Moderation log channel cleared."));
        co_return;
    }

    dpp::command_value raw = event.get_parameter("channel");
    if (!std::holds_alternative<dpp::snowflake>(raw)) {
        co_await event.co_reply(util::error("You must pick a channel (or use `clear`)."));
        co_return;
    }
    s.log_channel_id = std::get<dpp::snowflake>(raw);
    settings::set(guild_id, s);
    co_await event.co_reply(util::success("Moderation logs will now be sent to <#" +
                                          std::to_string(static_cast<uint64_t>(s.log_channel_id)) + ">."));
    co_return;
}

dpp::task<void> sub_welcome(const dpp::slashcommand_t& event, dpp::snowflake guild_id) {
    settings::guild_settings s = settings::get(guild_id);

    if (util::get_boolean(event, "clear")) {
        s.welcome_channel_id = 0;
        settings::set(guild_id, s);
        co_await event.co_reply(util::success("Welcome messages disabled."));
        co_return;
    }

    dpp::command_value raw = event.get_parameter("channel");
    if (!std::holds_alternative<dpp::snowflake>(raw)) {
        co_await event.co_reply(util::error("You must pick a channel (or use `clear`)."));
        co_return;
    }
    s.welcome_channel_id = std::get<dpp::snowflake>(raw);
    settings::set(guild_id, s);
    co_await event.co_reply(util::success("New members will now be welcomed in <#" +
                                          std::to_string(static_cast<uint64_t>(s.welcome_channel_id)) + ">."));
    co_return;
}

dpp::task<void> sub_welcomemessage(const dpp::slashcommand_t& event, dpp::snowflake guild_id) {
    settings::guild_settings s = settings::get(guild_id);
    std::string text = util::get_string(event, "text");
    if (text.empty()) {
        s.welcome_message.clear();
        settings::set(guild_id, s);
        co_await event.co_reply(util::success("Welcome message reset to the default template."));
        co_return;
    }
    if (text.size() > 1024) {
        co_await event.co_reply(util::error("Message is too long (max 1024 characters)."));
        co_return;
    }
    s.welcome_message = text;
    settings::set(guild_id, s);
    co_await event.co_reply(util::success("Welcome message updated. Placeholders: `{user}`, `{server}`."));
    co_return;
}

dpp::task<void> sub_leveling(const dpp::slashcommand_t& event, dpp::snowflake guild_id) {
    settings::guild_settings s = settings::get(guild_id);
    bool enable = util::get_boolean(event, "enabled");
    s.leveling_enabled = enable;
    settings::set(guild_id, s);
    co_await event.co_reply(util::success(enable ? "Leveling is now **enabled**." : "Leveling is now **disabled**."));
    co_return;
}

dpp::task<void> sub_levelupchannel(const dpp::slashcommand_t& event, dpp::snowflake guild_id) {
    settings::guild_settings s = settings::get(guild_id);

    if (util::get_boolean(event, "here")) {
        s.levelup_channel_id = 0; // 0 = same channel where the user leveled
        settings::set(guild_id, s);
        co_await event.co_reply(util::success("Level-up messages will appear in the channel where the user leveled up."));
        co_return;
    }

    dpp::command_value raw = event.get_parameter("channel");
    if (!std::holds_alternative<dpp::snowflake>(raw)) {
        co_await event.co_reply(util::error("You must pick a channel (or use `here`)."));
        co_return;
    }
    s.levelup_channel_id = std::get<dpp::snowflake>(raw);
    settings::set(guild_id, s);
    co_await event.co_reply(util::success("Level-up messages will be sent to <#" +
                                          std::to_string(static_cast<uint64_t>(s.levelup_channel_id)) + ">."));
    co_return;
}

dpp::task<void> sub_levelupmessage(const dpp::slashcommand_t& event, dpp::snowflake guild_id) {
    settings::guild_settings s = settings::get(guild_id);
    std::string text = util::get_string(event, "text");
    if (text.empty()) {
        s.levelup_message.clear();
        settings::set(guild_id, s);
        co_await event.co_reply(util::success("Level-up message reset to the default template."));
        co_return;
    }
    if (text.size() > 1024) {
        co_await event.co_reply(util::error("Message is too long (max 1024 characters)."));
        co_return;
    }
    s.levelup_message = text;
    settings::set(guild_id, s);
    co_await event.co_reply(util::success("Level-up message updated. Placeholders: `{user}`, `{level}`."));
    co_return;
}

dpp::task<void> sub_reward_add(dpp::cluster& bot, const dpp::slashcommand_t& event, dpp::snowflake guild_id) {
    dpp::command_value raw_role = event.get_parameter("role");
    if (!std::holds_alternative<dpp::snowflake>(raw_role)) {
        co_await event.co_reply(util::error("You must pick a role."));
        co_return;
    }
    int64_t level = util::get_integer(event, "level", 0);
    if (level < 1) {
        co_await event.co_reply(util::error("Level must be at least 1."));
        co_return;
    }
    dpp::snowflake role_id = std::get<dpp::snowflake>(raw_role);

    const dpp::guild* guild = dpp::find_guild(guild_id);
    if (guild == nullptr) {
        co_await event.co_reply(util::error("Server not found in cache."));
        co_return;
    }
    // The bot must be able to grant the role (role below the bot's top role).
    const dpp::role* role = dpp::find_role(role_id);
    if (role == nullptr) {
        co_await event.co_reply(util::error("Role not found in cache."));
        co_return;
    }
    dpp::guild_member bot_member = dpp::find_guild_member(guild_id, bot.me.id);
    if (util::highest_role_position(*guild, bot_member) <= role->position) {
        co_await event.co_reply(util::error("I cannot grant that role — it is positioned at or above my highest role."));
        co_return;
    }

    settings::guild_settings s = settings::get(guild_id);
    s.role_rewards[static_cast<uint64_t>(level)] = role_id;
    settings::set(guild_id, s);
    co_await event.co_reply(util::success("Level **" + std::to_string(level) + "** now rewards <@&" +
                                          std::to_string(static_cast<uint64_t>(role_id)) + ">."));
    co_return;
}

dpp::task<void> sub_reward_remove(const dpp::slashcommand_t& event, dpp::snowflake guild_id) {
    int64_t level = util::get_integer(event, "level", 0);
    settings::guild_settings s = settings::get(guild_id);
    auto it = s.role_rewards.find(static_cast<uint64_t>(level));
    if (it == s.role_rewards.end()) {
        co_await event.co_reply(util::warning("No reward is configured for level " + std::to_string(level) + "."));
        co_return;
    }
    s.role_rewards.erase(it);
    settings::set(guild_id, s);
    co_await event.co_reply(util::success("Removed the reward for level **" + std::to_string(level) + "**."));
    co_return;
}

} // namespace

void add_settings_commands(const dpp::cluster& bot,
                           std::vector<dpp::slashcommand>& definitions,
                           std::unordered_map<std::string, handler_t>& handlers) {
    dpp::slashcommand settings_cmd("settings", "Configure this server (moderation logs, welcome, leveling).", bot.me.id);
    settings_cmd.set_default_permissions(dpp::p_manage_guild);

    settings_cmd.add_option(dpp::command_option(dpp::co_sub_command, "show", "Show the current server settings."));

    dpp::command_option logchannel(dpp::co_sub_command, "logchannel", "Set the moderation log channel.");
    logchannel.add_option(dpp::command_option(dpp::co_channel, "channel", "Channel for moderation logs", false));
    logchannel.add_option(dpp::command_option(dpp::co_boolean, "clear", "Clear the log channel", false));
    settings_cmd.add_option(logchannel);

    dpp::command_option welcome(dpp::co_sub_command, "welcome", "Set the welcome channel.");
    welcome.add_option(dpp::command_option(dpp::co_channel, "channel", "Channel for welcome messages", false));
    welcome.add_option(dpp::command_option(dpp::co_boolean, "clear", "Disable welcome messages", false));
    settings_cmd.add_option(welcome);

    dpp::command_option welcomemessage(dpp::co_sub_command, "welcomemessage", "Set a custom welcome message.");
    welcomemessage.add_option(dpp::command_option(dpp::co_string, "text", "Message text (empty = default). Placeholders: {user}, {server}", false));
    settings_cmd.add_option(welcomemessage);

    dpp::command_option leveling(dpp::co_sub_command, "leveling", "Enable or disable the XP/level system.");
    leveling.add_option(dpp::command_option(dpp::co_boolean, "enabled", "True to enable, false to disable", true));
    settings_cmd.add_option(leveling);

    dpp::command_option levelupchannel(dpp::co_sub_command, "levelupchannel", "Set the channel for level-up messages.");
    levelupchannel.add_option(dpp::command_option(dpp::co_channel, "channel", "Channel for level-up messages", false));
    levelupchannel.add_option(dpp::command_option(dpp::co_boolean, "here", "Show level-ups in the current channel", false));
    settings_cmd.add_option(levelupchannel);

    dpp::command_option levelupmessage(dpp::co_sub_command, "levelupmessage", "Set a custom level-up message.");
    levelupmessage.add_option(dpp::command_option(dpp::co_string, "text", "Message text (empty = default). Placeholders: {user}, {level}", false));
    settings_cmd.add_option(levelupmessage);

    dpp::command_option reward_add(dpp::co_sub_command, "rewardadd", "Grant a role when a member reaches a level.");
    reward_add.add_option(dpp::command_option(dpp::co_integer, "level", "Level that unlocks the role", true).set_min_value(1));
    reward_add.add_option(dpp::command_option(dpp::co_role, "role", "Role to grant", true));
    settings_cmd.add_option(reward_add);

    dpp::command_option reward_remove(dpp::co_sub_command, "rewardremove", "Remove a level role reward.");
    reward_remove.add_option(dpp::command_option(dpp::co_integer, "level", "Level to remove the reward for", true).set_min_value(1));
    settings_cmd.add_option(reward_remove);

    definitions.emplace_back(settings_cmd);
    handlers["settings"] = make_handler([](dpp::cluster& bot, const dpp::slashcommand_t& event) -> dpp::task<void> {
        if (!util::require_permission(event, dpp::p_manage_guild, "Manage Server")) {
            co_return;
        }
        dpp::snowflake guild_id = event.command.guild_id;
        if (!guild_id) {
            co_await event.co_reply(util::error("This command can only be used inside a server."));
            co_return;
        }

        std::string sub = "show";
        try {
            const auto& data = std::get<dpp::command_interaction>(event.command.data);
            if (!data.options.empty() && data.options[0].type == dpp::co_sub_command) {
                sub = data.options[0].name;
            }
        } catch (const std::bad_variant_access&) {
            // not a command interaction
        }

        if (sub == "logchannel")      co_await sub_logchannel(event, guild_id);
        else if (sub == "welcome")    co_await sub_welcome(event, guild_id);
        else if (sub == "welcomemessage") co_await sub_welcomemessage(event, guild_id);
        else if (sub == "leveling")   co_await sub_leveling(event, guild_id);
        else if (sub == "levelupchannel") co_await sub_levelupchannel(event, guild_id);
        else if (sub == "levelupmessage") co_await sub_levelupmessage(event, guild_id);
        else if (sub == "rewardadd")  co_await sub_reward_add(bot, event, guild_id);
        else if (sub == "rewardremove") co_await sub_reward_remove(event, guild_id);
        else                          co_await sub_show(bot, event, guild_id);

        co_return;
    });
}

} // namespace cmd

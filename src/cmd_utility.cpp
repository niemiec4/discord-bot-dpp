#include "commands.h"

#include "config.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>

namespace cmd {

namespace {

constexpr uint64_t DISCORD_EPOCH_MS = 1420070400000ULL;

/** @brief Discord snowflake creation time as unix time_t. */
time_t snowflake_time(dpp::snowflake id) {
    return static_cast<time_t>((static_cast<uint64_t>(id) >> 22) / 1000 + DISCORD_EPOCH_MS / 1000);
}

std::string rgb_hex(uint32_t rgb) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%06X", rgb & 0xFFFFFF);
    return buf;
}

double gateway_ping(const dpp::cluster& bot) {
    const auto& shards = bot.get_shards();
    if (shards.empty()) {
        return 0.0;
    }
    double total = 0.0;
    for (const auto& [id, client] : shards) {
        total += client->websocket_ping;
    }
    return total / static_cast<double>(shards.size());
}

dpp::component link_button(const std::string& url, const std::string& label) {
    return dpp::component()
        .set_label(label)
        .set_url(url)
        .set_style(dpp::cos_link);
}

dpp::task<void> cmd_ping(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    double gateway_ms = gateway_ping(bot);

    auto start = std::chrono::steady_clock::now();
    dpp::confirmation_callback_t res = co_await bot.co_current_user_get();
    auto end = std::chrono::steady_clock::now();
    double rest_ms = std::chrono::duration<double, std::milli>(end - start).count();

    dpp::embed e = util::base_embed(bot, "Pong", util::COLOR_PRIMARY)
        .add_field("Gateway latency", std::to_string(static_cast<int64_t>(std::round(gateway_ms))) + " ms", true)
        .add_field("API latency", std::to_string(static_cast<int64_t>(std::round(rest_ms))) + " ms", true)
        .add_field("Shards", std::to_string(bot.get_shards().size()), true);

    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_help(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    dpp::embed e = util::base_embed(bot, "Help", util::COLOR_PRIMARY,
                                    "Hello, I am a multi-purpose Discord bot. Everything I do is "
                                    "controlled with slash commands — pick a category below.");

    e.add_field("Moderation",
                "`kick`, `ban`, `unban`, `timeout`, `purge`, `warn`, `warnings`, `clearwarns`, "
                "`lock`, `unlock`, `slowmode`, `snipe`");
    e.add_field("Information",
                "`ping`, `help`, `botinfo`, `serverinfo`, `userinfo`, `avatar`, `servericon`, "
                "`roleinfo`, `invite`, `sync`");
    e.add_field("Server settings",
                "`settings show`, `settings logchannel`, `settings welcome`, `settings welcomemessage`, "
                "`settings leveling`, `settings levelupchannel`, `settings levelupmessage`, "
                "`settings rewardadd`, `settings rewardremove`, `settings warnthreshold`, "
                "`settings warnaction`, `settings warntimeout`, `settings xpmult`, "
                "`settings xpmultremove`, `settings xpboost`, `settings autorole`, `settings auditlog`");
    e.add_field("Levels & Economy",
                "`rank`, `top`, `balance`, `daily`, `pay`, `work`, `gamble` — earn XP by chatting "
                "and coins by working, then gamble them away.");
    e.add_field("Engagement",
                "`reactionrole`, `ticket`, `giveaway`, `poll`, `cc` — role menus, support tickets, "
                "giveaways, polls and custom commands.");
    e.add_field("Fun",
                "`8ball`, `coinflip`, `dice`, `say`, `embed`");
    e.add_field("Note",
                "Moderation commands require the matching Discord permission "
                "(`Kick Members`, `Ban Members`, `Moderate Members`, `Manage Channels`, "
                "`Manage Messages`). All `/settings` values are per-server.");

    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_botinfo(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!bot.me.id) {
        co_await event.co_reply(util::error("Bot is not ready yet, try again in a moment."));
        co_return;
    }

    dpp::embed e = util::base_embed(bot, bot.me.username, util::COLOR_PRIMARY,
                                    "I am a multi-purpose Discord bot built with the **D++** library. "
                                    "I keep your server tidy with moderation tools, reward activity with "
                                    "levels and coins, and engage members with tickets, giveaways, polls "
                                    "and role menus — all configured per server.")
        .set_thumbnail(bot.me.get_avatar_url(512))
        .add_field("Uptime", bot.uptime().to_string(), true)
        .add_field("Servers", std::to_string(dpp::get_guild_count()), true)
        .add_field("Users seen", std::to_string(dpp::get_user_count()), true)
        .add_field("Shards", std::to_string(bot.get_shards().size()), true)
        .add_field("Gateway ping", std::to_string(static_cast<int64_t>(std::round(gateway_ping(bot)))) + " ms", true)
        .add_field("Library", DPP_VERSION_TEXT, true)
        .add_field("Bot ID", std::to_string(static_cast<uint64_t>(bot.me.id)), true)
        .add_field("Created", util::rel_time(snowflake_time(bot.me.id)), true);

    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_serverinfo(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) {
        co_await event.co_reply(util::error("Server not found in cache."));
        co_return;
    }

    // Channel statistics
    size_t text = 0, voice = 0, categories = 0, forums = 0, stages = 0;
    for (const dpp::snowflake& channel_id : guild->channels) {
        const dpp::channel* ch = dpp::find_channel(channel_id);
        if (ch == nullptr) {
            continue;
        }
        switch (ch->get_type()) {
            case dpp::CHANNEL_TEXT:          ++text;       break;
            case dpp::CHANNEL_VOICE:         ++voice;      break;
            case dpp::CHANNEL_CATEGORY:      ++categories; break;
            case dpp::CHANNEL_FORUM:         ++forums;     break;
            case dpp::CHANNEL_STAGE:         ++stages;     break;
            default:                                      break;
        }
    }

    uint64_t bots = 0;
    for (const auto& [id, m] : guild->members) {
        const dpp::user* u = dpp::find_user(m.user_id);
        if (u != nullptr && u->is_bot()) {
            ++bots;
        }
    }
    uint64_t humans = static_cast<uint64_t>(guild->members.size()) - bots;

    const char* verification = "None";
    switch (guild->verification_level) {
        case dpp::verification_level_t::ver_low:      verification = "Low";       break;
        case dpp::verification_level_t::ver_medium:   verification = "Medium";    break;
        case dpp::verification_level_t::ver_high:     verification = "High";      break;
        case dpp::verification_level_t::ver_very_high: verification = "Very High"; break;
        default: break;
    }

    std::string channels = std::to_string(guild->channels.size()) + " total (" +
                           std::to_string(text) + " text, " + std::to_string(voice) + " voice, " +
                           std::to_string(categories) + " categories, " + std::to_string(forums) +
                           " forums, " + std::to_string(stages) + " stages)";

    dpp::embed e = util::base_embed(bot, guild->name, util::COLOR_PRIMARY)
        .set_thumbnail(guild->get_icon_url(512))
        .add_field("Owner", guild->owner_id ? "<@" + std::to_string(static_cast<uint64_t>(guild->owner_id)) + ">" : "Unknown", true)
        .add_field("Members", std::to_string(guild->member_count) + " (" + std::to_string(humans) + " humans, " +
                              std::to_string(bots) + " bots)", true)
        .add_field("Channels", channels, true)
        .add_field("Roles", std::to_string(guild->roles.size()), true)
        .add_field("Emojis", std::to_string(guild->emojis.size()), true)
        .add_field("Boosts", std::to_string(guild->premium_subscription_count) + " (tier " +
                             std::to_string(static_cast<int>(guild->premium_tier)) + ")", true)
        .add_field("Verification", verification, true)
        .add_field("Server ID", std::to_string(static_cast<uint64_t>(guild->id)), true)
        .add_field("Created", util::rel_time(snowflake_time(guild->id)), true);

    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_userinfo(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    dpp::snowflake target_id = util::get_user(event, "user");
    if (!target_id) {
        target_id = event.command.usr.id;
    }

    const dpp::user* user = dpp::find_user(target_id);
    if (!user) {
        co_await event.co_reply(util::error("User not found in cache."));
        co_return;
    }

    const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) {
        co_await event.co_reply(util::error("Server not found in cache."));
        co_return;
    }

    dpp::guild_member member = dpp::find_guild_member(guild->id, target_id);
    if (!member.user_id) {
        dpp::confirmation_callback_t fetch = co_await bot.co_guild_get_member(guild->id, target_id);
        if (!fetch.is_error()) {
            member = std::get<dpp::guild_member>(fetch.value);
        }
    }
    bool in_guild = static_cast<bool>(member.user_id);

    std::string nickname = member.get_nickname();

    dpp::embed e = util::base_embed(bot, user->format_username(), util::COLOR_PRIMARY)
        .set_thumbnail(user->get_avatar_url(512))
        .set_description(in_guild && nickname != user->username
                             ? "Known as **" + util::escape(nickname) + "** here"
                             : (user->is_bot() ? "This account is a bot." : "A regular member of Discord."))
        .add_field("User ID", std::to_string(static_cast<uint64_t>(user->id)), true)
        .add_field("Account created", util::rel_time(snowflake_time(user->id)), true)
        .add_field("Bot", user->is_bot() ? "Yes" : "No", true);

    if (in_guild) {
        std::string roles;
        for (const dpp::snowflake& role_id : member.get_roles()) {
            if (role_id != guild->id && dpp::find_role(role_id) != nullptr) {
                roles += "<@&" + std::to_string(static_cast<uint64_t>(role_id)) + "> ";
            }
        }
        if (roles.empty()) {
            roles = "None";
        }
        e.add_field("Joined server", util::rel_time(static_cast<time_t>(member.joined_at)), true)
         .add_field("Roles", roles);
    } else {
        e.add_field("Joined server", "Not a member of this server", true);
    }

    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_avatar(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    dpp::snowflake target_id = util::get_user(event, "user");
    if (!target_id) {
        target_id = event.command.usr.id;
    }

    const dpp::user* user = dpp::find_user(target_id);
    if (!user) {
        co_await event.co_reply(util::error("User not found in cache."));
        co_return;
    }

    std::string url = user->get_avatar_url(1024, dpp::i_png, true);
    dpp::embed e = util::base_embed(bot, user->format_username() + "'s avatar", util::COLOR_PRIMARY)
        .set_image(url);

    dpp::message msg(e);
    msg.add_component(link_button(url, "Open in browser"));
    co_await event.co_reply(msg);
    co_return;
}

dpp::task<void> cmd_servericon(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) {
        co_await event.co_reply(util::error("Server not found in cache."));
        co_return;
    }

    std::string url = guild->get_icon_url(1024, dpp::i_png, true);
    if (url.empty()) {
        co_await event.co_reply(util::error("This server does not have an icon."));
        co_return;
    }

    dpp::embed e = util::base_embed(bot, guild->name + " icon", util::COLOR_PRIMARY)
        .set_image(url);

    dpp::message msg(e);
    msg.add_component(link_button(url, "Open in browser"));
    co_await event.co_reply(msg);
    co_return;
}

dpp::task<void> cmd_invite(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!bot.me.id) {
        co_await event.co_reply(util::error("Bot is not ready yet, try again in a moment."));
        co_return;
    }

    // Permissions the bot needs: general chat usage + moderation.
    const uint64_t perms = dpp::p_view_channel | dpp::p_send_messages | dpp::p_manage_messages |
                           dpp::p_embed_links | dpp::p_attach_files | dpp::p_read_message_history |
                           dpp::p_add_reactions | dpp::p_use_external_emojis | dpp::p_kick_members |
                           dpp::p_ban_members | dpp::p_manage_channels | dpp::p_manage_roles |
                           dpp::p_moderate_members;

    std::string url = "https://discord.com/api/oauth2/authorize?client_id=" +
                      std::to_string(static_cast<uint64_t>(bot.me.id)) +
                      "&permissions=" + std::to_string(perms) +
                      "&scope=bot%20applications.commands";

    dpp::embed e = util::base_embed(bot, "Add Me to Your Server", util::COLOR_PRIMARY,
        "Click the button below to invite me to any server where you have **Manage Server** permission.\n\n"
        "The link already includes the `bot` and `applications.commands` scopes plus all the "
        "permissions needed for moderation — it works out of the box.");

    dpp::message msg(e);
    msg.add_component(link_button(url, "Invite bot"));
    co_await event.co_reply(msg);
    co_return;
}

dpp::task<void> cmd_sync(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    // Owner-only when BOT_OWNER_ID is set, otherwise requires Manage Server.
    uint64_t owner_id = cfg::get_id("BOT_OWNER_ID");
    if (owner_id) {
        if (static_cast<uint64_t>(event.command.usr.id) != owner_id) {
            co_await event.co_reply(util::error("Only the bot owner can re-register commands."));
            co_return;
        }
    } else if (!util::require_permission(event, dpp::p_manage_guild, "Manage Server")) {
        co_return;
    }

    dpp::snowflake guild_id = event.command.guild_id;
    if (!guild_id) {
        co_await event.co_reply(util::error("This command can only be used inside a server."));
        co_return;
    }

    std::vector<dpp::slashcommand> commands = cmd::build_definitions(bot);
    co_await event.co_thinking(true);

    dpp::snowflake target_guild{cfg::get_id("GUILD_ID")};
    dpp::confirmation_callback_t res;
    std::string scope;
    if (target_guild) {
        // Sync only the configured test guild (instant updates).
        res = co_await bot.co_guild_bulk_command_create(commands, target_guild);
        scope = "guild " + std::to_string(static_cast<uint64_t>(target_guild));
    } else {
        // Sync globally (replaces the whole global list, removing stale commands).
        res = co_await bot.co_global_bulk_command_create(commands);
        scope = "global";

        // Remove stale guild-scoped commands so they don't duplicate the global ones.
        dpp::cache<dpp::guild>* gc = dpp::get_guild_cache();
        std::shared_lock lock(gc->get_mutex());
        for (const auto& [gid, g] : gc->get_container()) {
            bot.guild_bulk_command_create({}, gid);
        }
    }

    if (res.is_error()) {
        co_await event.co_edit_original_response(util::error("Failed to register commands (" + scope + "): " +
                                                             res.get_error().human_readable));
        co_return;
    }

    dpp::embed e = util::base_embed(bot, "Commands Synced", util::COLOR_SUCCESS)
        .set_description("Registered **" + std::to_string(commands.size()) + "** commands (" + scope + ").")
        .add_field("Note", target_guild ? "Guild commands update instantly."
                                        : "Global commands can take up to an hour to propagate to all servers.", true);

    co_await event.co_edit_original_response(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_roleinfo(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    dpp::command_value raw = event.get_parameter("role");
    if (!std::holds_alternative<dpp::snowflake>(raw)) {
        co_await event.co_reply(util::error("You must specify a role."));
        co_return;
    }
    dpp::snowflake role_id = std::get<dpp::snowflake>(raw);

    const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) {
        co_await event.co_reply(util::error("Server not found in cache."));
        co_return;
    }

    const dpp::role* role = dpp::find_role(role_id);
    if (role == nullptr) {
        co_await event.co_reply(util::error("Role not found in cache."));
        co_return;
    }

    uint64_t member_count = 0;
    for (const auto& [id, m] : guild->members) {
        const auto& member_roles = m.get_roles();
        if (std::find(member_roles.begin(), member_roles.end(), role->id) != member_roles.end()) {
            ++member_count;
        }
    }

    std::string perms;
    if (role->permissions.can(dpp::p_administrator))  perms += "Administrator, ";
    if (role->permissions.can(dpp::p_kick_members))   perms += "Kick Members, ";
    if (role->permissions.can(dpp::p_ban_members))    perms += "Ban Members, ";
    if (role->permissions.can(dpp::p_manage_messages)) perms += "Manage Messages, ";
    if (role->permissions.can(dpp::p_moderate_members)) perms += "Moderate Members, ";
    if (role->permissions.can(dpp::p_manage_roles))   perms += "Manage Roles, ";
    if (role->permissions.can(dpp::p_manage_channels)) perms += "Manage Channels, ";
    if (role->permissions.can(dpp::p_manage_guild))   perms += "Manage Server, ";
    if (perms.empty()) perms = "No notable permissions";

    dpp::embed e = util::base_embed(bot, role->name, role->colour ? role->colour : util::COLOR_PRIMARY)
        .add_field("Role ID", std::to_string(static_cast<uint64_t>(role->id)), true)
        .add_field("Color", rgb_hex(role->colour), true)
        .add_field("Position", std::to_string(role->position), true)
        .add_field("Members", std::to_string(member_count), true)
        .add_field("Mentionable", role->is_mentionable() ? "Yes" : "No", true)
        .add_field("Hoisted", role->is_hoisted() ? "Yes" : "No", true)
        .add_field("Bot role", role->is_managed() ? "Yes" : "No", true)
        .add_field("Created", util::rel_time(snowflake_time(role->id)), true)
        .add_field("Key permissions", perms);

    co_await event.co_reply(dpp::message(e));
    co_return;
}

} // namespace

void add_utility_commands(const dpp::cluster& bot,
                          std::vector<dpp::slashcommand>& definitions,
                          std::unordered_map<std::string, handler_t>& handlers) {
    definitions.emplace_back(dpp::slashcommand("ping", "Check the bot's latency.", bot.me.id));
    handlers["ping"] = make_handler(cmd_ping);

    definitions.emplace_back(dpp::slashcommand("help", "Show all available commands.", bot.me.id));
    handlers["help"] = make_handler(cmd_help);

    definitions.emplace_back(dpp::slashcommand("botinfo", "Information about this bot.", bot.me.id));
    handlers["botinfo"] = make_handler(cmd_botinfo);

    definitions.emplace_back(dpp::slashcommand("serverinfo", "Information about this server.", bot.me.id));
    handlers["serverinfo"] = make_handler(cmd_serverinfo);

    auto& userinfo = definitions.emplace_back(dpp::slashcommand("userinfo", "Information about a user.", bot.me.id));
    userinfo.add_option(dpp::command_option(dpp::co_user, "user", "The user to inspect (defaults to you)", false));
    handlers["userinfo"] = make_handler(cmd_userinfo);

    auto& avatar = definitions.emplace_back(dpp::slashcommand("avatar", "Show a user's avatar.", bot.me.id));
    avatar.add_option(dpp::command_option(dpp::co_user, "user", "Whose avatar? (defaults to you)", false));
    handlers["avatar"] = make_handler(cmd_avatar);

    definitions.emplace_back(dpp::slashcommand("servericon", "Show this server's icon.", bot.me.id));
    handlers["servericon"] = make_handler(cmd_servericon);

    definitions.emplace_back(dpp::slashcommand("sync", "Re-register the slash commands (owner or Manage Server).", bot.me.id));
    handlers["sync"] = make_handler(cmd_sync);

    definitions.emplace_back(dpp::slashcommand("invite", "Get an invite link to add the bot to other servers.", bot.me.id));
    handlers["invite"] = make_handler(cmd_invite);

    auto& roleinfo = definitions.emplace_back(dpp::slashcommand("roleinfo", "Information about a role.", bot.me.id));
    roleinfo.add_option(dpp::command_option(dpp::co_role, "role", "The role to inspect", true));
    handlers["roleinfo"] = make_handler(cmd_roleinfo);
}

} // namespace cmd

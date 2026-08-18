#include "src/auditlog.h"
#include "src/commands.h"
#include "src/config.h"
#include "src/custom_commands.h"
#include "src/db.h"
#include "src/giveaways.h"
#include "src/interactions.h"
#include "src/levels.h"
#include "src/settings.h"
#include "src/snipes.h"
#include "src/warnings.h"

#include <dpp/dpp.h>

#include <iostream>
#include <string>

namespace {

std::string replace_all(std::string text, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

} // namespace

int main() {
    // Load configuration from .env (BOT_TOKEN, GUILD_ID, LOG_CHANNEL_ID, ...)
    cfg::load_env();

    const std::string token = cfg::get("BOT_TOKEN");
    if (token.empty()) {
        std::cerr << "BOT_TOKEN is not set. Copy .env.example to .env and fill it in.\n";
        return 1;
    }

    // Open the SQLite database (imports legacy JSON data on first run).
    db::init();

    // All intents: requires "Presence Intent", "Server Members Intent" and
    // "Message Content Intent" to be enabled in the Discord Developer Portal.
    dpp::cluster bot(token, dpp::i_all_intents);

    bot.on_log(dpp::utility::cout_logger());

    // Command dispatch table (name -> coroutine handler).
    std::unordered_map<std::string, cmd::handler_t> handlers = cmd::build_handlers();

    bot.on_slashcommand([&bot, &handlers](const dpp::slashcommand_t& event) -> dpp::task<void> {
        std::string name = event.command.get_command_name();

        auto it = handlers.find(name);
        if (it != handlers.end()) {
            co_await it->second(bot, event);
            co_return;
        }

        // Fallback: per-guild custom commands (managed with /cc).
        if (event.command.guild_id) {
            std::string response;
            if (custom_cmds::get(event.command.guild_id, name, response)) {
                response = replace_all(response, "{user}", event.command.usr.get_mention());
                response = replace_all(response, "{channel}",
                                       "<#" + std::to_string(static_cast<uint64_t>(event.command.channel_id)) + ">");
                dpp::message msg(response);
                msg.set_reference(event.command.message_id);
                co_await event.co_reply(msg);
                co_return;
            }
        }

        // Stale command registered on Discord but missing from this build.
        bot.log(dpp::ll_warning, "Received unknown command: /" + name);
        dpp::message unknown_msg(dpp::embed()
            .set_color(util::COLOR_WARNING)
            .set_title("Unknown Command")
            .set_description("This command (\"/" + util::escape(name) +
                              "\") is no longer available. It is probably a leftover from an older version "
                              "of the bot.\n\nUse `/help` to see all current commands."));
        unknown_msg.set_flags(dpp::m_ephemeral);
        event.reply(unknown_msg);
        co_return;
    });

    // Buttons: tickets, giveaways, polls.
    bot.on_button_click([&bot](const dpp::button_click_t& event) -> dpp::task<void> {
        co_await interactions::handle_button(bot, event);
        co_return;
    });

    // Select menus: reaction role menus.
    bot.on_select_click([&bot](const dpp::select_click_t& event) {
        interactions::handle_select(bot, event);
    });

    bot.on_ready([&bot](const dpp::ready_t&) -> dpp::task<void> {
        if (dpp::run_once<struct register_bot_commands>()) {
            std::vector<dpp::slashcommand> commands = cmd::build_definitions(bot);

            dpp::snowflake guild_id{cfg::get_id("GUILD_ID")};
            if (guild_id) {
                // Register as guild commands: they appear instantly, ideal for development.
                bot.guild_bulk_command_create(commands, guild_id, [count = commands.size()](const dpp::confirmation_callback_t& res) {
                    if (res.is_error()) {
                        std::cerr << "Failed to register guild commands: " << res.get_error().human_readable << "\n";
                    } else {
                        std::cout << "Registered " << count << " guild commands\n";
                    }
                });
            } else {
                // Register globally: visible in every server (may take up to an hour to propagate).
                bot.global_bulk_command_create(commands, [count = commands.size()](const dpp::confirmation_callback_t& res) {
                    if (res.is_error()) {
                        std::cerr << "Failed to register global commands: " << res.get_error().human_readable << "\n";
                    } else {
                        std::cout << "Registered " << count << " global commands\n";
                    }
                });

                // Remove stale guild-scoped commands (leftovers from older builds / testing)
                // so they don't show up as duplicates next to the global ones.
                // Wait a few seconds first so the guild cache is fully populated.
                co_await bot.co_sleep(5);
                dpp::cache<dpp::guild>* gc = dpp::get_guild_cache();
                std::shared_lock lock(gc->get_mutex());
                size_t cleared = 0;
                for (const auto& [gid, g] : gc->get_container()) {
                    bot.guild_bulk_command_create({}, gid);
                    ++cleared;
                }
                bot.log(dpp::ll_info, "Cleared guild-scoped commands on " + std::to_string(cleared) +
                                      " guild(s) to avoid duplicates");
            }

            // Status: online with a custom activity pointing to /help
            dpp::presence presence(dpp::ps_online, dpp::at_custom, "A modern Discord bot — /help");
            bot.set_presence(presence);
        }
        co_return;
    });

    // ── Audit log + welcome + auto-role: member joins ────────────────────
    bot.on_guild_member_add([&bot](const dpp::guild_member_add_t& event) {
        dpp::snowflake guild_id = event.adding_guild.id;
        settings::guild_settings s = settings::get(guild_id);

        // Welcome message (per-server setting wins, falls back to the env default).
        dpp::snowflake welcome_channel = s.welcome_channel_id;
        if (!welcome_channel) {
            welcome_channel = dpp::snowflake{cfg::get_id("WELCOME_CHANNEL_ID")};
        }
        if (welcome_channel) {
            const dpp::guild* guild = dpp::find_guild(guild_id);
            std::string guild_name = guild ? guild->name : "the server";
            uint64_t member_count = guild ? guild->member_count : 0;

            dpp::embed e = util::welcome_embed(bot, guild_name, event.added,
                                               s.welcome_message, member_count);
            bot.message_create(dpp::message(welcome_channel, e));
        }

        // Auto-role.
        if (s.autorole_id) {
            bot.guild_member_add_role(guild_id, event.added.user_id, s.autorole_id,
                                      [](const dpp::confirmation_callback_t& res) {
                                          if (res.is_error()) {
                                              std::cerr << "[autorole] Could not assign role: "
                                                        << res.get_error().human_readable << "\n";
                                          }
                                      });
        }

        // Audit: member joined.
        constexpr uint64_t DISCORD_EPOCH_MS = 1420070400000ULL;
        time_t account_created = static_cast<time_t>(
            (static_cast<uint64_t>(event.added.user_id) >> 22) / 1000 + DISCORD_EPOCH_MS / 1000);
        audit::send(bot, guild_id, "Member Joined",
                    "<@" + std::to_string(static_cast<uint64_t>(event.added.user_id)) + "> joined the server.",
                    util::COLOR_SUCCESS, "Account created",
                    util::rel_time(account_created));
    });

    // Audit: member left.
    bot.on_guild_member_remove([&bot](const dpp::guild_member_remove_t& event) {
        audit::send(bot, event.guild_id, "Member Left",
                    "**" + util::escape(event.removed.format_username()) + "** left the server.",
                    util::COLOR_ERROR);
    });

    // Audit: bans and unbans.
    bot.on_guild_ban_add([&bot](const dpp::guild_ban_add_t& event) {
        audit::send(bot, event.banning_guild.id, "Member Banned",
                    "**" + util::escape(event.banned.format_username()) + "** was banned.",
                    util::COLOR_ERROR);
    });

    bot.on_guild_ban_remove([&bot](const dpp::guild_ban_remove_t& event) {
        audit::send(bot, event.unbanning_guild.id, "Member Unbanned",
                    "**" + util::escape(event.unbanned.format_username()) + "** was unbanned.",
                    util::COLOR_SUCCESS);
    });

    // Audit: channels created / deleted.
    bot.on_channel_create([&bot](const dpp::channel_create_t& event) {
        const dpp::channel& ch = event.created;
        std::string type_name = "Channel";
        switch (ch.get_type()) {
            case dpp::CHANNEL_TEXT:     type_name = "Text channel";     break;
            case dpp::CHANNEL_VOICE:    type_name = "Voice channel";    break;
            case dpp::CHANNEL_CATEGORY: type_name = "Category";         break;
            case dpp::CHANNEL_FORUM:    type_name = "Forum";            break;
            default:                                                    break;
        }
        audit::send(bot, event.creating_guild.id, "Channel Created",
                    type_name + " **" + util::escape(ch.name) + "** was created.",
                    util::COLOR_SUCCESS, "Channel", "<#" + std::to_string(static_cast<uint64_t>(ch.id)) + ">");
    });

    bot.on_channel_delete([&bot](const dpp::channel_delete_t& event) {
        audit::send(bot, event.deleting_guild.id, "Channel Deleted",
                    "Channel **" + util::escape(event.deleted.name) + "** was deleted.",
                    util::COLOR_ERROR);
    });

    // Audit: message edits.
    bot.on_message_update([&bot](const dpp::message_update_t& event) {
        if (!event.msg.guild_id || event.msg.author.is_bot()) {
            return; // skip bot-internal edits (poll updates, etc.)
        }
        if (event.msg.content.empty()) {
            return;
        }
        std::string content = event.msg.content;
        if (content.size() > 512) {
            content.resize(512);
            content += "...";
        }
        audit::send(bot, event.msg.guild_id, "Message Edited",
                    "**" + util::escape(event.msg.author.format_username()) + "** edited a message in <#" +
                    std::to_string(static_cast<uint64_t>(event.msg.channel_id)) + ">.",
                    util::COLOR_PRIMARY, "New content", util::escape(content));
    });

    // Audit + snipe: message deletes.
    bot.on_message_delete([&bot](const dpp::message_delete_t& event) {
        if (!event.guild_id) {
            return;
        }
        snipes::on_delete(event.channel_id, event.id);

        audit::send(bot, event.guild_id, "Message Deleted",
                    "A message was deleted in <#" + std::to_string(static_cast<uint64_t>(event.channel_id)) + ">.",
                    util::COLOR_WARNING, "Message ID", std::to_string(static_cast<uint64_t>(event.id)));
    });

    // XP/leveling + snipe tracking + mention replies: every chat message.
    bot.on_message_create([&bot](const dpp::message_create_t& event) {
        levels::handle_message(bot, event.msg);
        snipes::track(event.msg);

        // When the bot is mentioned and the message is ONLY the mention
        // (not a mention inside a sentence), reply with the bot's info card.
        if (bot.me.id && !event.msg.author.is_bot() && event.msg.guild_id) {
            std::string content = event.msg.content;
            // Trim surrounding whitespace.
            auto first = content.find_first_not_of(" \t\r\n");
            if (first != std::string::npos) {
                content = content.substr(first);
                auto last = content.find_last_not_of(" \t\r\n");
                content = content.substr(0, last + 1);
            }

            std::string bot_id = std::to_string(static_cast<uint64_t>(bot.me.id));
            if (content == "<@" + bot_id + ">" || content == "<@!" + bot_id + ">") {
                dpp::message reply(util::bot_info_embed(bot));
                reply.channel_id = event.msg.channel_id;
                reply.set_reference(event.msg.id);
                bot.message_create(reply);
            }
        }
    });

    // Giveaway sweeper: end giveaways whose time is up (every 10 seconds).
    bot.start_timer([&bot](dpp::timer) { giveaways::tick(bot); }, 10);

    bot.start(dpp::st_wait);
    return 0;
}

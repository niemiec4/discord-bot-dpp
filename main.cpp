#include "src/commands.h"
#include "src/config.h"
#include "src/levels.h"
#include "src/settings.h"
#include "src/warnings.h"

#include <dpp/dpp.h>

#include <iostream>
#include <string>

int main() {
    // Load configuration from .env (BOT_TOKEN, GUILD_ID, LOG_CHANNEL_ID, ...)
    cfg::load_env();

    const std::string token = cfg::get("BOT_TOKEN");
    if (token.empty()) {
        std::cerr << "❌ BOT_TOKEN is not set. Copy .env.example to .env and fill it in.\n";
        return 1;
    }

    // Load persistent data: warnings, per-server settings, XP levels
    warns::load();
    settings::load();
    levels::load();

    // All intents: requires "Presence Intent", "Server Members Intent" and
    // "Message Content Intent" to be enabled in the Discord Developer Portal.
    dpp::cluster bot(token, dpp::i_all_intents);

    bot.on_log(dpp::utility::cout_logger());

    // Command dispatch table (name -> coroutine handler)
    std::unordered_map<std::string, cmd::handler_t> handlers = cmd::build_handlers();

    bot.on_slashcommand([&bot, &handlers](const dpp::slashcommand_t& event) -> dpp::task<void> {
        std::string name = event.command.get_command_name();
        auto it = handlers.find(name);
        if (it != handlers.end()) {
            co_await it->second(bot, event);
        } else {
            // This happens when Discord still has a stale command registered from an
            // older version of the bot (e.g. the old `/bot`). Re-registering the
            // current command list removes it — see the README section on commands.
            bot.log(dpp::ll_warning, "Received unknown command: /" + name);
            dpp::message unknown_msg(dpp::embed()
                .set_color(util::COLOR_WARNING)
                .set_title("⚠️ Unknown command")
                .set_description("This command (\"/" + util::escape(name) +
                                  ") is no longer available. It is probably a leftover from an older version "
                                  "of the bot.\n\nUse `/help` to see all current commands."));
            unknown_msg.set_flags(dpp::m_ephemeral);
            event.reply(unknown_msg);
        }
        co_return;
    });

    bot.on_ready([&bot](const dpp::ready_t&) -> dpp::task<void> {
        if (dpp::run_once<struct register_bot_commands>()) {
            std::vector<dpp::slashcommand> commands = cmd::build_definitions(bot);

            dpp::snowflake guild_id{cfg::get_id("GUILD_ID")};
            if (guild_id) {
                // Register as guild commands: they appear instantly, ideal for development.
                bot.guild_bulk_command_create(commands, guild_id, [count = commands.size()](const dpp::confirmation_callback_t& res) {
                    if (res.is_error()) {
                        std::cerr << "❌ Failed to register guild commands: " << res.get_error().human_readable << "\n";
                    } else {
                        std::cout << "✅ Registered " << count << " guild commands\n";
                    }
                });
            } else {
                // Register globally: visible in every server (may take up to an hour to propagate).
                bot.global_bulk_command_create(commands, [count = commands.size()](const dpp::confirmation_callback_t& res) {
                    if (res.is_error()) {
                        std::cerr << "❌ Failed to register global commands: " << res.get_error().human_readable << "\n";
                    } else {
                        std::cout << "✅ Registered " << count << " global commands\n";
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
            dpp::presence presence(dpp::ps_online, dpp::at_custom, "🚀 Modern D++ bot | /help");
            bot.set_presence(presence);
        }
        co_return;
    });

    // Per-server welcome messages (customizable via /settings welcome).
    bot.on_guild_member_add([&bot](const dpp::guild_member_add_t& event) {
        dpp::snowflake guild_id = event.adding_guild.id;
        settings::guild_settings s = settings::get(guild_id);

        // Per-server setting wins; falls back to the global WELCOME_CHANNEL_ID env var.
        dpp::snowflake welcome_channel = s.welcome_channel_id;
        if (!welcome_channel) {
            welcome_channel = dpp::snowflake{cfg::get_id("WELCOME_CHANNEL_ID")};
        }
        if (!welcome_channel) {
            return;
        }

        const dpp::guild* guild = dpp::find_guild(guild_id);
        std::string guild_name = guild ? guild->name : "the server";
        std::string mention = "<@" + std::to_string(static_cast<uint64_t>(event.added.user_id)) + ">";

        std::string text = s.welcome_message;
        if (text.empty()) {
            text = "Hey {user}, welcome to **{server}**!\nType `/help` to see everything I can do.";
        }
        auto replace = [](std::string t, const std::string& from, const std::string& to) {
            size_t pos = 0;
            while ((pos = t.find(from, pos)) != std::string::npos) {
                t.replace(pos, from.size(), to);
                pos += to.size();
            }
            return t;
        };
        text = replace(text, "{user}", mention);
        text = replace(text, "{server}", util::escape(guild_name));

        dpp::embed e = dpp::embed()
            .set_color(util::COLOR_SUCCESS)
            .set_title("🎉 Welcome!")
            .set_description(text)
            .set_thumbnail(event.added.get_avatar_url(256))
            .set_timestamp(time(nullptr));

        bot.message_create(dpp::message(welcome_channel, e));
    });

    // XP/leveling: every chat message earns XP (per-server, see /settings leveling).
    bot.on_message_create([&bot](const dpp::message_create_t& event) {
        levels::handle_message(bot, event.msg);
    });

    bot.start(dpp::st_wait);
    return 0;
}

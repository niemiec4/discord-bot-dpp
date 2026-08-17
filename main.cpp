#include "src/commands.h"
#include "src/config.h"
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

    // Load persistent warning data from data/warnings.json
    warns::load();

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
            event.reply(dpp::message("Unknown command.").set_flags(dpp::m_ephemeral));
        }
        co_return;
    });

    bot.on_ready([&bot](const dpp::ready_t&) {
        if (dpp::run_once<struct register_bot_commands>()) {
            std::vector<dpp::slashcommand> commands = cmd::build_definitions(bot);

            dpp::snowflake guild_id{cfg::get_id("GUILD_ID")};
            if (guild_id) {
                // Register as guild commands: they appear instantly, ideal for development.
                bot.guild_bulk_command_create(commands, guild_id);
                bot.log(dpp::ll_info, "Registered " + std::to_string(commands.size()) +
                                      " commands for guild " + std::to_string(static_cast<uint64_t>(guild_id)));
            } else {
                // Register globally: visible in every server (may take up to an hour to propagate).
                bot.global_bulk_command_create(commands);
                bot.log(dpp::ll_info, "Registered " + std::to_string(commands.size()) + " global commands");
            }

            // Status: online with a custom activity pointing to /help
            dpp::presence presence(dpp::ps_online, dpp::at_custom, "🚀 Modern D++ bot | /help");
            bot.set_presence(presence);
        }
    });

    // Optional: welcome new members if WELCOME_CHANNEL_ID is set in .env
    bot.on_guild_member_add([&bot](const dpp::guild_member_add_t& event) {
        dpp::snowflake welcome_channel{cfg::get_id("WELCOME_CHANNEL_ID")};
        if (!welcome_channel) {
            return;
        }

        dpp::snowflake guild_id = event.adding_guild.id;
        const dpp::guild* guild = dpp::find_guild(guild_id);
        std::string guild_name = guild ? guild->name : "the server";

        dpp::embed e = dpp::embed()
            .set_color(util::COLOR_SUCCESS)
            .set_title("🎉 Welcome!")
            .set_description("Hey <@" + std::to_string(static_cast<uint64_t>(event.added.user_id)) +
                             ">, welcome to **" + util::escape(guild_name) + "**!\n"
                             "Type `/help` to see everything I can do.")
            .set_thumbnail(event.added.get_avatar_url(256))
            .set_timestamp(time(nullptr));

        bot.message_create(dpp::message(welcome_channel, e));
    });

    bot.start(dpp::st_wait);
    return 0;
}

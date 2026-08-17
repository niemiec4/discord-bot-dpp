#include <dpp/dpp.h>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdlib>

void load_env(const std::string& filename = ".env") {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "File " << filename << " not found" << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        auto pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            #ifdef _WIN32
                        _putenv_s(key.c_str(), value.c_str());
            #else
                        setenv(key.c_str(), value.c_str(), 1);
            #endif
        }
    }
}

int main() {
    load_env();

    const char* token = std::getenv("BOT_TOKEN");
    if(!token) {std::cout<<"Token not found."<<std::endl; return 1;}

    dpp::cluster bot(token);

    bot.on_log(dpp::utility::cout_logger());

    bot.on_slashcommand([&bot](const dpp::slashcommand_t& event) {
         if (event.command.get_command_name() == "ping") {
             event.reply("Pong!");
         }
         else if (event.command.get_command_name() == "bot") {
            dpp::embed bot = dpp::embed()
                .set_color(dpp::colors::gray)
                .set_title("Bot Informations")
                .set_url("")
                .set_description("Small discord bot created with passion by young computer enthusiast.")
                .add_field("Commands:", "`ping`, `bot`")
                .set_footer(
                    dpp::embed_footer() 
                    .set_text("Free open-source bot - VIsit my github")
                )
                .set_timestamp(time(0));

            dpp::message msg(event.command.channel_id, bot);

            event.reply(msg);
         }
    });

    bot.on_ready([&bot](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_bot_commands>()) {
            dpp::snowflake guild_id = 1428273811358486581;

            bot.guild_bulk_command_create({}, guild_id);

            std::vector<dpp::slashcommand> commands = {
                dpp::slashcommand("ping", "Pong!", bot.me.id),
                dpp::slashcommand("bot", "Bot infromations", bot.me.id)
            };

            bot.global_bulk_command_create(commands);
        }
    });

    bot.start(dpp::st_wait);
    return 0;
}
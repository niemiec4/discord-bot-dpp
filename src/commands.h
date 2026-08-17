#pragma once

#include <dpp/dpp.h>

#include "bot_utils.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @brief Central command registry: definitions for registration plus
 * coroutine handlers dispatched by command name.
 */
namespace cmd {

using handler_t = std::function<dpp::task<void>(dpp::cluster&, const dpp::slashcommand_t&)>;

/**
 * @brief Wrap a coroutine handler with uniform error handling. Any exception
 * thrown inside the handler is caught and reported to the user.
 */
template <typename F>
handler_t make_handler(F f) {
    return [f = std::move(f)](dpp::cluster& bot, const dpp::slashcommand_t& event) -> dpp::task<void> {
        try {
            co_await f(bot, event);
        } catch (const std::exception& e) {
            event.reply(dpp::message(dpp::embed()
                .set_color(util::COLOR_ERROR)
                .set_title("❌ Error")
                .set_description(std::string("Unexpected error: ") + e.what())));
        } catch (...) {
            event.reply(dpp::message(dpp::embed()
                .set_color(util::COLOR_ERROR)
                .set_title("❌ Error")
                .set_description("Unexpected error.")));
        }
        co_return;
    };
}

// Category registration — implemented in cmd_moderation.cpp, cmd_utility.cpp, cmd_fun.cpp
void add_moderation_commands(const dpp::cluster& bot,
                             std::vector<dpp::slashcommand>& definitions,
                             std::unordered_map<std::string, handler_t>& handlers);

void add_utility_commands(const dpp::cluster& bot,
                          std::vector<dpp::slashcommand>& definitions,
                          std::unordered_map<std::string, handler_t>& handlers);

void add_fun_commands(const dpp::cluster& bot,
                      std::vector<dpp::slashcommand>& definitions,
                      std::unordered_map<std::string, handler_t>& handlers);

void add_settings_commands(const dpp::cluster& bot,
                           std::vector<dpp::slashcommand>& definitions,
                           std::unordered_map<std::string, handler_t>& handlers);

void add_level_commands(const dpp::cluster& bot,
                        std::vector<dpp::slashcommand>& definitions,
                        std::unordered_map<std::string, handler_t>& handlers);

/** @brief All slash command definitions ready for Discord registration. */
std::vector<dpp::slashcommand> build_definitions(const dpp::cluster& bot);

/** @brief Dispatch map: command name -> handler. */
std::unordered_map<std::string, handler_t> build_handlers();

} // namespace cmd

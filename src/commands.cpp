#include "commands.h"

namespace cmd {

std::vector<dpp::slashcommand> build_definitions(const dpp::cluster& bot) {
    std::vector<dpp::slashcommand> definitions;
    std::unordered_map<std::string, handler_t> discard;
    add_moderation_commands(bot, definitions, discard);
    add_utility_commands(bot, definitions, discard);
    add_fun_commands(bot, definitions, discard);
    add_settings_commands(bot, definitions, discard);
    add_level_commands(bot, definitions, discard);
    add_economy_commands(bot, definitions, discard);
    add_custom_commands(bot, definitions, discard);
    add_rolemenu_commands(bot, definitions, discard);
    add_ticket_commands(bot, definitions, discard);
    add_giveaway_commands(bot, definitions, discard);
    add_poll_commands(bot, definitions, discard);
    add_mod_extra_commands(bot, definitions, discard);
    return definitions;
}

std::unordered_map<std::string, handler_t> build_handlers() {
    // Definitions are only needed at registration time; a dummy cluster is
    // enough to build them here so the dispatch map can be filled in.
    dpp::cluster dummy("");
    std::vector<dpp::slashcommand> definitions;

    std::unordered_map<std::string, handler_t> handlers;
    add_moderation_commands(dummy, definitions, handlers);
    add_utility_commands(dummy, definitions, handlers);
    add_fun_commands(dummy, definitions, handlers);
    add_settings_commands(dummy, definitions, handlers);
    add_level_commands(dummy, definitions, handlers);
    add_economy_commands(dummy, definitions, handlers);
    add_custom_commands(dummy, definitions, handlers);
    add_rolemenu_commands(dummy, definitions, handlers);
    add_ticket_commands(dummy, definitions, handlers);
    add_giveaway_commands(dummy, definitions, handlers);
    add_poll_commands(dummy, definitions, handlers);
    add_mod_extra_commands(dummy, definitions, handlers);
    return handlers;
}

} // namespace cmd

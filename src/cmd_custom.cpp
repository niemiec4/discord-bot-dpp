#include "commands.h"

#include "custom_commands.h"

#include <string>

namespace cmd {

namespace {

dpp::task<void> cmd_cc_add(dpp::cluster& /*bot*/, const dpp::slashcommand_t& event) {
    if (!util::require_permission(event, dpp::p_manage_guild, "Manage Server")) co_return;

    std::string name = util::get_string(event, "name");
    std::string response = util::get_string(event, "response");
    if (name.empty() || response.empty()) {
        co_await event.co_reply(util::error("Both `name` and `response` are required."));
        co_return;
    }

    // Never shadow a built-in command.
    auto builtin = cmd::build_handlers();
    std::string lower = name;
    if (!lower.empty() && lower[0] == '/') lower.erase(lower.begin());
    for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (builtin.count(lower) > 0) {
        co_await event.co_reply(util::error("`" + util::escape(name) + "` is a built-in command. "
                                            "Pick a different name."));
        co_return;
    }

    if (!custom_cmds::set(event.command.guild_id, name, response)) {
        co_await event.co_reply(util::error("Invalid command. Name must be 1-32 characters and "
                                            "the response 1-1024 characters."));
        co_return;
    }

    co_await event.co_reply(util::success("Custom command **/" + util::escape(name) +
                                          "** is now available. Placeholders: `{user}`, `{channel}`."));
    co_return;
}

dpp::task<void> cmd_cc_remove(dpp::cluster& /*bot*/, const dpp::slashcommand_t& event) {
    if (!util::require_permission(event, dpp::p_manage_guild, "Manage Server")) co_return;

    std::string name = util::get_string(event, "name");
    if (custom_cmds::remove(event.command.guild_id, name)) {
        co_await event.co_reply(util::success("Custom command **/" + util::escape(name) + "** removed."));
    } else {
        co_await event.co_reply(util::warning("No custom command named `" + util::escape(name) + "`."));
    }
    co_return;
}

dpp::task<void> cmd_cc_list(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!event.command.guild_id) {
        co_await event.co_reply(util::error("This command can only be used inside a server."));
        co_return;
    }
    auto commands = custom_cmds::list(event.command.guild_id);
    if (commands.empty()) {
        co_await event.co_reply(util::warning("This server has no custom commands yet. "
                                              "Add one with `/cc add`."));
        co_return;
    }

    std::string list;
    for (const auto& [name, response] : commands) {
        list += "`/" + util::escape(name) + "` — " + util::escape(response) + "\n";
    }

    dpp::embed e = util::base_embed(bot, "Custom Commands", util::COLOR_PRIMARY,
                                    std::to_string(commands.size()) + " command(s):\n\n" + list);

    co_await event.co_reply(dpp::message(e));
    co_return;
}

} // namespace

void add_custom_commands(const dpp::cluster& bot,
                         std::vector<dpp::slashcommand>& definitions,
                         std::unordered_map<std::string, handler_t>& handlers) {
    dpp::slashcommand cc("cc", "Manage this server's custom commands.", bot.me.id);
    cc.set_default_permissions(dpp::p_manage_guild);

    dpp::command_option add(dpp::co_sub_command, "add", "Add a custom command.");
    add.add_option(dpp::command_option(dpp::co_string, "name", "Command name (no slash needed)", true));
    add.add_option(dpp::command_option(dpp::co_string, "response", "What the bot should reply. Placeholders: {user}, {channel}", true));
    cc.add_option(add);

    dpp::command_option remove(dpp::co_sub_command, "remove", "Remove a custom command.");
    remove.add_option(dpp::command_option(dpp::co_string, "name", "Command name", true));
    cc.add_option(remove);

    cc.add_option(dpp::command_option(dpp::co_sub_command, "list", "List all custom commands."));

    definitions.emplace_back(cc);
    handlers["cc"] = make_handler([](dpp::cluster& bot, const dpp::slashcommand_t& event) -> dpp::task<void> {
        dpp::snowflake guild_id = event.command.guild_id;
        if (!guild_id) {
            co_await event.co_reply(util::error("This command can only be used inside a server."));
            co_return;
        }

        std::string sub = "list";
        try {
            const auto& data = std::get<dpp::command_interaction>(event.command.data);
            if (!data.options.empty() && data.options[0].type == dpp::co_sub_command) {
                sub = data.options[0].name;
            }
        } catch (const std::bad_variant_access&) {
            // not a command interaction
        }

        if (sub == "add")         co_await cmd_cc_add(bot, event);
        else if (sub == "remove") co_await cmd_cc_remove(bot, event);
        else                      co_await cmd_cc_list(bot, event);

        co_return;
    });
}

} // namespace cmd

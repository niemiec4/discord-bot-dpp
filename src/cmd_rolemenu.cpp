#include "commands.h"

#include "role_menus.h"

#include <string>

namespace cmd {

namespace {

dpp::task<void> sub_create(dpp::cluster& /*bot*/, const dpp::slashcommand_t& event, dpp::snowflake guild_id) {
    std::string name = util::get_string(event, "name");
    std::string title = util::get_string(event, "title");
    if (name.empty() || title.empty()) {
        co_await event.co_reply(util::error("Both `name` and `title` are required."));
        co_return;
    }
    if (name.size() > 32 || title.size() > 256) {
        co_await event.co_reply(util::error("Name must be 1-32 and title 1-256 characters."));
        co_return;
    }

    int64_t id = role_menus::create(guild_id, name, title);
    if (id == 0) {
        co_await event.co_reply(util::error("A menu named `" + util::escape(name) + "` already exists. "
                                            "Pick a different name."));
        co_return;
    }

    co_await event.co_reply(util::success("Menu **" + util::escape(name) + "** created (id " +
                                          std::to_string(id) + "). Add roles with `/reactionrole add`."));
    co_return;
}

dpp::task<void> sub_add(dpp::cluster& /*bot*/, const dpp::slashcommand_t& event, dpp::snowflake guild_id) {
    std::string name = util::get_string(event, "name");
    dpp::command_value raw_role = event.get_parameter("role");
    if (!std::holds_alternative<dpp::snowflake>(raw_role)) {
        co_await event.co_reply(util::error("You must pick a role."));
        co_return;
    }
    dpp::snowflake role_id = std::get<dpp::snowflake>(raw_role);
    std::string label = util::get_string(event, "label");
    if (label.empty()) {
        const dpp::role* role = dpp::find_role(role_id);
        label = role ? role->name : "Role";
    }

    // Find the menu by name.
    int64_t menu_id = 0;
    for (const auto& m : role_menus::list(guild_id)) {
        if (m.name == name) {
            menu_id = m.id;
            break;
        }
    }
    if (menu_id == 0) {
        co_await event.co_reply(util::error("No menu named `" + util::escape(name) + "`. "
                                            "Create one with `/reactionrole create`."));
        co_return;
    }

    role_menus::add_option(menu_id, role_id, label);
    co_await event.co_reply(util::success("Added <@&" + std::to_string(static_cast<uint64_t>(role_id)) +
                                          "> to menu **" + util::escape(name) + "**."));
    co_return;
}

dpp::task<void> sub_post(dpp::cluster& bot, const dpp::slashcommand_t& event, dpp::snowflake guild_id) {
    std::string name = util::get_string(event, "name");
    dpp::snowflake channel_id = event.command.channel_id;
    dpp::command_value raw_channel = event.get_parameter("channel");
    if (std::holds_alternative<dpp::snowflake>(raw_channel)) {
        channel_id = std::get<dpp::snowflake>(raw_channel);
    }
    if (!channel_id) {
        co_await event.co_reply(util::error("This command can only be used in a channel."));
        co_return;
    }

    int64_t menu_id = 0;
    role_menus::menu menu;
    for (const auto& m : role_menus::list(guild_id)) {
        if (m.name == name) {
            menu_id = m.id;
            menu = m;
            break;
        }
    }
    if (menu_id == 0) {
        co_await event.co_reply(util::error("No menu named `" + util::escape(name) + "`. "
                                            "Create one with `/reactionrole create`."));
        co_return;
    }

    auto options = role_menus::options(menu_id);
    if (options.empty()) {
        co_await event.co_reply(util::error("Menu **" + util::escape(name) + "** has no roles yet. "
                                            "Add some with `/reactionrole add`."));
        co_return;
    }
    if (options.size() > 25) {
        options.resize(25); // Discord select menus allow at most 25 options
    }

    dpp::component select;
    select.set_type(dpp::cot_selectmenu)
          .set_id("rolemenu:" + std::to_string(menu_id))
          .set_placeholder("Choose roles to toggle")
          .set_min_values(0)
          .set_max_values(static_cast<uint32_t>(options.size()));
    for (const auto& [role, label] : options) {
        select.add_select_option(dpp::select_option(label, std::to_string(static_cast<uint64_t>(role))));
    }

    dpp::embed e = dpp::embed()
        .set_color(util::COLOR_PRIMARY)
        .set_title(menu.title)
        .set_description("Pick roles from the dropdown below to toggle them.")
        .set_timestamp(time(nullptr));

    dpp::message msg(channel_id, e);
    msg.add_component(select);

    co_await bot.co_message_create(msg);
    co_await event.co_reply(util::success("Menu **" + util::escape(name) + "** posted in <#" +
                                          std::to_string(static_cast<uint64_t>(channel_id)) + ">."));
    co_return;
}

dpp::task<void> sub_remove(dpp::cluster& /*bot*/, const dpp::slashcommand_t& event, dpp::snowflake guild_id) {
    std::string name = util::get_string(event, "name");
    if (role_menus::remove(guild_id, name)) {
        co_await event.co_reply(util::success("Menu **" + util::escape(name) + "** deleted."));
    } else {
        co_await event.co_reply(util::warning("No menu named `" + util::escape(name) + "`."));
    }
    co_return;
}

dpp::task<void> sub_list(dpp::cluster& bot, const dpp::slashcommand_t& event, dpp::snowflake guild_id) {
    auto menus = role_menus::list(guild_id);
    if (menus.empty()) {
        co_await event.co_reply(util::warning("This server has no role menus yet. "
                                              "Create one with `/reactionrole create`."));
        co_return;
    }

    std::string list;
    for (const auto& m : menus) {
        auto opts = role_menus::options(m.id);
        list += "**" + util::escape(m.name) + "** (" + std::to_string(opts.size()) + " role(s)) — " +
                util::escape(m.title) + "\n";
    }

    dpp::embed e = util::base_embed(bot, "Role Menus", util::COLOR_PRIMARY,
                                    std::to_string(menus.size()) + " menu(s):\n\n" + list);

    co_await event.co_reply(dpp::message(e));
    co_return;
}

} // namespace

void add_rolemenu_commands(const dpp::cluster& bot,
                           std::vector<dpp::slashcommand>& definitions,
                           std::unordered_map<std::string, handler_t>& handlers) {
    dpp::slashcommand reactionrole("reactionrole", "Manage reaction role menus (dropdown role picker).", bot.me.id);
    reactionrole.set_default_permissions(dpp::p_manage_guild);

    dpp::command_option create(dpp::co_sub_command, "create", "Create a new role menu.");
    create.add_option(dpp::command_option(dpp::co_string, "name", "Menu name (used by other subcommands)", true));
    create.add_option(dpp::command_option(dpp::co_string, "title", "Title of the posted embed", true));
    reactionrole.add_option(create);

    dpp::command_option add(dpp::co_sub_command, "add", "Add a role to a menu.");
    add.add_option(dpp::command_option(dpp::co_string, "name", "Menu name", true));
    add.add_option(dpp::command_option(dpp::co_role, "role", "Role to add", true));
    add.add_option(dpp::command_option(dpp::co_string, "label", "Label shown in the dropdown (defaults to role name)", false));
    reactionrole.add_option(add);

    dpp::command_option post(dpp::co_sub_command, "post", "Post the menu as a message.");
    post.add_option(dpp::command_option(dpp::co_string, "name", "Menu name", true));
    post.add_option(dpp::command_option(dpp::co_channel, "channel", "Channel to post in (defaults to current)", false));
    reactionrole.add_option(post);

    dpp::command_option remove(dpp::co_sub_command, "remove", "Delete a menu.");
    remove.add_option(dpp::command_option(dpp::co_string, "name", "Menu name", true));
    reactionrole.add_option(remove);

    reactionrole.add_option(dpp::command_option(dpp::co_sub_command, "list", "List all menus."));

    definitions.emplace_back(reactionrole);
    handlers["reactionrole"] = make_handler([](dpp::cluster& bot, const dpp::slashcommand_t& event) -> dpp::task<void> {
        if (!util::require_permission(event, dpp::p_manage_guild, "Manage Server")) {
            co_return;
        }
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

        if (sub == "create")    co_await sub_create(bot, event, guild_id);
        else if (sub == "add")  co_await sub_add(bot, event, guild_id);
        else if (sub == "post") co_await sub_post(bot, event, guild_id);
        else if (sub == "remove") co_await sub_remove(bot, event, guild_id);
        else                    co_await sub_list(bot, event, guild_id);

        co_return;
    });
}

} // namespace cmd

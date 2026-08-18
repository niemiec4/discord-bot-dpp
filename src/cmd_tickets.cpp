#include "commands.h"

#include "tickets.h"

#include <string>

namespace cmd {

namespace {

dpp::task<void> cmd_ticket_setup(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!util::require_permission(event, dpp::p_manage_guild, "Manage Server")) co_return;
    if (!util::require_bot_permission(bot, event, dpp::p_manage_channels, "Manage Channels")) co_return;

    dpp::embed e = util::base_embed(bot, "Support Tickets", util::COLOR_PRIMARY,
                                    "Click the button below to open a private support ticket. "
                                    "A staff member will help you as soon as possible.")
        .set_timestamp(time(nullptr));

    dpp::component button;
    button.set_label("Open a ticket")
          .set_style(dpp::cos_primary)
          .set_id("ticket:open");

    dpp::message msg(event.command.channel_id, e);
    msg.add_component(button);

    co_await bot.co_message_create(msg);
    co_await event.co_reply(util::success("Ticket panel posted. Members can now open tickets.", true));
    co_return;
}

dpp::task<void> cmd_ticket_add(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    dpp::snowflake target_id = util::get_user(event, "user");
    if (!target_id) {
        co_await event.co_reply(util::error("You must specify a user."));
        co_return;
    }

    dpp::snowflake channel_id = event.command.channel_id;
    tickets::ticket t;
    if (!tickets::get_by_channel(channel_id, t)) {
        co_await event.co_reply(util::error("This is not a ticket channel."));
        co_return;
    }

    dpp::confirmation_callback_t res = co_await bot.co_channel_edit_permissions(
        channel_id, target_id,
        dpp::p_view_channel | dpp::p_send_messages | dpp::p_read_message_history,
        0, true);
    if (res.is_error()) {
        co_await event.co_reply(util::error("Failed to grant access: " + res.get_error().human_readable));
        co_return;
    }

    co_await event.co_reply(util::success("<@" + std::to_string(static_cast<uint64_t>(target_id)) +
                                          "> can now see this ticket."));
    co_return;
}

dpp::task<void> cmd_ticket_close(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    dpp::snowflake channel_id = event.command.channel_id;
    tickets::ticket t;
    if (!tickets::get_by_channel(channel_id, t)) {
        co_await event.co_reply(util::error("This is not a ticket channel."));
        co_return;
    }

    // The opener or anyone with Manage Channels may close the ticket.
    bool allowed = static_cast<uint64_t>(event.command.usr.id) == static_cast<uint64_t>(t.user_id);
    if (!allowed) {
        const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
        if (guild != nullptr && guild->base_permissions(event.command.member).can(dpp::p_manage_channels)) {
            allowed = true;
        }
    }
    if (!allowed) {
        co_await event.co_reply(util::error("Only the ticket owner or staff with **Manage Channels** "
                                            "can close this ticket."));
        co_return;
    }

    dpp::embed e = util::base_embed(bot, "Ticket Closed", util::COLOR_WARNING,
                                    "This ticket is being closed. The channel will be deleted shortly.")
        .set_timestamp(time(nullptr));

    tickets::close(channel_id);
    co_await event.co_reply(dpp::message(e));

    // Give Discord a moment to deliver the reply, then remove the channel.
    co_await bot.co_sleep(3);
    bot.channel_delete(channel_id);
    co_return;
}

} // namespace

void add_ticket_commands(const dpp::cluster& bot,
                         std::vector<dpp::slashcommand>& definitions,
                         std::unordered_map<std::string, handler_t>& handlers) {
    dpp::slashcommand ticket("ticket", "Support tickets (setup panel, manage a ticket).", bot.me.id);

    ticket.add_option(dpp::command_option(dpp::co_sub_command, "setup", "Post the open-ticket button panel."));
    ticket.add_option(dpp::command_option(dpp::co_sub_command, "close", "Close the current ticket."));

    dpp::command_option add(dpp::co_sub_command, "add", "Give another user access to the ticket.");
    add.add_option(dpp::command_option(dpp::co_user, "user", "User to grant access to", true));
    ticket.add_option(add);

    definitions.emplace_back(ticket);
    handlers["ticket"] = make_handler([](dpp::cluster& bot, const dpp::slashcommand_t& event) -> dpp::task<void> {
        if (!event.command.guild_id) {
            co_await event.co_reply(util::error("This command can only be used inside a server."));
            co_return;
        }
        std::string sub = "setup";
        try {
            const auto& data = std::get<dpp::command_interaction>(event.command.data);
            if (!data.options.empty() && data.options[0].type == dpp::co_sub_command) {
                sub = data.options[0].name;
            }
        } catch (const std::bad_variant_access&) {
            // not a command interaction
        }

        if (sub == "close")  co_await cmd_ticket_close(bot, event);
        else if (sub == "add") co_await cmd_ticket_add(bot, event);
        else                  co_await cmd_ticket_setup(bot, event);

        co_return;
    });
}

} // namespace cmd

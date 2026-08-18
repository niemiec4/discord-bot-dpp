#include "commands.h"

#include "db.h"
#include "giveaways.h"

#include <string>

namespace cmd {

namespace {

dpp::task<void> cmd_giveaway_start(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!util::require_permission(event, dpp::p_manage_guild, "Manage Server")) co_return;

    uint64_t seconds = util::parse_duration(util::get_string(event, "duration"));
    if (seconds == 0) {
        co_await event.co_reply(util::error("Invalid duration. Examples: `10m`, `2h`, `1d` (max 30 days)."));
        co_return;
    }
    std::string prize = util::get_string(event, "prize");
    if (prize.empty() || prize.size() > 256) {
        co_await event.co_reply(util::error("The prize must be 1-256 characters."));
        co_return;
    }
    int64_t winners = std::clamp<int64_t>(util::get_integer(event, "winners", 1), 1, 20);

    dpp::snowflake channel_id = event.command.channel_id;
    dpp::command_value raw_channel = event.get_parameter("channel");
    if (std::holds_alternative<dpp::snowflake>(raw_channel)) {
        channel_id = std::get<dpp::snowflake>(raw_channel);
    }
    if (!channel_id) {
        co_await event.co_reply(util::error("This command can only be used in a channel."));
        co_return;
    }

    time_t end = time(nullptr) + static_cast<time_t>(seconds);

    dpp::component button;
    button.set_label("Join giveaway")
          .set_style(dpp::cos_success)
          .set_id("giveaway:join"); // id replaced after creation

    // Placeholder message so we can store its id before adding the real one.
    dpp::embed placeholder = dpp::embed()
        .set_color(util::COLOR_SUCCESS)
        .set_title("Giveaway")
        .set_description(util::escape(prize))
        .add_field("Ends", util::rel_time(end), true)
        .add_field("Winners", std::to_string(winners), true)
        .add_field("Hosted by", event.command.usr.get_mention(), true)
        .set_timestamp(time(nullptr));

    dpp::message msg(channel_id, placeholder);

    dpp::confirmation_callback_t sent = co_await bot.co_message_create(msg);
    if (sent.is_error()) {
        co_await event.co_reply(util::error("Failed to post the giveaway: " + sent.get_error().human_readable));
        co_return;
    }

    dpp::message posted = std::get<dpp::message>(sent.value);
    int64_t id = giveaways::create(event.command.guild_id, channel_id, posted.id,
                                   prize, winners, end, event.command.usr.id);

    // Attach the join button (with the giveaway id) to the posted message.
    dpp::component join_button;
    join_button.set_label("Join giveaway")
               .set_style(dpp::cos_success)
               .set_id("giveaway:" + std::to_string(id) + ":join");
    posted.add_component(join_button);
    bot.message_edit(posted);

    dpp::embed e = util::base_embed(bot, "Giveaway Started", util::COLOR_SUCCESS,
                                    "**" + util::escape(prize) + "** — ends " + util::rel_time(end) + ".")
        .add_field("Winners", std::to_string(winners), true)
        .add_field("Channel", "<#" + std::to_string(static_cast<uint64_t>(channel_id)) + ">", true);

    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_giveaway_end(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!util::require_permission(event, dpp::p_manage_guild, "Manage Server")) co_return;

    std::string id_str = util::get_string(event, "message_id");
    dpp::snowflake message_id{id_str};
    if (!message_id) {
        co_await event.co_reply(util::error("That doesn't look like a valid message ID."));
        co_return;
    }

    giveaways::giveaway g;
    if (!giveaways::find_by_message(message_id, g)) {
        co_await event.co_reply(util::warning("No active giveaway found for that message."));
        co_return;
    }

    giveaways::end_now(bot, g);
    co_await event.co_reply(util::success("Giveaway ended. The winners were announced in <#" +
                                          std::to_string(static_cast<uint64_t>(g.channel_id)) + ">."));
    co_return;
}

dpp::task<void> cmd_giveaway_reroll(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!util::require_permission(event, dpp::p_manage_guild, "Manage Server")) co_return;

    std::string id_str = util::get_string(event, "message_id");
    dpp::snowflake message_id{id_str};
    if (!message_id) {
        co_await event.co_reply(util::error("That doesn't look like a valid message ID."));
        co_return;
    }

    // Reroll uses the last ended giveaway for this message id: search all giveaways.
    giveaways::giveaway g;
    {
        // find_by_message only returns active ones, so scan raw rows for any
        // giveaway with this message id.
        bool found = false;
        db::stmt s("SELECT id, guild_id, channel_id, message_id, prize, winners, end_time, host_id "
                   "FROM giveaways WHERE message_id = ? ORDER BY id DESC LIMIT 1");
        s.bind(1, static_cast<int64_t>(message_id));
        if (s.step()) {
            g.id = s.col_int(0);
            g.guild_id = dpp::snowflake{static_cast<uint64_t>(s.col_int(1))};
            g.channel_id = dpp::snowflake{static_cast<uint64_t>(s.col_int(2))};
            g.message_id = dpp::snowflake{static_cast<uint64_t>(s.col_int(3))};
            g.prize = s.col_text(4);
            g.winners = s.col_int(5);
            g.end_time = static_cast<time_t>(s.col_int(6));
            g.host_id = dpp::snowflake{static_cast<uint64_t>(s.col_int(7))};
            found = true;
        }
        if (!found) {
            co_await event.co_reply(util::warning("No giveaway found for that message."));
            co_return;
        }
    }

    giveaways::end_now(bot, g);
    co_await event.co_reply(util::success("Giveaway rerolled. New winners announced in <#" +
                                          std::to_string(static_cast<uint64_t>(g.channel_id)) + ">."));
    co_return;
}

} // namespace

void add_giveaway_commands(const dpp::cluster& bot,
                           std::vector<dpp::slashcommand>& definitions,
                           std::unordered_map<std::string, handler_t>& handlers) {
    dpp::slashcommand giveaway("giveaway", "Start and manage giveaways.", bot.me.id);
    giveaway.set_default_permissions(dpp::p_manage_guild);

    dpp::command_option start(dpp::co_sub_command, "start", "Start a new giveaway.");
    start.add_option(dpp::command_option(dpp::co_string, "duration", "Duration e.g. 10m, 2h, 1d (max 30 days)", true));
    start.add_option(dpp::command_option(dpp::co_string, "prize", "What is being given away?", true));
    start.add_option(dpp::command_option(dpp::co_integer, "winners", "Number of winners (1-20, default 1)", false).set_min_value(1).set_max_value(20));
    start.add_option(dpp::command_option(dpp::co_channel, "channel", "Channel for the giveaway (defaults to current)", false));
    giveaway.add_option(start);

    dpp::command_option end(dpp::co_sub_command, "end", "End a giveaway early and pick winners.");
    end.add_option(dpp::command_option(dpp::co_string, "message_id", "ID of the giveaway message", true));
    giveaway.add_option(end);

    dpp::command_option reroll(dpp::co_sub_command, "reroll", "Reroll a finished giveaway.");
    reroll.add_option(dpp::command_option(dpp::co_string, "message_id", "ID of the giveaway message", true));
    giveaway.add_option(reroll);

    definitions.emplace_back(giveaway);
    handlers["giveaway"] = make_handler([](dpp::cluster& bot, const dpp::slashcommand_t& event) -> dpp::task<void> {
        if (!event.command.guild_id) {
            co_await event.co_reply(util::error("This command can only be used inside a server."));
            co_return;
        }
        std::string sub = "start";
        try {
            const auto& data = std::get<dpp::command_interaction>(event.command.data);
            if (!data.options.empty() && data.options[0].type == dpp::co_sub_command) {
                sub = data.options[0].name;
            }
        } catch (const std::bad_variant_access&) {
            // not a command interaction
        }

        if (sub == "end")      co_await cmd_giveaway_end(bot, event);
        else if (sub == "reroll") co_await cmd_giveaway_reroll(bot, event);
        else                   co_await cmd_giveaway_start(bot, event);

        co_return;
    });
}

} // namespace cmd

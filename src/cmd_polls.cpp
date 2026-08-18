#include "commands.h"

#include "polls.h"

#include <string>
#include <vector>

namespace cmd {

namespace {

dpp::task<void> cmd_poll(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    std::string question = util::get_string(event, "question");
    if (question.empty() || question.size() > 256) {
        co_await event.co_reply(util::error("The question must be 1-256 characters."));
        co_return;
    }

    std::vector<std::string> options;
    static const char* opt_names[] = {"option1", "option2", "option3", "option4", "option5"};
    for (const char* name : opt_names) {
        std::string value = util::get_string(event, name);
        if (!value.empty()) {
            options.push_back(value);
        }
    }
    if (options.size() < 2) {
        co_await event.co_reply(util::error("You need at least two options."));
        co_return;
    }
    if (options.size() > 5) {
        co_await event.co_reply(util::error("Maximum 5 options."));
        co_return;
    }
    for (const std::string& o : options) {
        if (o.size() > 80) {
            co_await event.co_reply(util::error("Options must be 1-80 characters each."));
            co_return;
        }
    }

    int64_t id = polls::create(event.command.guild_id, event.command.channel_id,
                               event.command.usr.id, question, options);

    std::string description;
    for (size_t i = 0; i < options.size(); ++i) {
        description += "**" + std::to_string(i + 1) + ".** " + util::escape(options[i]) + "\n";
    }

    dpp::embed e = dpp::embed()
        .set_color(util::COLOR_PRIMARY)
        .set_title(util::escape(question))
        .set_description(description)
        .set_footer(dpp::embed_footer().set_text("Click an option to vote — click again to change your vote"))
        .set_timestamp(time(nullptr));

    // Buttons: one per option + an end button for the author.
    dpp::component row;
    for (size_t i = 0; i < options.size(); ++i) {
        row.add_component(dpp::component()
            .set_label(std::to_string(i + 1))
            .set_style(static_cast<dpp::component_style>(dpp::cos_primary + static_cast<int>(i) % 4))
            .set_id("poll:" + std::to_string(id) + ":" + std::to_string(i)));
    }

    dpp::message msg(event.command.channel_id, e);
    msg.add_component(row);

    dpp::component end_row;
    end_row.add_component(dpp::component()
        .set_label("End poll")
        .set_style(dpp::cos_danger)
        .set_id("poll:" + std::to_string(id) + ":end"));
    msg.add_component(end_row);

    dpp::confirmation_callback_t sent = co_await bot.co_message_create(msg);
    if (sent.is_error()) {
        co_await event.co_reply(util::error("Failed to post the poll: " + sent.get_error().human_readable));
        co_return;
    }

    dpp::message posted = std::get<dpp::message>(sent.value);
    polls::set_message_id(id, posted.id);

    co_await event.co_reply(util::success("Poll created.", true));
    co_return;
}

} // namespace

void add_poll_commands(const dpp::cluster& bot,
                       std::vector<dpp::slashcommand>& definitions,
                       std::unordered_map<std::string, handler_t>& handlers) {
    auto& poll = definitions.emplace_back(dpp::slashcommand("poll", "Create a poll with clickable options.", bot.me.id));
    poll.add_option(dpp::command_option(dpp::co_string, "question", "The poll question", true));
    poll.add_option(dpp::command_option(dpp::co_string, "option1", "First option", true));
    poll.add_option(dpp::command_option(dpp::co_string, "option2", "Second option", true));
    poll.add_option(dpp::command_option(dpp::co_string, "option3", "Third option (optional)", false));
    poll.add_option(dpp::command_option(dpp::co_string, "option4", "Fourth option (optional)", false));
    poll.add_option(dpp::command_option(dpp::co_string, "option5", "Fifth option (optional)", false));
    handlers["poll"] = make_handler(cmd_poll);
}

} // namespace cmd

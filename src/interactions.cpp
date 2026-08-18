#include "interactions.h"

#include "bot_utils.h"
#include "db.h"
#include "giveaways.h"
#include "polls.h"
#include "role_menus.h"
#include "tickets.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace interactions {

namespace {

/** @brief Split "prefix:arg1:arg2" style custom ids. */
std::vector<std::string> split_custom_id(const std::string& id, char sep = ':') {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        size_t pos = id.find(sep, start);
        if (pos == std::string::npos) {
            out.push_back(id.substr(start));
            break;
        }
        out.push_back(id.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

// ── Tickets ──────────────────────────────────────────────────────────────

dpp::task<void> open_ticket(dpp::cluster& bot, const dpp::button_click_t& event) {
    dpp::snowflake guild_id = event.command.guild_id;
    if (!guild_id) {
        co_await event.co_reply(dpp::message("This button only works inside a server.").set_flags(dpp::m_ephemeral));
        co_return;
    }

    // One open ticket per user.
    dpp::snowflake existing{0};
    if (tickets::has_open_ticket(guild_id, event.command.usr.id, existing)) {
        dpp::message msg(util::warning("You already have an open ticket: <#" +
                                       std::to_string(static_cast<uint64_t>(existing)) + ">."));
        msg.set_flags(dpp::m_ephemeral);
        co_await event.co_reply(msg);
        co_return;
    }

    const dpp::guild* guild = dpp::find_guild(guild_id);
    if (!guild) {
        co_await event.co_reply(dpp::message(util::error("Server not found in cache.")));
        co_return;
    }

    // Channel name from the user's name, sanitized.
    std::string base = event.command.usr.username;
    std::string name = "ticket-";
    for (char c : base) {
        char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (std::isalnum(static_cast<unsigned char>(lower)) || lower == '-') {
            name += lower;
        }
    }
    if (name.size() > 80) {
        name.resize(80);
    }

    dpp::channel ch;
    ch.set_name(name);
    ch.set_guild_id(guild_id);
    ch.set_type(dpp::CHANNEL_TEXT);

    // Deny @everyone, allow the opener and the bot.
    ch.permission_overwrites.emplace_back(dpp::permission_overwrite(guild_id, 0, dpp::p_view_channel, dpp::ot_role));
    ch.permission_overwrites.emplace_back(dpp::permission_overwrite(
        event.command.usr.id,
        dpp::p_view_channel | dpp::p_send_messages | dpp::p_read_message_history |
            dpp::p_attach_files | dpp::p_embed_links,
        0, dpp::ot_member));
    ch.permission_overwrites.emplace_back(dpp::permission_overwrite(
        bot.me.id,
        dpp::p_view_channel | dpp::p_send_messages | dpp::p_read_message_history | dpp::p_manage_channels,
        0, dpp::ot_member));

    co_await event.co_thinking(true);

    dpp::confirmation_callback_t res = co_await bot.co_channel_create(ch);
    if (res.is_error()) {
        co_await event.co_edit_original_response(
            util::error("Failed to create the ticket: " + res.get_error().human_readable));
        co_return;
    }

    dpp::channel created = std::get<dpp::channel>(res.value);
    tickets::create(guild_id, created.id, event.command.usr.id);

    dpp::embed intro = dpp::embed()
        .set_color(util::COLOR_PRIMARY)
        .set_title("Support Ticket")
        .set_description("Hello <@" + std::to_string(static_cast<uint64_t>(event.command.usr.id)) +
                         ">! Describe your issue here. A staff member will reply shortly.\n\n"
                         "Close the ticket with `/ticket close`.")
        .set_timestamp(time(nullptr));
    bot.message_create(dpp::message(created.id, intro));

    co_await event.co_edit_original_response(util::success("Your ticket is ready: <#" +
                                                           std::to_string(static_cast<uint64_t>(created.id)) + ">."));
    co_return;
}

// ── Giveaways ────────────────────────────────────────────────────────────

dpp::task<void> join_giveaway(dpp::cluster& /*bot*/, const dpp::button_click_t& event, int64_t id) {
    if (!giveaways::is_active(id)) {
        dpp::message msg(util::warning("This giveaway has already ended."));
        msg.set_flags(dpp::m_ephemeral);
        co_await event.co_reply(msg);
        co_return;
    }

    bool joined = giveaways::join(id, event.command.usr.id);
    std::string text = joined
        ? "You joined the giveaway. Good luck!"
        : "You are already in this giveaway.";

    dpp::message msg(util::success(text));
    msg.set_flags(dpp::m_ephemeral);
    co_await event.co_reply(msg);
    co_return;
}

// ── Polls ────────────────────────────────────────────────────────────────

dpp::task<void> render_poll(dpp::cluster& bot, const dpp::button_click_t& event, int64_t id,
                            bool& found) {
    found = false;
    std::string question;
    std::vector<std::string> options;
    bool ended = false;
    if (!polls::info(id, question, options, ended)) {
        co_return;
    }
    found = true;

    auto counts = polls::counts(id);
    int64_t total = 0;
    for (int64_t c : counts) {
        total += c;
    }

    std::string description;
    for (size_t i = 0; i < options.size(); ++i) {
        int64_t c = i < counts.size() ? counts[i] : 0;
        double pct = total > 0 ? 100.0 * static_cast<double>(c) / static_cast<double>(total) : 0.0;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.0f%%", pct);
        description += "**" + std::to_string(i + 1) + ".** " + util::escape(options[i]) +
                       " — " + std::to_string(c) + " vote(s) (" + buf + ")\n";
    }
    if (ended) {
        description += "\n*This poll has ended.*";
    }

    dpp::confirmation_callback_t fetched = co_await bot.co_message_get(event.command.message_id, event.command.channel_id);
    if (fetched.is_error()) {
        co_return;
    }
    dpp::message msg = std::get<dpp::message>(fetched.value);
    if (!msg.embeds.empty()) {
        msg.embeds[0].description = description;
    }
    if (ended) {
        for (auto& row : msg.components) {
            for (auto& comp : row.components) {
                comp.disabled = true;
            }
        }
    }
    bot.message_edit(msg);
    co_return;
}

dpp::task<void> vote_poll(dpp::cluster& bot, const dpp::button_click_t& event, int64_t id, int option) {
    int64_t count = polls::vote(id, event.command.usr.id, option);
    if (count < 0) {
        dpp::message msg(util::warning("This poll has already ended."));
        msg.set_flags(dpp::m_ephemeral);
        co_await event.co_reply(msg);
        co_return;
    }

    bool found = false;
    co_await render_poll(bot, event, id, found);

    std::string text = polls::has_voted(id, event.command.usr.id)
        ? "Vote registered for option **" + std::to_string(option + 1) + "**."
        : "Your vote was removed.";
    dpp::message msg(util::success(text));
    msg.set_flags(dpp::m_ephemeral);
    co_await event.co_reply(msg);
    co_return;
}

dpp::task<void> end_poll(dpp::cluster& bot, const dpp::button_click_t& event, int64_t id) {
    if (static_cast<uint64_t>(polls::author_id(id)) != static_cast<uint64_t>(event.command.usr.id)) {
        dpp::message msg(util::error("Only the poll author can end the poll."));
        msg.set_flags(dpp::m_ephemeral);
        co_await event.co_reply(msg);
        co_return;
    }

    bool ended_now = polls::end(id);
    if (!ended_now) {
        dpp::message msg(util::warning("This poll has already ended."));
        msg.set_flags(dpp::m_ephemeral);
        co_await event.co_reply(msg);
        co_return;
    }

    bool found = false;
    co_await render_poll(bot, event, id, found);

    dpp::message msg(util::success("Poll ended. Final results are shown above."));
    msg.set_flags(dpp::m_ephemeral);
    co_await event.co_reply(msg);
    co_return;
}

// ── Role menus ───────────────────────────────────────────────────────────

void toggle_roles(dpp::cluster& bot, const dpp::select_click_t& event, int64_t menu_id) {
    auto options = role_menus::options(menu_id);
    if (options.empty()) {
        return;
    }

    dpp::guild_member member = dpp::find_guild_member(event.command.guild_id, event.command.usr.id);
    if (!member.user_id) {
        event.reply(dpp::message(util::error("Could not find your membership in the cache."))
            .set_flags(dpp::m_ephemeral));
        return;
    }

    std::vector<std::string> changes;
    const auto& member_roles = member.get_roles();

    for (const std::string& value : event.values) {
        dpp::snowflake role_id{value};
        if (!role_id) {
            continue;
        }
        bool has = std::find(member_roles.begin(), member_roles.end(), role_id) != member_roles.end();
        if (has) {
            bot.guild_member_delete_role(event.command.guild_id, event.command.usr.id, role_id);
            changes.push_back("removed <@&" + std::to_string(static_cast<uint64_t>(role_id)) + ">");
        } else {
            bot.guild_member_add_role(event.command.guild_id, event.command.usr.id, role_id);
            changes.push_back("granted <@&" + std::to_string(static_cast<uint64_t>(role_id)) + ">");
        }
    }

    std::string summary = changes.empty() ? "No role changes." : "";
    for (const std::string& c : changes) {
        summary += "• " + c + "\n";
    }

    event.reply(dpp::message(util::success(summary)).set_flags(dpp::m_ephemeral));
}

} // namespace

dpp::task<void> handle_button(dpp::cluster& bot, const dpp::button_click_t& event) {
    std::vector<std::string> parts = split_custom_id(event.custom_id);

    if (parts.size() == 2 && parts[0] == "ticket" && parts[1] == "open") {
        co_await open_ticket(bot, event);
        co_return;
    }

    if (parts.size() == 3 && parts[0] == "giveaway") {
        int64_t id = 0;
        try { id = std::stoll(parts[1]); } catch (...) {}
        if (id > 0 && parts[2] == "join") {
            co_await join_giveaway(bot, event, id);
        }
        co_return;
    }

    if (parts.size() == 3 && parts[0] == "poll") {
        int64_t id = 0;
        try { id = std::stoll(parts[1]); } catch (...) {}
        if (id > 0) {
            if (parts[2] == "end") {
                co_await end_poll(bot, event, id);
            } else {
                int option = 0;
                try { option = std::stoi(parts[2]); } catch (...) {}
                co_await vote_poll(bot, event, id, option);
            }
        }
        co_return;
    }
    co_return;
}

void handle_select(dpp::cluster& bot, const dpp::select_click_t& event) {
    std::vector<std::string> parts = split_custom_id(event.custom_id);
    if (parts.size() == 2 && parts[0] == "rolemenu") {
        int64_t menu_id = 0;
        try { menu_id = std::stoll(parts[1]); } catch (...) {}
        if (menu_id > 0) {
            toggle_roles(bot, event, menu_id);
        }
    }
}

} // namespace interactions

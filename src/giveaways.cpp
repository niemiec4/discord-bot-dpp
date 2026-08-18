#include "giveaways.h"

#include "bot_utils.h"
#include "db.h"

#include <algorithm>
#include <iostream>
#include <random>

namespace giveaways {

namespace {

giveaway row_to_giveaway(db::stmt& s) {
    giveaway g;
    g.id = s.col_int(0);
    g.guild_id = dpp::snowflake{static_cast<uint64_t>(s.col_int(1))};
    g.channel_id = dpp::snowflake{static_cast<uint64_t>(s.col_int(2))};
    g.message_id = dpp::snowflake{static_cast<uint64_t>(s.col_int(3))};
    g.prize = s.col_text(4);
    g.winners = s.col_int(5);
    g.end_time = static_cast<time_t>(s.col_int(6));
    g.host_id = dpp::snowflake{static_cast<uint64_t>(s.col_int(7))};
    return g;
}

/** @brief Randomly pick `count` winners from a participant list. */
std::vector<dpp::snowflake> pick_winners(const std::vector<dpp::snowflake>& participants, int64_t count) {
    std::vector<dpp::snowflake> pool = participants;
    std::shuffle(pool.begin(), pool.end(), std::mt19937{std::random_device{}()});
    if (static_cast<int64_t>(pool.size()) > count) {
        pool.resize(static_cast<size_t>(count));
    }
    return pool;
}

} // namespace

int64_t create(dpp::snowflake guild_id, dpp::snowflake channel_id, dpp::snowflake message_id,
               const std::string& prize, int64_t winners, time_t end_time, dpp::snowflake host_id) {
    db::stmt ins("INSERT INTO giveaways (guild_id, channel_id, message_id, prize, winners, end_time, host_id) "
                 "VALUES (?,?,?,?,?,?,?)");
    ins.bind(1, static_cast<int64_t>(guild_id));
    ins.bind(2, static_cast<int64_t>(channel_id));
    ins.bind(3, static_cast<int64_t>(message_id));
    ins.bind(4, prize);
    ins.bind(5, winners);
    ins.bind(6, static_cast<int64_t>(end_time));
    ins.bind(7, static_cast<int64_t>(host_id));
    ins.step();
    return db::last_insert_rowid();
}

void mark_ended(int64_t id) {
    db::stmt upd("UPDATE giveaways SET ended = 1 WHERE id = ?");
    upd.bind(1, id);
    upd.step();
}

void remove(int64_t id) {
    db::stmt del_entries("DELETE FROM giveaway_entries WHERE giveaway_id = ?");
    del_entries.bind(1, id);
    del_entries.step();
    db::stmt del("DELETE FROM giveaways WHERE id = ?");
    del.bind(1, id);
    del.step();
}

bool join(int64_t id, dpp::snowflake user_id) {
    db::stmt check("SELECT COUNT(*) FROM giveaway_entries WHERE giveaway_id = ? AND user_id = ?");
    check.bind(1, id);
    check.bind(2, static_cast<int64_t>(user_id));
    if (check.step() && check.col_int(0) > 0) {
        return false;
    }
    db::stmt ins("INSERT INTO giveaway_entries (giveaway_id, user_id) VALUES (?,?)");
    ins.bind(1, id);
    ins.bind(2, static_cast<int64_t>(user_id));
    ins.step();
    return true;
}

bool has_joined(int64_t id, dpp::snowflake user_id) {
    db::stmt check("SELECT COUNT(*) FROM giveaway_entries WHERE giveaway_id = ? AND user_id = ?");
    check.bind(1, id);
    check.bind(2, static_cast<int64_t>(user_id));
    return check.step() && check.col_int(0) > 0;
}

std::vector<dpp::snowflake> entries(int64_t id) {
    std::vector<dpp::snowflake> out;
    db::stmt s("SELECT user_id FROM giveaway_entries WHERE giveaway_id = ?");
    s.bind(1, id);
    while (s.step()) {
        out.emplace_back(dpp::snowflake{static_cast<uint64_t>(s.col_int(0))});
    }
    return out;
}

bool is_active(int64_t id) {
    db::stmt s("SELECT COUNT(*) FROM giveaways WHERE id = ? AND ended = 0");
    s.bind(1, id);
    return s.step() && s.col_int(0) > 0;
}

bool find_by_message(dpp::snowflake message_id, giveaway& out) {
    db::stmt s("SELECT id, guild_id, channel_id, message_id, prize, winners, end_time, host_id "
               "FROM giveaways WHERE message_id = ? AND ended = 0");
    s.bind(1, static_cast<int64_t>(message_id));
    if (!s.step()) {
        return false;
    }
    out = row_to_giveaway(s);
    return true;
}

void end_now(dpp::cluster& bot, const giveaway& g) {
    mark_ended(g.id);
    std::vector<dpp::snowflake> participants = entries(g.id);
    std::vector<dpp::snowflake> winners = pick_winners(participants, g.winners);

    std::string winner_text;
    if (winners.empty()) {
        winner_text = "No one joined this giveaway.";
    } else {
        for (const dpp::snowflake& w : winners) {
            winner_text += "<@" + std::to_string(static_cast<uint64_t>(w)) + "> ";
        }
    }

    dpp::embed e = dpp::embed()
        .set_color(util::COLOR_WARNING)
        .set_title("Giveaway Ended")
        .set_description("**" + util::escape(g.prize) + "**")
        .add_field("Winners", winner_text.empty() ? "*none*" : winner_text)
        .add_field("Participants", std::to_string(participants.size()), true)
        .set_timestamp(time(nullptr));

    bot.message_create(dpp::message(g.channel_id, e));

    // Disable the join button on the original giveaway message.
    bot.message_get(g.message_id, g.channel_id, [&bot, g](const dpp::confirmation_callback_t& res) {
        if (res.is_error()) {
            return;
        }
        dpp::message msg = std::get<dpp::message>(res.value);
        for (auto& row : msg.components) {
            for (auto& comp : row.components) {
                comp.disabled = true;
            }
        }
        bot.message_edit(msg);
    });
}

void tick(dpp::cluster& bot) {
    time_t now = time(nullptr);
    std::vector<giveaway> due;
    {
        db::stmt s("SELECT id, guild_id, channel_id, message_id, prize, winners, end_time, host_id "
                   "FROM giveaways WHERE ended = 0 AND end_time <= ?");
        s.bind(1, static_cast<int64_t>(now));
        while (s.step()) {
            due.push_back(row_to_giveaway(s));
        }
    }
    for (const giveaway& g : due) {
        end_now(bot, g);
    }
}

} // namespace giveaways

#include "tickets.h"

#include "db.h"

namespace tickets {

int64_t create(dpp::snowflake guild_id, dpp::snowflake channel_id, dpp::snowflake user_id) {
    db::stmt ins("INSERT INTO tickets (guild_id, channel_id, user_id, created, status) VALUES (?,?,?,?,0)");
    ins.bind(1, static_cast<int64_t>(guild_id));
    ins.bind(2, static_cast<int64_t>(channel_id));
    ins.bind(3, static_cast<int64_t>(user_id));
    ins.bind(4, static_cast<int64_t>(time(nullptr)));
    ins.step();
    return db::last_insert_rowid();
}

bool is_ticket_channel(dpp::snowflake channel_id) {
    db::stmt s("SELECT COUNT(*) FROM tickets WHERE channel_id = ?");
    s.bind(1, static_cast<int64_t>(channel_id));
    return s.step() && s.col_int(0) > 0;
}

bool get_by_channel(dpp::snowflake channel_id, ticket& out) {
    db::stmt s("SELECT id, guild_id, channel_id, user_id, created, status FROM tickets WHERE channel_id = ?");
    s.bind(1, static_cast<int64_t>(channel_id));
    if (!s.step()) {
        return false;
    }
    out.id = s.col_int(0);
    out.guild_id = dpp::snowflake{static_cast<uint64_t>(s.col_int(1))};
    out.channel_id = dpp::snowflake{static_cast<uint64_t>(s.col_int(2))};
    out.user_id = dpp::snowflake{static_cast<uint64_t>(s.col_int(3))};
    out.created = static_cast<time_t>(s.col_int(4));
    out.open = s.col_int(5) == 0;
    return true;
}

bool has_open_ticket(dpp::snowflake guild_id, dpp::snowflake user_id, dpp::snowflake& channel_id) {
    db::stmt s("SELECT channel_id FROM tickets WHERE guild_id = ? AND user_id = ? AND status = 0");
    s.bind(1, static_cast<int64_t>(guild_id));
    s.bind(2, static_cast<int64_t>(user_id));
    if (!s.step()) {
        return false;
    }
    channel_id = dpp::snowflake{static_cast<uint64_t>(s.col_int(0))};
    return true;
}

void close(dpp::snowflake channel_id) {
    db::stmt upd("UPDATE tickets SET status = 1 WHERE channel_id = ?");
    upd.bind(1, static_cast<int64_t>(channel_id));
    upd.step();
}

} // namespace tickets

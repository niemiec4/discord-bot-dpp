#include "warnings.h"

#include "db.h"

#include <algorithm>
#include <ctime>

namespace warns {

void load() {
    // Data now lives in SQLite (initialised by db::init in main).
}

void save() {
    // Data now lives in SQLite (initialised by db::init in main).
}

size_t add(dpp::snowflake guild_id, dpp::snowflake user_id,
           const std::string& reason, dpp::snowflake moderator) {
    // Next warning id within the guild+user pair.
    int64_t next_id = 1;
    {
        db::stmt max_id("SELECT COALESCE(MAX(id), 0) + 1 FROM warnings WHERE guild_id = ? AND user_id = ?");
        max_id.bind(1, static_cast<int64_t>(guild_id));
        max_id.bind(2, static_cast<int64_t>(user_id));
        if (max_id.step()) {
            next_id = max_id.col_int(0);
        }
    }

    db::stmt ins("INSERT INTO warnings (guild_id, user_id, id, moderator, reason, date) VALUES (?,?,?,?,?,?)");
    ins.bind(1, static_cast<int64_t>(guild_id));
    ins.bind(2, static_cast<int64_t>(user_id));
    ins.bind(3, next_id);
    ins.bind(4, static_cast<int64_t>(moderator));
    ins.bind(5, reason);
    ins.bind(6, static_cast<int64_t>(time(nullptr)));
    ins.step();

    return static_cast<size_t>(count(guild_id, user_id));
}

std::vector<entry> get(dpp::snowflake guild_id, dpp::snowflake user_id) {
    std::vector<entry> out;
    db::stmt s("SELECT id, moderator, reason, date FROM warnings "
               "WHERE guild_id = ? AND user_id = ? ORDER BY id ASC");
    s.bind(1, static_cast<int64_t>(guild_id));
    s.bind(2, static_cast<int64_t>(user_id));
    while (s.step()) {
        out.push_back(entry{
            static_cast<uint64_t>(s.col_int(0)),
            dpp::snowflake{static_cast<uint64_t>(s.col_int(1))},
            s.col_text(2),
            static_cast<time_t>(s.col_int(3))
        });
    }
    return out;
}

size_t count(dpp::snowflake guild_id, dpp::snowflake user_id) {
    db::stmt s("SELECT COUNT(*) FROM warnings WHERE guild_id = ? AND user_id = ?");
    s.bind(1, static_cast<int64_t>(guild_id));
    s.bind(2, static_cast<int64_t>(user_id));
    return s.step() ? static_cast<size_t>(s.col_int(0)) : 0;
}

size_t clear(dpp::snowflake guild_id, dpp::snowflake user_id) {
    db::stmt del("DELETE FROM warnings WHERE guild_id = ? AND user_id = ?");
    del.bind(1, static_cast<int64_t>(guild_id));
    del.bind(2, static_cast<int64_t>(user_id));
    del.step();
    return static_cast<size_t>(db::changes());
}

} // namespace warns

#include "economy.h"

#include "db.h"

#include <random>

namespace economy {

namespace {
constexpr time_t DAILY_COOLDOWN = 86400;       // 24 h
constexpr time_t WORK_COOLDOWN = 3600;         // 1 h
constexpr int64_t DAILY_REWARD = 100;
constexpr int64_t WORK_MIN = 20;
constexpr int64_t WORK_MAX = 60;
} // namespace

account get(dpp::snowflake guild_id, dpp::snowflake user_id) {
    account acc;
    db::stmt s("SELECT wallet, last_daily, last_work FROM economy WHERE guild_id = ? AND user_id = ?");
    s.bind(1, static_cast<int64_t>(guild_id));
    s.bind(2, static_cast<int64_t>(user_id));
    if (s.step()) {
        acc.wallet = s.col_int(0);
        acc.last_daily = static_cast<time_t>(s.col_int(1));
        acc.last_work = static_cast<time_t>(s.col_int(2));
    }
    return acc;
}

void add_wallet(dpp::snowflake guild_id, dpp::snowflake user_id, int64_t amount) {
    int64_t balance = std::max<int64_t>(0, get(guild_id, user_id).wallet + amount);
    db::stmt upsert("INSERT INTO economy (guild_id, user_id, wallet) VALUES (?,?,?) "
                    "ON CONFLICT(guild_id, user_id) DO UPDATE SET wallet = excluded.wallet");
    upsert.bind(1, static_cast<int64_t>(guild_id));
    upsert.bind(2, static_cast<int64_t>(user_id));
    upsert.bind(3, balance);
    upsert.step();
}

bool spend(dpp::snowflake guild_id, dpp::snowflake user_id, int64_t amount) {
    if (get(guild_id, user_id).wallet < amount) {
        return false;
    }
    add_wallet(guild_id, user_id, -amount);
    return true;
}

time_t daily_remaining(dpp::snowflake guild_id, dpp::snowflake user_id) {
    time_t last = get(guild_id, user_id).last_daily;
    time_t elapsed = time(nullptr) - last;
    return elapsed >= DAILY_COOLDOWN ? 0 : DAILY_COOLDOWN - elapsed;
}

bool claim_daily(dpp::snowflake guild_id, dpp::snowflake user_id) {
    if (daily_remaining(guild_id, user_id) > 0) {
        return false;
    }
    add_wallet(guild_id, user_id, DAILY_REWARD);
    db::stmt upd("UPDATE economy SET last_daily = ? WHERE guild_id = ? AND user_id = ?");
    upd.bind(1, static_cast<int64_t>(time(nullptr)));
    upd.bind(2, static_cast<int64_t>(guild_id));
    upd.bind(3, static_cast<int64_t>(user_id));
    upd.step();
    return true;
}

time_t work_remaining(dpp::snowflake guild_id, dpp::snowflake user_id) {
    time_t last = get(guild_id, user_id).last_work;
    time_t elapsed = time(nullptr) - last;
    return elapsed >= WORK_COOLDOWN ? 0 : WORK_COOLDOWN - elapsed;
}

int64_t do_work(dpp::snowflake guild_id, dpp::snowflake user_id) {
    if (work_remaining(guild_id, user_id) > 0) {
        return 0;
    }
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int64_t> dist(WORK_MIN, WORK_MAX);
    int64_t reward = dist(rng);
    add_wallet(guild_id, user_id, reward);
    db::stmt upd("UPDATE economy SET last_work = ? WHERE guild_id = ? AND user_id = ?");
    upd.bind(1, static_cast<int64_t>(time(nullptr)));
    upd.bind(2, static_cast<int64_t>(guild_id));
    upd.bind(3, static_cast<int64_t>(user_id));
    upd.step();
    return reward;
}

} // namespace economy

#include "polls.h"

#include "db.h"

#include <dpp/json.h>

namespace polls {

int64_t create(dpp::snowflake guild_id, dpp::snowflake channel_id, dpp::snowflake author_id,
               const std::string& question, const std::vector<std::string>& options) {
    nlohmann::json j = nlohmann::json::array();
    for (const std::string& o : options) {
        j.push_back(o);
    }
    db::stmt ins("INSERT INTO polls (guild_id, channel_id, message_id, author_id, question, options, voters, ended) "
                 "VALUES (?,?,0,?,?,?,?,0)");
    ins.bind(1, static_cast<int64_t>(guild_id));
    ins.bind(2, static_cast<int64_t>(channel_id));
    ins.bind(3, static_cast<int64_t>(author_id));
    ins.bind(4, question);
    ins.bind(5, j.dump());
    ins.bind(6, nlohmann::json::array().dump());
    ins.step();
    return db::last_insert_rowid();
}

void set_message_id(int64_t id, dpp::snowflake message_id) {
    db::stmt upd("UPDATE polls SET message_id = ? WHERE id = ?");
    upd.bind(1, static_cast<int64_t>(message_id));
    upd.bind(2, id);
    upd.step();
}

bool find_by_message(dpp::snowflake message_id, int64_t& id) {
    db::stmt s("SELECT id FROM polls WHERE message_id = ?");
    s.bind(1, static_cast<int64_t>(message_id));
    if (!s.step()) {
        return false;
    }
    id = s.col_int(0);
    return true;
}

bool info(int64_t id, std::string& question, std::vector<std::string>& options, bool& ended) {
    db::stmt s("SELECT question, options, ended FROM polls WHERE id = ?");
    s.bind(1, id);
    if (!s.step()) {
        return false;
    }
    question = s.col_text(0);
    ended = s.col_int(2) != 0;
    try {
        nlohmann::json j = nlohmann::json::parse(s.col_text(1));
        options.clear();
        for (const auto& o : j) {
            options.push_back(o.get<std::string>());
        }
    } catch (...) {
        return false;
    }
    return true;
}

dpp::snowflake author_id(int64_t id) {
    db::stmt s("SELECT author_id FROM polls WHERE id = ?");
    s.bind(1, id);
    if (!s.step()) {
        return dpp::snowflake{0};
    }
    return dpp::snowflake{static_cast<uint64_t>(s.col_int(0))};
}

bool is_open(int64_t id) {
    db::stmt s("SELECT ended FROM polls WHERE id = ?");
    s.bind(1, id);
    return s.step() && s.col_int(0) == 0;
}

bool has_voted(int64_t id, dpp::snowflake user_id) {
    db::stmt s("SELECT voters FROM polls WHERE id = ?");
    s.bind(1, id);
    if (!s.step()) {
        return false;
    }
    try {
        nlohmann::json voters = nlohmann::json::parse(s.col_text(0));
        std::string uid = std::to_string(static_cast<uint64_t>(user_id));
        return voters.contains(uid);
    } catch (...) {
        return false;
    }
}

int64_t vote(int64_t id, dpp::snowflake user_id, int option_index) {
    if (!is_open(id)) {
        return -1;
    }

    std::string voters_json;
    {
        db::stmt s("SELECT voters FROM polls WHERE id = ?");
        s.bind(1, id);
        if (!s.step()) {
            return -1;
        }
        voters_json = s.col_text(0);
    }

    nlohmann::json voters = nlohmann::json::parse(voters_json);
    std::string uid = std::to_string(static_cast<uint64_t>(user_id));

    // Toggle: same option again removes the vote, another option moves it.
    if (voters.contains(uid)) {
        int old = voters[uid].get<int>();
        if (old == option_index) {
            voters.erase(uid);
        } else {
            voters[uid] = option_index;
        }
    } else {
        voters[uid] = option_index;
    }

    db::stmt upd("UPDATE polls SET voters = ? WHERE id = ?");
    upd.bind(1, voters.dump());
    upd.bind(2, id);
    upd.step();

    // Count votes for the chosen option.
    int64_t total = 0;
    for (auto it = voters.begin(); it != voters.end(); ++it) {
        if (it.value().get<int>() == option_index) {
            ++total;
        }
    }
    return total;
}

std::vector<int64_t> counts(int64_t id) {
    db::stmt s("SELECT voters, options FROM polls WHERE id = ?");
    s.bind(1, id);
    if (!s.step()) {
        return {};
    }
    std::vector<int64_t> out;
    try {
        nlohmann::json voters = nlohmann::json::parse(s.col_text(0));
        nlohmann::json options = nlohmann::json::parse(s.col_text(1));
        out.assign(options.size(), 0);
        for (auto it = voters.begin(); it != voters.end(); ++it) {
            int idx = it.value().get<int>();
            if (idx >= 0 && static_cast<size_t>(idx) < out.size()) {
                ++out[static_cast<size_t>(idx)];
            }
        }
    } catch (...) {
        // fall through with empty counts
    }
    return out;
}

bool end(int64_t id) {
    db::stmt upd("UPDATE polls SET ended = 1 WHERE id = ? AND ended = 0");
    upd.bind(1, id);
    upd.step();
    return db::changes() > 0;
}

} // namespace polls

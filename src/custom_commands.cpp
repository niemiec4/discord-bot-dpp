#include "custom_commands.h"

#include "db.h"

#include <cctype>

namespace custom_cmds {

namespace {
/** @brief Normalize a command name: lowercase, strip leading slash. */
std::string normalize(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    size_t start = 0;
    if (!name.empty() && name[0] == '/') {
        start = 1;
    }
    for (size_t i = start; i < name.size(); ++i) {
        out += static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));
    }
    return out;
}
} // namespace

bool set(dpp::snowflake guild_id, const std::string& name, const std::string& response) {
    std::string n = normalize(name);
    if (n.empty() || n.size() > 32) {
        return false;
    }
    if (response.empty() || response.size() > 1024) {
        return false;
    }
    db::stmt upsert("INSERT OR REPLACE INTO custom_commands (guild_id, name, response) VALUES (?,?,?)");
    upsert.bind(1, static_cast<int64_t>(guild_id));
    upsert.bind(2, n);
    upsert.bind(3, response);
    upsert.step();
    return true;
}

bool remove(dpp::snowflake guild_id, const std::string& name) {
    std::string n = normalize(name);
    db::stmt del("DELETE FROM custom_commands WHERE guild_id = ? AND name = ?");
    del.bind(1, static_cast<int64_t>(guild_id));
    del.bind(2, n);
    del.step();
    return db::changes() > 0;
}

bool get(dpp::snowflake guild_id, const std::string& name, std::string& response) {
    std::string n = normalize(name);
    db::stmt s("SELECT response FROM custom_commands WHERE guild_id = ? AND name = ?");
    s.bind(1, static_cast<int64_t>(guild_id));
    s.bind(2, n);
    if (!s.step()) {
        return false;
    }
    response = s.col_text(0);
    return true;
}

std::vector<std::pair<std::string, std::string>> list(dpp::snowflake guild_id) {
    std::vector<std::pair<std::string, std::string>> out;
    db::stmt s("SELECT name, response FROM custom_commands WHERE guild_id = ? ORDER BY name");
    s.bind(1, static_cast<int64_t>(guild_id));
    while (s.step()) {
        out.emplace_back(s.col_text(0), s.col_text(1));
    }
    return out;
}

} // namespace custom_cmds

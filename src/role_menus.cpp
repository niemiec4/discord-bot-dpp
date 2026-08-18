#include "role_menus.h"

#include "db.h"

namespace role_menus {

int64_t create(dpp::snowflake guild_id, const std::string& name, const std::string& title) {
    // Reject duplicate names.
    {
        db::stmt check("SELECT COUNT(*) FROM role_menus WHERE guild_id = ? AND name = ?");
        check.bind(1, static_cast<int64_t>(guild_id));
        check.bind(2, name);
        if (check.step() && check.col_int(0) > 0) {
            return 0;
        }
    }
    db::stmt ins("INSERT INTO role_menus (guild_id, name, title) VALUES (?,?,?)");
    ins.bind(1, static_cast<int64_t>(guild_id));
    ins.bind(2, name);
    ins.bind(3, title);
    ins.step();
    return db::last_insert_rowid();
}

bool add_option(int64_t menu_id, dpp::snowflake role_id, const std::string& label) {
    db::stmt check("SELECT COUNT(*) FROM role_menus WHERE id = ?");
    check.bind(1, menu_id);
    if (!check.step() || check.col_int(0) == 0) {
        return false;
    }
    db::stmt ins("INSERT OR REPLACE INTO role_menu_options (menu_id, role_id, label) VALUES (?,?,?)");
    ins.bind(1, menu_id);
    ins.bind(2, static_cast<int64_t>(role_id));
    ins.bind(3, label);
    ins.step();
    return true;
}

bool remove(dpp::snowflake guild_id, const std::string& name) {
    int64_t menu_id = 0;
    {
        db::stmt find("SELECT id FROM role_menus WHERE guild_id = ? AND name = ?");
        find.bind(1, static_cast<int64_t>(guild_id));
        find.bind(2, name);
        if (!find.step()) {
            return false;
        }
        menu_id = find.col_int(0);
    }
    db::stmt del_options("DELETE FROM role_menu_options WHERE menu_id = ?");
    del_options.bind(1, menu_id);
    del_options.step();

    db::stmt del_menu("DELETE FROM role_menus WHERE id = ?");
    del_menu.bind(1, menu_id);
    del_menu.step();
    return true;
}

std::vector<menu> list(dpp::snowflake guild_id) {
    std::vector<menu> out;
    db::stmt s("SELECT id, name, title FROM role_menus WHERE guild_id = ? ORDER BY name");
    s.bind(1, static_cast<int64_t>(guild_id));
    while (s.step()) {
        out.push_back(menu{s.col_int(0), s.col_text(1), s.col_text(2)});
    }
    return out;
}

bool get_by_id(int64_t menu_id, menu& out) {
    db::stmt s("SELECT id, name, title FROM role_menus WHERE id = ?");
    s.bind(1, menu_id);
    if (!s.step()) {
        return false;
    }
    out = menu{s.col_int(0), s.col_text(1), s.col_text(2)};
    return true;
}

std::vector<std::pair<dpp::snowflake, std::string>> options(int64_t menu_id) {
    std::vector<std::pair<dpp::snowflake, std::string>> out;
    db::stmt s("SELECT role_id, label FROM role_menu_options WHERE menu_id = ? ORDER BY rowid");
    s.bind(1, menu_id);
    while (s.step()) {
        out.emplace_back(dpp::snowflake{static_cast<uint64_t>(s.col_int(0))}, s.col_text(1));
    }
    return out;
}

} // namespace role_menus

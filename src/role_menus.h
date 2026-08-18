#pragma once

#include <dpp/dpp.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief Reaction role menus: each menu owns a select menu posted in a
 * channel; members pick a role from the dropdown to toggle it. Stored in
 * SQLite.
 */
namespace role_menus {

struct menu {
    int64_t id{0};
    std::string name;
    std::string title;
};

/** @brief Create a menu. @return menu id, 0 on failure (duplicate name). */
int64_t create(dpp::snowflake guild_id, const std::string& name, const std::string& title);

/** @brief Add a role option to a menu. @return false when the menu is unknown. */
bool add_option(int64_t menu_id, dpp::snowflake role_id, const std::string& label);

/** @brief Delete a menu and its options. @return false if it did not exist. */
bool remove(dpp::snowflake guild_id, const std::string& name);

/** @brief All menus of a guild. */
std::vector<menu> list(dpp::snowflake guild_id);

/** @brief Look up a menu by id. @return false if unknown. */
bool get_by_id(int64_t menu_id, menu& out);

/** @brief Role options of a menu (role_id, label). */
std::vector<std::pair<dpp::snowflake, std::string>> options(int64_t menu_id);

} // namespace role_menus

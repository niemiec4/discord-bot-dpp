#pragma once

#include <dpp/dpp.h>

/**
 * @brief Handlers for component interactions (buttons and select menus)
 * used by tickets, giveaways, polls and role menus. Called from main.cpp.
 */
namespace interactions {

/** @brief Dispatch a button click by its custom_id prefix (coroutine). */
dpp::task<void> handle_button(dpp::cluster& bot, const dpp::button_click_t& event);

/** @brief Dispatch a select menu change (role menus). */
void handle_select(dpp::cluster& bot, const dpp::select_click_t& event);

} // namespace interactions

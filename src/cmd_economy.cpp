#include "commands.h"

#include "economy.h"

#include <cstdint>
#include <string>

namespace cmd {

namespace {

dpp::task<void> cmd_balance(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    dpp::snowflake target_id = util::get_user(event, "user");
    if (!target_id) {
        target_id = event.command.usr.id;
    }
    if (!event.command.guild_id) {
        co_await event.co_reply(util::error("This command can only be used inside a server."));
        co_return;
    }

    const dpp::user* user = dpp::find_user(target_id);
    std::string name = user ? util::escape(user->format_username())
                            : "<@" + std::to_string(static_cast<uint64_t>(target_id)) + ">";

    economy::account acc = economy::get(event.command.guild_id, target_id);

    dpp::embed e = util::base_embed(bot, "Wallet", util::COLOR_PRIMARY,
                                    "**" + name + "** has **" + std::to_string(acc.wallet) + "** coins.")
        .set_thumbnail(user ? user->get_avatar_url(256) : "");

    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_daily(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!event.command.guild_id) {
        co_await event.co_reply(util::error("This command can only be used inside a server."));
        co_return;
    }
    dpp::snowflake user_id = event.command.usr.id;

    time_t remaining = economy::daily_remaining(event.command.guild_id, user_id);
    if (remaining > 0) {
        co_await event.co_reply(util::warning("You already claimed your daily reward. "
                                              "Come back in **" + util::format_duration(remaining) + "**."));
        co_return;
    }

    economy::claim_daily(event.command.guild_id, user_id);
    int64_t balance = economy::get(event.command.guild_id, user_id).wallet;

    dpp::embed e = util::base_embed(bot, "Daily Reward", util::COLOR_SUCCESS,
                                    "You claimed **100 coins**. Your balance is now **" +
                                    std::to_string(balance) + "** coins.");

    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_pay(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!event.command.guild_id) {
        co_await event.co_reply(util::error("This command can only be used inside a server."));
        co_return;
    }
    dpp::snowflake target_id = util::get_user(event, "user");
    int64_t amount = util::get_integer(event, "amount", 0);

    if (!target_id) {
        co_await event.co_reply(util::error("You must specify a user."));
        co_return;
    }
    if (target_id == event.command.usr.id) {
        co_await event.co_reply(util::error("You cannot pay yourself."));
        co_return;
    }
    if (amount <= 0) {
        co_await event.co_reply(util::error("The amount must be a positive number."));
        co_return;
    }

    if (!economy::spend(event.command.guild_id, event.command.usr.id, amount)) {
        co_await event.co_reply(util::error("You do not have enough coins."));
        co_return;
    }
    economy::add_wallet(event.command.guild_id, target_id, amount);

    int64_t balance = economy::get(event.command.guild_id, event.command.usr.id).wallet;

    dpp::embed e = util::base_embed(bot, "Payment Sent", util::COLOR_SUCCESS,
                                    "You sent **" + std::to_string(amount) + "** coins to <@" +
                                    std::to_string(static_cast<uint64_t>(target_id)) + ">.")
        .add_field("Your balance", std::to_string(balance) + " coins", true);

    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_work(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!event.command.guild_id) {
        co_await event.co_reply(util::error("This command can only be used inside a server."));
        co_return;
    }
    dpp::snowflake user_id = event.command.usr.id;

    time_t remaining = economy::work_remaining(event.command.guild_id, user_id);
    if (remaining > 0) {
        co_await event.co_reply(util::warning("You are tired. You can work again in **" +
                                              util::format_duration(remaining) + "**."));
        co_return;
    }

    int64_t reward = economy::do_work(event.command.guild_id, user_id);
    int64_t balance = economy::get(event.command.guild_id, user_id).wallet;

    dpp::embed e = util::base_embed(bot, "Work Shift", util::COLOR_SUCCESS,
                                    "You worked a shift and earned **" + std::to_string(reward) +
                                    "** coins. Your balance is now **" + std::to_string(balance) + "** coins.")
        .add_field("Next shift", "in 1 hour", true);

    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_gamble(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    if (!event.command.guild_id) {
        co_await event.co_reply(util::error("This command can only be used inside a server."));
        co_return;
    }
    dpp::snowflake user_id = event.command.usr.id;
    int64_t amount = util::get_integer(event, "amount", 0);
    if (amount <= 0) {
        co_await event.co_reply(util::error("The bet must be a positive number."));
        co_return;
    }
    if (!economy::spend(event.command.guild_id, user_id, amount)) {
        co_await event.co_reply(util::error("You do not have enough coins."));
        co_return;
    }

    // 50/50 coin flip: win doubles the bet, lose loses it.
    bool won = (static_cast<uint64_t>(std::rand()) % 2) == 0;
    std::string outcome;
    uint32_t color;
    if (won) {
        economy::add_wallet(event.command.guild_id, user_id, amount * 2);
        outcome = "You won! Your bet of **" + std::to_string(amount) +
                  "** coins doubled to **" + std::to_string(amount * 2) + "**.";
        color = util::COLOR_SUCCESS;
    } else {
        outcome = "You lost your bet of **" + std::to_string(amount) + "** coins.";
        color = util::COLOR_ERROR;
    }

    int64_t balance = economy::get(event.command.guild_id, user_id).wallet;
    dpp::embed e = util::base_embed(bot, "Coin Flip Bet", color, outcome)
        .add_field("Balance", std::to_string(balance) + " coins", true);

    co_await event.co_reply(dpp::message(e));
    co_return;
}

} // namespace

void add_economy_commands(const dpp::cluster& bot,
                          std::vector<dpp::slashcommand>& definitions,
                          std::unordered_map<std::string, handler_t>& handlers) {
    auto& balance = definitions.emplace_back(dpp::slashcommand("balance", "Check your (or someone's) coin balance.", bot.me.id));
    balance.add_option(dpp::command_option(dpp::co_user, "user", "Whose balance? (defaults to you)", false));
    handlers["balance"] = make_handler(cmd_balance);

    definitions.emplace_back(dpp::slashcommand("daily", "Claim your daily coin reward.", bot.me.id));
    handlers["daily"] = make_handler(cmd_daily);

    auto& pay = definitions.emplace_back(dpp::slashcommand("pay", "Send coins to another user.", bot.me.id));
    pay.add_option(dpp::command_option(dpp::co_user, "user", "Who do you want to pay?", true));
    pay.add_option(dpp::command_option(dpp::co_integer, "amount", "How many coins?", true).set_min_value(1));
    handlers["pay"] = make_handler(cmd_pay);

    definitions.emplace_back(dpp::slashcommand("work", "Work a shift and earn coins (1h cooldown).", bot.me.id));
    handlers["work"] = make_handler(cmd_work);

    auto& gamble = definitions.emplace_back(dpp::slashcommand("gamble", "Bet coins on a coin flip (50/50).", bot.me.id));
    gamble.add_option(dpp::command_option(dpp::co_integer, "amount", "How many coins to bet?", true).set_min_value(1));
    handlers["gamble"] = make_handler(cmd_gamble);
}

} // namespace cmd

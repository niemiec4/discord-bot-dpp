#include "commands.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace cmd {

namespace {

int random_int(int min, int max) {
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}

uint32_t parse_color(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower == "red" || lower == "error")    return util::COLOR_ERROR;
    if (lower == "green" || lower == "success") return util::COLOR_SUCCESS;
    if (lower == "yellow" || lower == "warning") return util::COLOR_WARNING;
    if (lower == "blue" || lower == "blurple" || lower == "primary") return util::COLOR_PRIMARY;
    if (lower == "purple") return 0x9B59B6;
    if (lower == "orange") return 0xE67E22;
    if (lower == "pink")   return 0xE91E63;
    if (lower == "black")  return 0x000000;
    if (lower == "white")  return 0xFFFFFF;

    // Try hex like #ff0000 or ff0000
    std::string hex = lower;
    if (!hex.empty() && hex.front() == '#') {
        hex.erase(hex.begin());
    }
    if (hex.size() == 6) {
        char* end = nullptr;
        unsigned long value = std::strtoul(hex.c_str(), &end, 16);
        if (end != nullptr && *end == '\0') {
            return static_cast<uint32_t>(value);
        }
    }
    return util::COLOR_PRIMARY;
}

dpp::task<void> cmd_8ball(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    std::string question = util::get_string(event, "question");
    if (question.empty()) {
        co_await event.co_reply(util::error("You must ask a question."));
        co_return;
    }

    static const std::vector<std::string> answers = {
        "It is certain.", "It is decidedly so.", "Without a doubt.", "Yes — definitely.",
        "You may rely on it.", "As I see it, yes.", "Most likely.", "Outlook good.",
        "Yes.", "Signs point to yes.", "Reply hazy, try again.", "Ask again later.",
        "Better not tell you now.", "Cannot predict now.", "Concentrate and ask again.",
        "Don't count on it.", "My reply is no.", "My sources say no.", "Outlook not so good.",
        "Very doubtful."
    };

    dpp::embed e = util::base_embed(bot, "🎱 Magic 8-Ball", util::COLOR_PRIMARY)
        .add_field("Question", util::escape(question))
        .add_field("Answer", answers[random_int(0, static_cast<int>(answers.size()) - 1)]);

    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_coinflip(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    bool heads = random_int(0, 1) == 0;
    dpp::embed e = util::base_embed(bot, "🪙 Coin Flip", util::COLOR_PRIMARY)
        .set_description(heads ? "**Heads!** 🟡" : "**Tails!** 🪙");

    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_dice(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    int64_t sides = util::get_integer(event, "sides", 6);
    sides = std::clamp<int64_t>(sides, 2, 1000);

    int result = random_int(1, static_cast<int>(sides));

    dpp::embed e = util::base_embed(bot, "🎲 Dice Roll", util::COLOR_PRIMARY)
        .set_description("You rolled a d" + std::to_string(sides) + " and got **" +
                         std::to_string(result) + "**!");

    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_say(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    std::string text = util::get_string(event, "text");
    if (text.empty()) {
        co_await event.co_reply(util::error("You must provide some text."));
        co_return;
    }
    if (text.size() > 2000) {
        co_await event.co_reply(util::error("Text is too long (max 2000 characters)."));
        co_return;
    }

    dpp::embed e = util::base_embed(bot, "💬 " + event.command.usr.format_username(), util::COLOR_PRIMARY)
        .set_description(util::escape(text))
        .set_thumbnail(event.command.usr.get_avatar_url(256));

    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_embed(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    std::string title = util::get_string(event, "title");
    std::string description = util::get_string(event, "description");
    std::string color_name = util::get_string(event, "color");

    if (title.empty() || description.empty()) {
        co_await event.co_reply(util::error("Both `title` and `description` are required."));
        co_return;
    }
    if (title.size() > 256 || description.size() > 4096) {
        co_await event.co_reply(util::error("Title or description is too long."));
        co_return;
    }

    dpp::embed e = util::base_embed(bot, util::escape(title), parse_color(color_name),
                                    util::escape(description))
        .set_author(event.command.usr.format_username(), "", event.command.usr.get_avatar_url(256));

    co_await event.co_reply(dpp::message(e));
    co_return;
}

} // namespace

void add_fun_commands(const dpp::cluster& bot,
                      std::vector<dpp::slashcommand>& definitions,
                      std::unordered_map<std::string, handler_t>& handlers) {
    auto& ball = definitions.emplace_back(dpp::slashcommand("8ball", "Ask the magic 8-ball a question.", bot.me.id));
    ball.add_option(dpp::command_option(dpp::co_string, "question", "Your question", true));
    handlers["8ball"] = make_handler(cmd_8ball);

    definitions.emplace_back(dpp::slashcommand("coinflip", "Flip a coin.", bot.me.id));
    handlers["coinflip"] = make_handler(cmd_coinflip);

    auto& dice = definitions.emplace_back(dpp::slashcommand("dice", "Roll a die.", bot.me.id));
    dice.add_option(dpp::command_option(dpp::co_integer, "sides", "Number of sides (2-1000, default 6)", false).set_min_value(2).set_max_value(1000));
    handlers["dice"] = make_handler(cmd_dice);

    auto& say = definitions.emplace_back(dpp::slashcommand("say", "Make the bot repeat your text.", bot.me.id));
    say.add_option(dpp::command_option(dpp::co_string, "text", "Text to repeat", true));
    handlers["say"] = make_handler(cmd_say);

    auto& embed = definitions.emplace_back(dpp::slashcommand("embed", "Create a custom embed.", bot.me.id));
    embed.add_option(dpp::command_option(dpp::co_string, "title", "Embed title", true));
    embed.add_option(dpp::command_option(dpp::co_string, "description", "Embed description", true));
    embed.add_option(dpp::command_option(dpp::co_string, "color", "Color: red, green, yellow, blue, purple, orange, pink, or #hex", false));
    handlers["embed"] = make_handler(cmd_embed);
}

} // namespace cmd

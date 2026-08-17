#include "commands.h"

#include "levels.h"

#include <algorithm>
#include <string>

namespace cmd {

namespace {

dpp::task<void> cmd_rank(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    dpp::snowflake target_id = util::get_user(event, "user");
    if (!target_id) {
        target_id = event.command.usr.id;
    }

    const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) {
        co_await event.co_reply(util::error("Server not found in cache."));
        co_return;
    }

    const dpp::user* user = dpp::find_user(target_id);
    if (!user) {
        co_await event.co_reply(util::error("User not found in cache."));
        co_return;
    }

    levels::user_level info = levels::get(guild->id, target_id);

    // Rank by XP among the guild's recorded members.
    size_t rank = 1;
    for (const auto& [id, lvl] : levels::top(guild->id, 1000)) {
        if (id == target_id) {
            break;
        }
        ++rank;
    }

    dpp::embed e = util::base_embed(bot, user->format_username(), util::COLOR_PRIMARY,
                                    "Rank **#" + std::to_string(rank) + "** on this server")
        .set_thumbnail(user->get_avatar_url(256))
        .add_field("Level", std::to_string(info.level), true)
        .add_field("Total XP", std::to_string(info.xp), true)
        .add_field("Progress", "`" + levels::progress_bar(info.progress) + "` " +
                               std::to_string(static_cast<int>(info.progress * 100.0)) + "%", true)
        .add_field("To next level", std::to_string(info.xp_to_next) + " XP needed", true);

    co_await event.co_reply(dpp::message(e));
    co_return;
}

dpp::task<void> cmd_top(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) {
        co_await event.co_reply(util::error("Server not found in cache."));
        co_return;
    }

    auto entries = levels::top(guild->id, 10);
    if (entries.empty()) {
        co_await event.co_reply(util::warning("No XP data on this server yet. Start chatting to earn XP!"));
        co_return;
    }

    const char* medals[] = {"🥇", "🥈", "🥉"};
    std::string list;
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& [user_id, info] = entries[i];
        const dpp::user* user = dpp::find_user(user_id);
        std::string name = user ? user->format_username() : "Unknown user";
        std::string prefix = i < 3 ? std::string(medals[i]) + " " : "`#" + std::to_string(i + 1) + "` ";
        list += prefix + "**" + util::escape(name) + "** — level " + std::to_string(info.level) +
                " · " + std::to_string(info.xp) + " XP\n";
    }

    dpp::embed e = util::base_embed(bot, "🏆 Leaderboard", util::COLOR_PRIMARY,
                                    "Top " + std::to_string(entries.size()) + " members on **" +
                                    util::escape(guild->name) + "**:\n\n" + list);

    co_await event.co_reply(dpp::message(e));
    co_return;
}

} // namespace

void add_level_commands(const dpp::cluster& bot,
                        std::vector<dpp::slashcommand>& definitions,
                        std::unordered_map<std::string, handler_t>& handlers) {
    auto& rank = definitions.emplace_back(dpp::slashcommand("rank", "Check your (or someone's) level and XP.", bot.me.id));
    rank.add_option(dpp::command_option(dpp::co_user, "user", "The user to check (defaults to you)", false));
    handlers["rank"] = make_handler(cmd_rank);

    definitions.emplace_back(dpp::slashcommand("top", "Show the server XP leaderboard.", bot.me.id));
    handlers["top"] = make_handler(cmd_top);
}

} // namespace cmd

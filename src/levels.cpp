#include "levels.h"

#include "bot_utils.h"
#include "db.h"
#include "settings.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <mutex>
#include <random>
#include <unordered_map>

namespace levels {

namespace {
// Per-user XP cooldown: one gain per user per 10 seconds (in-memory only).
std::mutex cooldown_mtx;
std::unordered_map<dpp::snowflake, time_t> last_gain;
constexpr time_t XP_COOLDOWN = 10;

// XP awarded per message: uniform between 5 and 15.
constexpr uint64_t XP_MIN = 5;
constexpr uint64_t XP_MAX = 15;
} // namespace

void load() {
    // Data now lives in SQLite (initialised by db::init in main).
}

void save() {
    // Data now lives in SQLite (initialised by db::init in main).
}

uint64_t level_from_xp(uint64_t xp) {
    // Cumulative XP to reach level L is 50 * L * (L + 1).
    double root = std::sqrt(2500.0 + 200.0 * static_cast<double>(xp));
    double level = (root - 50.0) / 100.0;
    return level > 0 ? static_cast<uint64_t>(level) : 0;
}

uint64_t xp_for_level(uint64_t level) {
    return 50ULL * level * (level + 1ULL);
}

user_level get(dpp::snowflake guild_id, dpp::snowflake user_id) {
    user_level out;
    db::stmt s("SELECT xp FROM levels WHERE guild_id = ? AND user_id = ?");
    s.bind(1, static_cast<int64_t>(guild_id));
    s.bind(2, static_cast<int64_t>(user_id));
    if (s.step()) {
        out.xp = static_cast<uint64_t>(s.col_int(0));
    }

    out.level = level_from_xp(out.xp);
    out.xp_into_level = out.xp - xp_for_level(out.level);
    uint64_t next = xp_for_level(out.level + 1);
    out.xp_to_next = next - out.xp;
    uint64_t span = next - xp_for_level(out.level);
    out.progress = span > 0 ? static_cast<double>(out.xp_into_level) / static_cast<double>(span) : 0.0;
    return out;
}

uint64_t add_xp(dpp::snowflake guild_id, dpp::snowflake user_id, uint64_t amount) {
    db::stmt upsert("INSERT INTO levels (guild_id, user_id, xp) VALUES (?,?,?) "
                    "ON CONFLICT(guild_id, user_id) DO UPDATE SET xp = xp + excluded.xp");
    upsert.bind(1, static_cast<int64_t>(guild_id));
    upsert.bind(2, static_cast<int64_t>(user_id));
    upsert.bind(3, static_cast<int64_t>(amount));
    upsert.step();
    return level_from_xp(get(guild_id, user_id).xp);
}

std::vector<std::pair<dpp::snowflake, user_level>> top(dpp::snowflake guild_id, size_t limit) {
    std::vector<std::pair<dpp::snowflake, user_level>> out;
    db::stmt s("SELECT user_id, xp FROM levels WHERE guild_id = ? ORDER BY xp DESC LIMIT ?");
    s.bind(1, static_cast<int64_t>(guild_id));
    s.bind(2, static_cast<int64_t>(limit));
    while (s.step()) {
        user_level info = get(guild_id, dpp::snowflake{static_cast<uint64_t>(s.col_int(0))});
        out.emplace_back(dpp::snowflake{static_cast<uint64_t>(s.col_int(0))}, info);
    }
    return out;
}

std::string progress_bar(double fraction, int width) {
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;
    int filled = static_cast<int>(std::round(fraction * width));

    // UTF-8 block characters: U+2588 (full block), U+2591 (light shade)
    const std::string full = "\xE2\x96\x88";
    const std::string empty = "\xE2\x96\x91";
    std::string out;
    out.reserve(static_cast<size_t>(width) * 3);
    for (int i = 0; i < width; ++i) {
        out += (i < filled) ? full : empty;
    }
    return out;
}

namespace {

std::string replace_all(std::string text, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

void grant_role_rewards(dpp::cluster& bot, const dpp::message& msg, const settings::guild_settings& s,
                        uint64_t new_level) {
    if (s.role_rewards.empty() || new_level == 0) {
        return;
    }

    // Highest reward threshold at or below the new level.
    uint64_t granted_level = 0;
    dpp::snowflake granted_role{0};
    for (const auto& [level, role] : s.role_rewards) {
        if (level <= new_level) {
            granted_level = level;
            granted_role = role;
        }
    }
    if (!granted_level || !granted_role) {
        return;
    }

    dpp::guild_member member = dpp::find_guild_member(msg.guild_id, msg.author.id);
    if (!member.user_id) {
        return;
    }
    const auto& member_roles = member.get_roles();
    if (std::find(member_roles.begin(), member_roles.end(), granted_role) != member_roles.end()) {
        return; // already has the reward role
    }

    // Replace any lower reward roles the member still has.
    for (const auto& [level, role] : s.role_rewards) {
        if (level < granted_level &&
            std::find(member_roles.begin(), member_roles.end(), role) != member_roles.end()) {
            bot.guild_member_delete_role(msg.guild_id, msg.author.id, role);
        }
    }

    bot.guild_member_add_role(msg.guild_id, msg.author.id, granted_role,
                              [](const dpp::confirmation_callback_t& res) {
                                  if (res.is_error()) {
                                      std::cerr << "[levels] Could not grant reward role: "
                                                << res.get_error().human_readable << "\n";
                                  }
                              });
}

} // namespace

void handle_message(dpp::cluster& bot, const dpp::message& msg) {
    // Only in servers, from human users, and not from commands.
    if (!msg.guild_id || msg.author.is_bot()) {
        return;
    }
    if (msg.content.size() > 0 && msg.content[0] == '/') {
        return; // slash command invocation
    }

    settings::guild_settings s = settings::get(msg.guild_id);
    if (!s.leveling_enabled) {
        return;
    }

    // Per-user cooldown.
    {
        std::lock_guard<std::mutex> lock(cooldown_mtx);
        time_t now = time(nullptr);
        auto it = last_gain.find(msg.author.id);
        if (it != last_gain.end() && now - it->second < XP_COOLDOWN) {
            return;
        }
        last_gain[msg.author.id] = now;
    }

    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist(XP_MIN, XP_MAX);

    // XP multipliers: best role multiplier wins, booster bonus stacks on top.
    double mult = 1.0;
    dpp::guild_member member = dpp::find_guild_member(msg.guild_id, msg.author.id);
    if (member.user_id) {
        for (const dpp::snowflake& role_id : member.get_roles()) {
            auto it = s.role_multipliers.find(role_id);
            if (it != s.role_multipliers.end()) {
                mult = std::max(mult, it->second);
            }
        }
        if (member.premium_since != 0 && s.booster_multiplier > 1.0) {
            mult *= s.booster_multiplier;
        }
    }
    uint64_t amount = std::max<uint64_t>(1ULL, static_cast<uint64_t>(std::llround(dist(rng) * mult)));

    uint64_t before = level_from_xp(get(msg.guild_id, msg.author.id).xp);
    uint64_t after = add_xp(msg.guild_id, msg.author.id, amount);

    if (after <= before) {
        return;
    }

    // Level up!
    const dpp::user* user = dpp::find_user(msg.author.id);
    std::string name = user ? user->username : "<@" + std::to_string(static_cast<uint64_t>(msg.author.id)) + ">";

    std::string text = s.levelup_message;
    if (text.empty()) {
        text = "{user} reached **level {level}**.";
    }
    text = replace_all(text, "{user}", name);
    text = replace_all(text, "{level}", std::to_string(after));

    dpp::snowflake channel_id = s.levelup_channel_id ? s.levelup_channel_id : msg.channel_id;
    dpp::embed e = dpp::embed()
        .set_color(util::COLOR_SUCCESS)
        .set_title("Level Up")
        .set_description(text)
        .set_timestamp(time(nullptr));

    bot.message_create(dpp::message(channel_id, e));

    // Grant role rewards for the new level.
    grant_role_rewards(bot, msg, s, after);
}

} // namespace levels

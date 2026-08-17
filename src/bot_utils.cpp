#include "bot_utils.h"

#include "config.h"
#include "settings.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <sstream>

namespace util {

dpp::embed base_embed(const dpp::cluster& bot, const std::string& title,
                      uint32_t color, const std::string& description) {
    dpp::embed embed = dpp::embed()
        .set_color(color)
        .set_title(title)
        .set_timestamp(time(nullptr));

    if (!description.empty()) {
        embed.set_description(description);
    }

    if (bot.me.id) {
        embed.set_footer(
            dpp::embed_footer()
                .set_text(bot.me.username)
                .set_icon(bot.me.get_avatar_url(256))
        );
    }
    return embed;
}

dpp::message error(const std::string& text, bool ephemeral) {
    dpp::message msg(dpp::embed()
        .set_color(COLOR_ERROR)
        .set_title("❌ Error")
        .set_description(text)
        .set_timestamp(time(nullptr)));
    if (ephemeral) {
        msg.set_flags(dpp::m_ephemeral);
    }
    return msg;
}

dpp::message success(const std::string& text, bool ephemeral) {
    dpp::message msg(dpp::embed()
        .set_color(COLOR_SUCCESS)
        .set_title("✅ Success")
        .set_description(text)
        .set_timestamp(time(nullptr)));
    if (ephemeral) {
        msg.set_flags(dpp::m_ephemeral);
    }
    return msg;
}

dpp::message warning(const std::string& text, bool ephemeral) {
    dpp::message msg(dpp::embed()
        .set_color(COLOR_WARNING)
        .set_title("⚠️ Warning")
        .set_description(text)
        .set_timestamp(time(nullptr)));
    if (ephemeral) {
        msg.set_flags(dpp::m_ephemeral);
    }
    return msg;
}

bool require_permission(const dpp::slashcommand_t& event, dpp::permission required,
                        const std::string& perm_name) {
    if (!event.command.guild_id) {
        event.reply(error("This command can only be used inside a server."));
        return false;
    }
    const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (guild == nullptr) {
        event.reply(error("Could not find the server in the cache."));
        return false;
    }
    dpp::permission perms = guild->base_permissions(event.command.member);
    if (!perms.can(required)) {
        event.reply(error("You need the **" + perm_name + "** permission to use this command."));
        return false;
    }
    return true;
}

bool require_bot_permission(dpp::cluster& bot, const dpp::slashcommand_t& event,
                            dpp::permission required, const std::string& perm_name) {
    if (!event.command.guild_id) {
        event.reply(error("This command can only be used inside a server."));
        return false;
    }
    const dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (guild == nullptr) {
        event.reply(error("Could not find the server in the cache."));
        return false;
    }
    dpp::guild_member bot_member = dpp::find_guild_member(guild->id, bot.me.id);
    dpp::permission perms = guild->base_permissions(bot_member);
    if (!perms.can(required)) {
        event.reply(error("I need the **" + perm_name + "** permission to do this. "
                          "Please grant it to my role."));
        return false;
    }
    return true;
}

dpp::snowflake get_user(const dpp::slashcommand_t& event, const std::string& name) {
    dpp::command_value value = event.get_parameter(name);
    if (std::holds_alternative<dpp::snowflake>(value)) {
        return std::get<dpp::snowflake>(value);
    }
    return dpp::snowflake{0};
}

std::string get_string(const dpp::slashcommand_t& event, const std::string& name) {
    dpp::command_value value = event.get_parameter(name);
    if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    }
    return {};
}

int64_t get_integer(const dpp::slashcommand_t& event, const std::string& name, int64_t fallback) {
    dpp::command_value value = event.get_parameter(name);
    if (std::holds_alternative<int64_t>(value)) {
        return std::get<int64_t>(value);
    }
    return fallback;
}

bool get_boolean(const dpp::slashcommand_t& event, const std::string& name) {
    dpp::command_value value = event.get_parameter(name);
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value);
    }
    return false;
}

int64_t highest_role_position(const dpp::guild& guild, const dpp::guild_member& member) {
    (void)guild;
    int64_t highest = 0; // @everyone is always at position 0
    for (const dpp::snowflake& role_id : member.get_roles()) {
        const dpp::role* role = dpp::find_role(role_id);
        if (role != nullptr) {
            highest = std::max(highest, static_cast<int64_t>(role->position));
        }
    }
    return highest;
}

bool hierarchy_allows(const dpp::guild& guild, const dpp::guild_member& actor,
                      const dpp::guild_member& target) {
    // The owner can moderate everyone.
    if (actor.user_id == guild.owner_id) {
        return target.user_id != guild.owner_id;
    }
    // Nobody can moderate the owner.
    if (target.user_id == guild.owner_id) {
        return false;
    }
    return highest_role_position(guild, actor) > highest_role_position(guild, target);
}

void send_log(dpp::cluster& bot, dpp::snowflake guild_id, const dpp::embed& embed) {
    // Per-server setting wins; falls back to the global LOG_CHANNEL_ID env var.
    dpp::snowflake log_channel = settings::get(guild_id).log_channel_id;
    if (!log_channel) {
        log_channel = dpp::snowflake{cfg::get_id("LOG_CHANNEL_ID")};
    }
    if (!log_channel) {
        return;
    }
    bot.message_create(dpp::message(log_channel, embed));
}

std::string escape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '*': out += "\\*"; break;
            case '_': out += "\\_"; break;
            case '~': out += "\\~"; break;
            case '`': out += "\\`"; break;
            case '|': out += "\\|"; break;
            case '>': out += "\\>"; break;
            default:  out += c;      break;
        }
    }
    return out;
}

std::string format_duration(uint64_t seconds) {
    if (seconds == 0) {
        return "0s";
    }
    uint64_t days = seconds / 86400; seconds %= 86400;
    uint64_t hours = seconds / 3600; seconds %= 3600;
    uint64_t mins = seconds / 60;    seconds %= 60;

    std::ostringstream out;
    if (days)    { out << days << "d "; }
    if (hours)   { out << hours << "h "; }
    if (mins)    { out << mins << "m "; }
    if (seconds || (!days && !hours && !mins)) { out << seconds << "s"; }
    return out.str();
}

std::string rel_time(time_t t) {
    return "<t:" + std::to_string(t) + ":R>";
}

std::string full_time(time_t t) {
    return "<t:" + std::to_string(t) + ":f>";
}

uint64_t parse_duration(const std::string& input) {
    if (input.empty()) {
        return 0;
    }

    uint64_t total = 0;
    size_t i = 0;
    const size_t n = input.size();

    while (i < n) {
        size_t num_start = i;
        while (i < n && std::isdigit(static_cast<unsigned char>(input[i]))) {
            ++i;
        }
        if (i == num_start) {
            return 0; // expected a number
        }
        uint64_t amount = 0;
        try {
            amount = std::stoull(input.substr(num_start, i - num_start));
        } catch (...) {
            return 0;
        }

        char unit = 's';
        if (i < n) {
            unit = static_cast<char>(std::tolower(static_cast<unsigned char>(input[i])));
            ++i;
        }

        switch (unit) {
            case 's': total += amount;                       break;
            case 'm': total += amount * 60;                  break;
            case 'h': total += amount * 3600;                break;
            case 'd': total += amount * 86400;               break;
            case 'w': total += amount * 7 * 86400;           break;
            default:  return 0;
        }
    }

    // Discord timeouts are limited to 28 days.
    constexpr uint64_t MAX_TIMEOUT = 28ULL * 86400ULL;
    if (total == 0 || total > MAX_TIMEOUT) {
        return 0;
    }
    return total;
}

} // namespace util

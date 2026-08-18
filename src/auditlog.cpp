#include "auditlog.h"

#include "bot_utils.h"
#include "settings.h"

#include <ctime>

namespace audit {

dpp::snowflake channel_for(dpp::snowflake guild_id) {
    return settings::get(guild_id).audit_channel_id;
}

void send(dpp::cluster& bot, dpp::snowflake guild_id, const std::string& title,
          const std::string& description, uint32_t color, const std::string& extra_field_name,
          const std::string& extra_field_value) {
    dpp::snowflake channel = channel_for(guild_id);
    if (!channel) {
        return;
    }

    dpp::embed e = dpp::embed()
        .set_color(color)
        .set_title(title)
        .set_description(description)
        .set_timestamp(time(nullptr));

    if (!extra_field_name.empty()) {
        e.add_field(extra_field_name, extra_field_value, true);
    }

    if (bot.me.id) {
        e.set_footer(dpp::embed_footer()
            .set_text(bot.me.username)
            .set_icon(bot.me.get_avatar_url(256)));
    }

    bot.message_create(dpp::message(channel, e));
}

} // namespace audit

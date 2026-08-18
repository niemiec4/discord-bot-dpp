#include "snipes.h"

#include <algorithm>
#include <deque>
#include <map>
#include <mutex>

namespace snipes {

namespace {
std::mutex mtx;

// Per channel: ring buffer of recently seen messages (id -> content).
std::map<dpp::snowflake, std::deque<std::pair<dpp::snowflake, dpp::message>>> recent;
constexpr size_t MAX_TRACKED_PER_CHANNEL = 50;

// Per channel: the latest deleted message.
std::map<dpp::snowflake, entry> sniped;
} // namespace

void track(const dpp::message& msg) {
    std::lock_guard<std::mutex> lock(mtx);
    if (msg.content.empty() && msg.attachments.empty()) {
        return;
    }
    auto& queue = recent[msg.channel_id];
    queue.emplace_back(msg.id, msg);
    while (queue.size() > MAX_TRACKED_PER_CHANNEL) {
        queue.pop_front();
    }
}

void on_delete(dpp::snowflake channel_id, dpp::snowflake message_id) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = recent.find(channel_id);
    if (it == recent.end()) {
        return;
    }
    auto& queue = it->second;
    auto found = std::find_if(queue.begin(), queue.end(),
                              [&](const auto& pair) { return pair.first == message_id; });
    if (found == queue.end()) {
        return;
    }

    const dpp::message& msg = found->second;
    entry e;
    e.content = msg.content;
    e.author = msg.author.username;
    e.author_id = std::to_string(static_cast<uint64_t>(msg.author.id));
    e.avatar = msg.author.get_avatar_url(256);
    e.time = static_cast<time_t>(msg.sent);
    if (!msg.attachments.empty()) {
        e.attachment = msg.attachments.front().url;
    }

    queue.erase(found);
    sniped[channel_id] = std::move(e);
}

bool get(dpp::snowflake channel_id, entry& out) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = sniped.find(channel_id);
    if (it == sniped.end()) {
        return false;
    }
    out = it->second;
    return true;
}

} // namespace snipes

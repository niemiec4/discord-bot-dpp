# 🤖 Discord Bot (D++ / dpp)

A modern, feature-rich Discord bot written in C++20 using the
[**D++ (dpp)**](https://dpp.dev) library. Everything is driven by slash commands,
with colorful embeds, coroutines and moderation logging.

## ✨ Features

| Category | Commands |
|---|---|
| 🛡️ **Moderation** | `/kick`, `/ban`, `/unban`, `/timeout`, `/purge`, `/warn`, `/warnings`, `/clearwarns` |
| ℹ️ **Information** | `/ping`, `/help`, `/botinfo`, `/serverinfo`, `/userinfo`, `/avatar`, `/servericon`, `/roleinfo`, `/invite`, `/sync` |
| ⚙️ **Server settings** | `/settings show`, `logchannel`, `welcome`, `welcomemessage`, `leveling`, `levelupchannel`, `levelupmessage`, `rewardadd`, `rewardremove`, `warnthreshold`, `warnaction`, `warntimeout`, `xpmult`, `xpmultremove`, `xpboost` |
| 📈 **Levels** | `/rank`, `/top` |
| 🎉 **Fun** | `/8ball`, `/coinflip`, `/dice`, `/say`, `/embed` |

Highlights:

- **Safe moderation** — checks the invoker's *and* the bot's permissions, enforces
  the role hierarchy, and protects the server owner from kick/ban/timeout.
- **Warning system (`/warn`)** — persisted to `data/warnings.json`, survives bot restarts.
  Warnings are stored **per server and per user**, so each server keeps its own counts.
- **Moderation logs** — every kick/ban/timeout/purge/warn is posted to the channel
  from `LOG_CHANNEL_ID`.
- **Welcome messages** — optional, posted to `WELCOME_CHANNEL_ID` when someone joins.
- **Per-server settings** — every server configures its own moderation log channel,
  welcome message, leveling on/off, level-up channel and role rewards via `/settings`.
- **Leveling system** — chat to earn XP (one gain per user per minute), level up with
  progress bars (`/rank`) and a leaderboard (`/top`), plus automatic role rewards.
- **Auto-punishment** — when a member reaches a configurable warning threshold
  (`/settings warnthreshold` + `/settings warnaction`), the bot automatically
  times them out, kicks, or bans them.
- **XP multipliers** — per-role multipliers (`/settings xpmult`) and a booster
  bonus (`/settings xpboost`) make leveling fair and rewarding.
- **C++20 coroutines** — clean, sequential async code instead of nested callbacks.
- **Consistent embed style** — footer with the bot's name and avatar, timestamps,
  semantic colors.
- **One bot, every server** — globally registered commands + the `/invite` command
  to add the bot anywhere.

## 📦 Requirements

- CMake ≥ 3.16
- A compiler with C++20 support (GCC 11+, Clang 14+, MSVC 2022)
- **D++ (dpp) ≥ 10.x** — install per the [official docs](https://dpp.dev/install.html)
  (Linux: `sudo apt install libdpp-dev` / `dnf install dpp-devel`)

## 🚀 Setup & run

```bash
# 1. Copy the config template and enter your bot token
cp .env.example .env

# 2. Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# 3. Run
./build/discord_bot
```

### Configuration (`.env`)

| Variable | Description |
|---|---|
| `BOT_TOKEN` | Bot token from https://discord.com/developers/applications **(required)** |
| `GUILD_ID` | Server ID — commands register instantly on that server only. Leave empty for global registration (up to 1 h propagation) |
| `BOT_OWNER_ID` | Your Discord user ID — when set, only you can use `/sync` (otherwise it requires Manage Server) |

> 💾 **Data files** (created automatically in `data/`):
> - `warnings.json` — warnings per server + user
> - `settings.json` — per-server configuration from `/settings`
> - `levels.json` — XP per server + user
| `LOG_CHANNEL_ID` | **Default** channel for moderation logs (optional) — overridable per server with `/settings logchannel` |
| `WELCOME_CHANNEL_ID` | **Default** channel for welcome messages (optional) — overridable per server with `/settings welcome` |

> ⚠️ **Privileged Intents** — in the developer portal (Bot → Privileged Gateway Intents)
> enable **Presence Intent**, **Server Members Intent** and **Message Content Intent**.

## 🌐 Running on multiple servers

- One token = one bot instance that can be added to **any number of servers**.
- Use `/invite` to generate an OAuth2 link (scopes `bot` + `applications.commands`)
  with all the permissions the bot needs, then use it on each server.
- Keep `GUILD_ID` **empty** so commands are registered globally and appear on
  every server where the bot is present.
- Warnings, `/warn`, `/warnings` and `/clearwarns` are fully **per-server**:
  the same user has independent warning counts on each server.
- All `/settings` values (log channel, welcome message, leveling, role rewards)
  are **per-server** and persisted in `data/settings.json`.
- XP/levels (`data/levels.json`) are **per-server too** — the same user levels up
  independently on every server.

## 🗂️ Project structure

```
├── main.cpp               # entry point: cluster, command registration, events
├── CMakeLists.txt
├── .env.example           # configuration template
└── src/
    ├── config.h/.cpp      # .env loading
    ├── bot_utils.h/.cpp   # embeds, permission checks, formatting, logs
    ├── warnings.h/.cpp    # persistent warning system (JSON)
    ├── settings.h/.cpp    # per-server settings (JSON)
    ├── levels.h/.cpp      # XP/leveling system (JSON)
    ├── commands.h/.cpp    # command registry + dispatch
    ├── cmd_moderation.cpp # kick, ban, unban, timeout, purge, warn…
    ├── cmd_utility.cpp    # ping, help, botinfo, serverinfo, userinfo…
    ├── cmd_settings.cpp   # /settings per-server configuration
    ├── cmd_levels.cpp     # /rank, /top
    └── cmd_fun.cpp        # 8ball, coinflip, dice, say, embed
```

## 🔧 Adding a new command

1. Write a handler in the appropriate `cmd_*.cpp` file, e.g.:

```cpp
dpp::task<void> cmd_hello(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    dpp::embed e = util::base_embed(bot, "Hello!", util::COLOR_PRIMARY,
                                    "Hi <@" + std::to_string(static_cast<uint64_t>(event.command.usr.id)) + ">!");
    co_await event.co_reply(dpp::message(e));
    co_return;
}
```

2. Register it in the matching `add_*_commands` function:

```cpp
definitions.emplace_back(dpp::slashcommand("hello", "Say hello.", bot.me.id));
handlers["hello"] = make_handler(cmd_hello);
```

That's it — the command appears after a restart (instantly when `GUILD_ID` is set).

## 📄 License

MIT — use, modify and extend it freely.

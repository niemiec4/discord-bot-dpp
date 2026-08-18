# Discord Bot (D++ / dpp)

A modern, feature-rich Discord bot written in C++20 using the
[**D++ (dpp)**](https://dpp.dev) library. Everything is driven by slash commands,
with consistent embeds, C++20 coroutines and per-server configuration backed by
SQLite.

## Features

| Category | Commands |
|---|---|
| **Moderation** | `/kick`, `/ban`, `/unban`, `/timeout`, `/purge`, `/warn`, `/warnings`, `/clearwarns`, `/lock`, `/unlock`, `/slowmode`, `/snipe` |
| **Information** | `/ping`, `/help`, `/botinfo`, `/serverinfo`, `/userinfo`, `/avatar`, `/servericon`, `/roleinfo`, `/invite`, `/sync` |
| **Server settings** | `/settings` — `show`, `logchannel`, `welcome`, `welcomemessage`, `leveling`, `levelupchannel`, `levelupmessage`, `rewardadd`, `rewardremove`, `warnthreshold`, `warnaction`, `warntimeout`, `xpmult`, `xpmultremove`, `xpboost`, `autorole`, `auditlog` |
| **Levels** | `/rank`, `/top` |
| **Economy** | `/balance`, `/daily`, `/pay`, `/work`, `/gamble` |
| **Engagement** | `/reactionrole`, `/ticket`, `/giveaway`, `/poll`, `/cc` |
| **Fun** | `/8ball`, `/coinflip`, `/dice`, `/say`, `/embed` |

Highlights:

- **Safe moderation** — checks the invoker's *and* the bot's permissions,
  enforces the role hierarchy, and protects the server owner.
- **Warning system** — persisted to SQLite, **per server and per user**.
  Auto-punishment kicks in at a configurable threshold (`/settings warnthreshold`
  + `warnaction` + `warntimeout`).
- **Full audit log** — message edits/deletes, member joins/leaves, bans,
  channel changes — posted to the server's audit channel (`/settings auditlog`).
- **Reaction roles** — `/reactionrole` builds a dropdown role picker; members
  toggle roles with one click.
- **Support tickets** — `/ticket setup` posts an "Open a ticket" button; each
  ticket is a private channel the member and staff can use, closed with
  `/ticket close`.
- **Giveaways** — `/giveaway start/end/reroll`, join via button, winners picked
  automatically (survives restarts).
- **Polls** — `/poll` with clickable options, live vote counts, changeable
  votes and an end button for the author.
- **Economy** — coins from `/daily` and `/work`, transferable with `/pay`,
  gamblable with `/gamble`.
- **Custom commands** — `/cc add` creates server-specific commands (with
  `{user}` / `{channel}` placeholders); built-in names are protected.
- **Leveling** — chat to earn XP (per-server on/off, per-role multipliers,
  booster bonus), `/rank` with progress bars, `/top` leaderboard and automatic
  role rewards.
- **Auto-role** — `/settings autorole` grants a role to every new member.
- **C++20 coroutines** — clean, sequential async code instead of nested callbacks.
- **One bot, every server** — globally registered commands + `/invite`.

## Requirements

- CMake ≥ 3.16
- A compiler with C++20 support (GCC 11+, Clang 14+, MSVC 2022)
- **D++ (dpp) ≥ 10.x** — see the [official docs](https://dpp.dev/install.html)
- **SQLite 3** (runtime library only — the header is bundled in `src/sqlite3.h`)

## Setup & run

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
| `LOG_CHANNEL_ID` | **Default** channel for moderation logs (optional) — overridable per server with `/settings logchannel` |
| `WELCOME_CHANNEL_ID` | **Default** channel for welcome messages (optional) — overridable per server with `/settings welcome` |

> **Data storage** — everything lives in one SQLite database `data/bot.db`
> (warnings, settings, levels, economy, custom commands, role menus, tickets,
> giveaways, polls). On the first run after upgrading from the JSON versions,
> existing `data/*.json` files are imported automatically.

> ⚠️ **Privileged Intents** — in the developer portal (Bot → Privileged Gateway
> Intents) enable **Presence Intent**, **Server Members Intent** and **Message
> Content Intent**.

## Running on multiple servers

- One token = one bot instance that can be added to **any number of servers**.
- Use `/invite` to generate an OAuth2 link (scopes `bot` + `applications.commands`)
  with all the permissions the bot needs.
- Keep `GUILD_ID` **empty** so commands are registered globally and appear on
  every server.
- **Everything is per-server**: warnings, XP/levels, economy, custom commands,
  role menus, tickets and all `/settings` values. The same user has independent
  data on each server — no database per server is needed, one SQLite file handles
  them all.

## Project structure

```
├── main.cpp               # entry point: cluster, command registration, events
├── CMakeLists.txt
├── .env.example           # configuration template
└── src/
    ├── config.h/.cpp      # .env loading
    ├── db.h/.cpp          # SQLite wrapper + schema + JSON migration
    ├── sqlite3.h          # bundled SQLite header
    ├── bot_utils.h/.cpp   # embeds, permission checks, formatting, logs
    ├── warnings.h/.cpp    # warning system (SQLite)
    ├── settings.h/.cpp    # per-server settings (SQLite)
    ├── levels.h/.cpp      # XP/leveling system (SQLite)
    ├── economy.h/.cpp     # coins: daily, work, pay, gamble (SQLite)
    ├── custom_commands.h/.cpp  # per-server /cc commands (SQLite)
    ├── role_menus.h/.cpp  # reaction role menus (SQLite)
    ├── tickets.h/.cpp     # support tickets (SQLite)
    ├── giveaways.h/.cpp   # giveaways + auto-end timer (SQLite)
    ├── polls.h/.cpp       # polls with buttons (SQLite)
    ├── snipes.h/.cpp      # in-memory deleted-message cache
    ├── auditlog.h/.cpp    # full audit log
    ├── interactions.h/.cpp # button/select handlers (tickets, polls, …)
    ├── commands.h/.cpp    # command registry + dispatch
    ├── cmd_moderation.cpp # kick, ban, unban, timeout, purge, warn…
    ├── cmd_mod_extra.cpp  # snipe, lock, unlock, slowmode
    ├── cmd_utility.cpp    # ping, help, botinfo, serverinfo, userinfo…
    ├── cmd_settings.cpp   # /settings per-server configuration
    ├── cmd_levels.cpp     # /rank, /top
    ├── cmd_economy.cpp    # balance, daily, pay, work, gamble
    ├── cmd_custom.cpp     # /cc
    ├── cmd_rolemenu.cpp   # /reactionrole
    ├── cmd_tickets.cpp    # /ticket
    ├── cmd_giveaways.cpp  # /giveaway
    ├── cmd_polls.cpp      # /poll
    └── cmd_fun.cpp        # 8ball, coinflip, dice, say, embed
```

## Adding a new command

1. Write a handler in the appropriate `cmd_*.cpp` file:

```cpp
dpp::task<void> cmd_hello(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    dpp::embed e = util::base_embed(bot, "Hello", util::COLOR_PRIMARY,
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

The command appears after a restart (instantly when `GUILD_ID` is set).

## License

MIT — use, modify and extend it freely.

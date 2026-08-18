# Discord Bot

A modern, feature-rich Discord bot written in C++20 using the
[**D++ (dpp)**](https://dpp.dev) library. Everything is controlled with slash
commands, with consistent embeds, C++20 coroutines and per-server configuration
backed by SQLite.

## Features

| Category | Commands |
|---|---|
| **Moderation** | `/kick`, `/ban`, `/unban`, `/timeout`, `/purge`, `/warn`, `/warnings`, `/clearwarns`, `/lock`, `/unlock`, `/slowmode`, `/snipe` |
| **Information** | `/ping`, `/help`, `/botinfo`, `/serverinfo`, `/userinfo`, `/avatar`, `/servericon`, `/roleinfo`, `/invite`, `/sync` |
| **Server settings** | `/settings` — `show`, `logchannel`, `welcome`, `welcomemessage`, `welcomepreview`, `leveling`, `levelupchannel`, `levelupmessage`, `rewardadd`, `rewardremove`, `warnthreshold`, `warnaction`, `warntimeout`, `xpmult`, `xpmultremove`, `xpboost`, `autorole`, `auditlog` |
| **Levels** | `/rank`, `/top` |
| **Economy** | `/balance`, `/daily`, `/pay`, `/work`, `/gamble` |
| **Engagement** | `/reactionrole`, `/ticket`, `/giveaway`, `/poll`, `/cc` |
| **Fun** | `/8ball`, `/coinflip`, `/dice`, `/say`, `/embed` |

## What the bot can do

- **Moderation** — kick, ban, unban, timeout, purge messages, warn with a
  persistent per-server record, lock/unlock channels, set slowmode, and snipe
  deleted messages. Actions check both the moderator's and the bot's
  permissions and respect the role hierarchy.
- **Auto-punishment** — when a member reaches a configurable warning threshold,
  the bot automatically times them out, kicks or bans them.
- **Full audit log** — message edits and deletions, member joins and leaves,
  bans, channel changes — posted to the server's audit channel.
- **Reaction roles** — dropdown role pickers; members toggle their own roles
  with one click.
- **Support tickets** — an "Open a ticket" button creates a private channel
  for each member; staff and the member can work in it and close it when done.
- **Giveaways** — start, end or reroll; members join with a button and winners
  are picked automatically.
- **Polls** — clickable options, live vote counts, changeable votes and an end
  button for the author.
- **Levels** — earn XP by chatting, view progress with `/rank`, compete on the
  `/top` leaderboard, and unlock role rewards at configured levels. XP can be
  boosted per role and for server boosters.
- **Economy** — collect a daily reward, work shifts for coins, pay other
  members and gamble on a coin flip.
- **Custom commands** — each server can define its own commands with `/cc`
  (built-in names are protected).
- **Welcome cards** — new members are greeted with a polished card showing
  their avatar, join number and account age; the text is customizable and can
  be previewed live before saving.
- **Auto-role** — new members automatically receive a configured role.
- **Professional look** — every response is a clean embed with consistent
  colors, a footer and timestamps. Mention the bot (just the mention, nothing
  else) anywhere and it replies with an info card about itself.

## Running on multiple servers

- One bot instance works on **any number of servers** — just use `/invite` to
  get an invite link with all needed permissions.
- Commands are registered globally, so they appear on every server the bot
  joins.
- **Everything is per-server**: warnings, XP, economy, custom commands, role
  menus, tickets and all `/settings` values. The same user has independent
  data on each server — one SQLite database handles them all.

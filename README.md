# 🤖 Discord Bot (D++ / dpp)

Nowoczesny, funkcjonalny bot Discord napisany w C++20 z użyciem biblioteki
[**D++ (dpp)**](https://dpp.dev). Wszystko oparte na slash commands
(„ukośnikowych” komendach), z kolorowymi embedami, korutynami i logowaniem akcji moderacji.

## ✨ Funkcje

| Kategoria | Komendy |
|---|---|
| 🛡️ **Moderacja** | `/kick`, `/ban`, `/unban`, `/timeout`, `/purge`, `/warn`, `/warnings`, `/clearwarns` |
| ℹ️ **Informacje** | `/ping`, `/help`, `/botinfo`, `/serverinfo`, `/userinfo`, `/avatar`, `/servericon`, `/roleinfo`, `/invite` |
| 🎉 **Zabawa** | `/8ball`, `/coinflip`, `/dice`, `/say`, `/embed` |

Wyróżniki:

- **Bezpieczna moderacja** — sprawdzanie uprawnień (również bota), hierarchii ról
  oraz ochrona właściciela serwera przed kick/ban/timeout.
- **System ostrzeżeń (`/warn`)** — trwały zapis do `data/warnings.json`, odporny na restart bota.
- **Logi moderacji** — każde kick/ban/timeout/purge/warn ląduje w kanale z `LOG_CHANNEL_ID`.
- **Powitalne wiadomości** — opcjonalne `/welcome` dla nowych użytkowników (`WELCOME_CHANNEL_ID`).
- **Korutyny (C++20)** — czytelny, sekwencyjny kod asynchroniczny zamiast zagnieżdżonych callbacków.
- **Jednolity styl embedów** — stopka z avatar i nazwą bota, znaczniki czasu, kolory semantyczne.

## 📦 Wymagania

- CMake ≥ 3.16
- Kompilator z obsługą C++20 (GCC 11+, Clang 14+, MSVC 2022)
- Biblioteka **D++ (dpp) ≥ 10.x** — zainstaluj zgodnie z [dokumentacją](https://dpp.dev/install.html)
  (Linux: `sudo apt install libdpp-dev` / `dnf install dpp-devel`)

## 🚀 Instalacja i uruchomienie

```bash
# 1. Skopiuj konfigurację i wpisz swój token bota
cp .env.example .env

# 2. Zbuduj
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# 3. Uruchom
./build/discord_bot
```

### Konfiguracja (`.env`)

| Zmienna | Opis |
|---|---|
| `BOT_TOKEN` | Token bota z https://discord.com/developers/applications **(wymagane)** |
| `GUILD_ID` | ID serwera — komendy rejestrują się natychmiast tylko tam. Puste = komendy globalne (do 1 h propagacji) |
| `LOG_CHANNEL_ID` | Kanał z logami moderacji (opcjonalnie) |
| `WELCOME_CHANNEL_ID` | Kanał z wiadomościami powitalnymi (opcjonalnie) |

> ⚠️ **Privileged Intents** — w panelu deweloperskim bota (Bot → Privileged Gateway Intents)
> włącz: **Presence Intent**, **Server Members Intent** oraz **Message Content Intent**.

## 🗂️ Struktura projektu

```
├── main.cpp               # wejście: klaster, rejestracja komend, zdarzenia
├── CMakeLists.txt
├── .env.example           # szablon konfiguracji
└── src/
    ├── config.h/.cpp      # wczytywanie .env
    ├── bot_utils.h/.cpp   # embedy, uprawnienia, formatowanie, logi
    ├── warnings.h/.cpp    # trwały system ostrzeżeń (JSON)
    ├── commands.h/.cpp    # rejestr komend + dispatch
    ├── cmd_moderation.cpp # kick, ban, unban, timeout, purge, warn…
    ├── cmd_utility.cpp    # ping, help, botinfo, serverinfo, userinfo…
    └── cmd_fun.cpp        # 8ball, coinflip, dice, say, embed
```

## 🔧 Dodawanie nowej komendy

1. Napisz handler w odpowiednim pliku `cmd_*.cpp`, np.:

```cpp
dpp::task<void> cmd_hello(dpp::cluster& bot, const dpp::slashcommand_t& event) {
    dpp::embed e = util::base_embed(bot, "Hello!", util::COLOR_PRIMARY,
                                    "Hi <@" + std::to_string(static_cast<uint64_t>(event.command.usr.id)) + ">!");
    co_await event.co_reply(dpp::message(e));
    co_return;
}
```

2. Zarejestruj go w funkcji `add_*_commands`:

```cpp
definitions.emplace_back(dpp::slashcommand("hello", "Say hello.", bot.me.id));
handlers["hello"] = make_handler(cmd_hello);
```

Gotowe — komenda pojawi się po restarcie bota (przy `GUILD_ID` natychmiast).

## 📄 Licencja

MIT — używaj, modyfikuj i rozwijaj dowolnie.

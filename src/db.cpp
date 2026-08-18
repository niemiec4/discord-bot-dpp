#include "db.h"

#include "sqlite3.h"

#include <dpp/json.h>

#include <fstream>
#include <iostream>
#include <mutex>
#include <sys/stat.h>

namespace db {

namespace {
const std::string DATA_DIR = "data";
const std::string DB_PATH = DATA_DIR + "/bot.db";

sqlite3* handle_ = nullptr;
std::mutex mtx;
} // namespace

void ensure_dir() {
#ifdef _WIN32
    _mkdir(DATA_DIR.c_str());
#else
    mkdir(DATA_DIR.c_str(), 0755);
#endif
}

struct stmt::Impl {
    sqlite3_stmt* st = nullptr;
    std::string sql;
};

stmt::stmt(const std::string& sql) : impl_(new Impl) {
    impl_->sql = sql;
    std::lock_guard<std::mutex> lock(mtx);
    if (handle_ == nullptr) {
        return; // db not initialised; ok() will report failure
    }
    if (sqlite3_prepare_v2(handle_, sql.c_str(), -1, &impl_->st, nullptr) != SQLITE_OK) {
        std::cerr << "[db] prepare failed: " << sqlite3_errmsg(handle_) << "\n  SQL: " << sql << "\n";
        impl_->st = nullptr;
    }
}

stmt::~stmt() {
    if (impl_ != nullptr) {
        if (impl_->st != nullptr) {
            std::lock_guard<std::mutex> lock(mtx);
            sqlite3_finalize(impl_->st);
        }
        delete impl_;
    }
}

stmt::stmt(stmt&& other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
}

stmt& stmt::operator=(stmt&& other) noexcept {
    if (this != &other) {
        if (impl_ != nullptr && impl_->st != nullptr) {
            std::lock_guard<std::mutex> lock(mtx);
            sqlite3_finalize(impl_->st);
        }
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

bool stmt::ok() const {
    return impl_ != nullptr && impl_->st != nullptr;
}

bool stmt::step() {
    if (!ok()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mtx);
    int rc = sqlite3_step(impl_->st);
    return rc == SQLITE_ROW;
}

void stmt::reset() {
    if (!ok()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mtx);
    sqlite3_reset(impl_->st);
}

void stmt::bind(int index, int64_t value) {
    if (!ok()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mtx);
    sqlite3_bind_int64(impl_->st, index, value);
}

void stmt::bind(int index, double value) {
    if (!ok()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mtx);
    sqlite3_bind_double(impl_->st, index, value);
}

void stmt::bind(int index, const std::string& value) {
    if (!ok()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mtx);
    sqlite3_bind_text(impl_->st, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

void stmt::bind_null(int index) {
    if (!ok()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mtx);
    sqlite3_bind_null(impl_->st, index);
}

int64_t stmt::col_int(int index) const {
    if (!ok()) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(mtx);
    return sqlite3_column_int64(impl_->st, index);
}

double stmt::col_double(int index) const {
    if (!ok()) {
        return 0.0;
    }
    std::lock_guard<std::mutex> lock(mtx);
    return sqlite3_column_double(impl_->st, index);
}

std::string stmt::col_text(int index) const {
    if (!ok()) {
        return {};
    }
    std::lock_guard<std::mutex> lock(mtx);
    const unsigned char* text = sqlite3_column_text(impl_->st, index);
    if (text == nullptr) {
        return {};
    }
    return reinterpret_cast<const char*>(text);
}

bool stmt::col_is_null(int index) const {
    if (!ok()) {
        return true;
    }
    std::lock_guard<std::mutex> lock(mtx);
    return sqlite3_column_type(impl_->st, index) == SQLITE_NULL;
}

void exec(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mtx);
    if (handle_ == nullptr) {
        return;
    }
    char* err = nullptr;
    if (sqlite3_exec(handle_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[db] exec failed: " << (err ? err : "unknown") << "\n  SQL: " << sql << "\n";
        sqlite3_free(err);
    }
}

int64_t last_insert_rowid() {
    std::lock_guard<std::mutex> lock(mtx);
    return handle_ != nullptr ? sqlite3_last_insert_rowid(handle_) : 0;
}

int changes() {
    std::lock_guard<std::mutex> lock(mtx);
    return handle_ != nullptr ? sqlite3_changes(handle_) : 0;
}

std::string error() {
    std::lock_guard<std::mutex> lock(mtx);
    return handle_ != nullptr ? sqlite3_errmsg(handle_) : "database not initialised";
}

namespace {

/**
 * @brief Import legacy JSON files (warnings.json, settings.json,
 * levels.json) into the SQLite schema. Runs once, only when the target
 * tables are empty, then the JSON files are left untouched.
 */
void migrate_json() {
    const std::string warn_file = DATA_DIR + "/warnings.json";
    const std::string set_file = DATA_DIR + "/settings.json";
    const std::string lvl_file = DATA_DIR + "/levels.json";

    auto file_exists = [](const std::string& path) {
        std::ifstream f(path);
        return f.good();
    };

    // Warnings: {"guild_id:user_id": [ {id, moderator, reason, date}, ... ]}
    if (file_exists(warn_file)) {
        try {
            std::ifstream f(warn_file);
            nlohmann::json j;
            f >> j;
            stmt count("SELECT COUNT(*) FROM warnings");
            bool has_rows = count.step() && count.col_int(0) > 0;
            if (!has_rows && j.is_object()) {
                stmt ins("INSERT INTO warnings (guild_id, user_id, id, moderator, reason, date) VALUES (?,?,?,?,?,?)");
                for (auto it = j.begin(); it != j.end(); ++it) {
                    const std::string& k = it.key();
                    size_t colon = k.find(':');
                    if (colon == std::string::npos || !it->is_array()) {
                        continue;
                    }
                    int64_t guild_id = std::stoll(k.substr(0, colon));
                    int64_t user_id = std::stoll(k.substr(colon + 1));
                    for (const auto& w : *it) {
                        ins.reset();
                        ins.bind(1, guild_id);
                        ins.bind(2, user_id);
                        ins.bind(3, w.value("id", int64_t(0)));
                        ins.bind(4, w.value("moderator", int64_t(0)));
                        ins.bind(5, w.value("reason", std::string()));
                        ins.bind(6, w.value("date", int64_t(0)));
                        ins.step();
                    }
                }
                std::cout << "[db] Imported legacy warnings.json\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "[db] warnings.json migration failed: " << e.what() << "\n";
        }
    }

    // Settings: {"guild_id": { ... }}
    if (file_exists(set_file)) {
        try {
            std::ifstream f(set_file);
            nlohmann::json j;
            f >> j;
            stmt count("SELECT COUNT(*) FROM guild_settings");
            bool has_rows = count.step() && count.col_int(0) > 0;
            if (!has_rows && j.is_object()) {
                stmt ins("INSERT OR REPLACE INTO guild_settings (guild_id, data) VALUES (?,?)");
                for (auto it = j.begin(); it != j.end(); ++it) {
                    ins.reset();
                    ins.bind(1, static_cast<int64_t>(std::stoll(it.key())));
                    ins.bind(2, it.value().dump());
                    ins.step();
                }
                std::cout << "[db] Imported legacy settings.json\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "[db] settings.json migration failed: " << e.what() << "\n";
        }
    }

    // Levels: {"guild_id:user_id": xp}
    if (file_exists(lvl_file)) {
        try {
            std::ifstream f(lvl_file);
            nlohmann::json j;
            f >> j;
            stmt count("SELECT COUNT(*) FROM levels");
            bool has_rows = count.step() && count.col_int(0) > 0;
            if (!has_rows && j.is_object()) {
                stmt ins("INSERT OR REPLACE INTO levels (guild_id, user_id, xp) VALUES (?,?,?)");
                for (auto it = j.begin(); it != j.end(); ++it) {
                    const std::string& k = it.key();
                    size_t colon = k.find(':');
                    if (colon == std::string::npos || !it->is_number()) {
                        continue;
                    }
                    ins.reset();
                    ins.bind(1, static_cast<int64_t>(std::stoll(k.substr(0, colon))));
                    ins.bind(2, static_cast<int64_t>(std::stoll(k.substr(colon + 1))));
                    ins.bind(3, it->get<int64_t>());
                    ins.step();
                }
                std::cout << "[db] Imported legacy levels.json\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "[db] levels.json migration failed: " << e.what() << "\n";
        }
    }
}

} // namespace

void init() {
    // NOTE: no mutex here on purpose — init runs single-threaded at startup
    // and the helpers below (exec, stmt) lock internally, so holding the
    // lock would deadlock.
    ensure_dir();

    if (sqlite3_open(DB_PATH.c_str(), &handle_) != SQLITE_OK) {
        std::cerr << "[db] Cannot open " << DB_PATH << ": "
                  << (handle_ ? sqlite3_errmsg(handle_) : "unknown error") << "\n";
        handle_ = nullptr;
        return;
    }

    exec("PRAGMA journal_mode = WAL;");
    exec("PRAGMA synchronous = NORMAL;");

    exec("CREATE TABLE IF NOT EXISTS guild_settings ("
         " guild_id INTEGER PRIMARY KEY,"
         " data TEXT NOT NULL);");

    exec("CREATE TABLE IF NOT EXISTS warnings ("
         " guild_id INTEGER NOT NULL,"
         " user_id INTEGER NOT NULL,"
         " id INTEGER NOT NULL,"
         " moderator INTEGER NOT NULL,"
         " reason TEXT NOT NULL,"
         " date INTEGER NOT NULL,"
         " PRIMARY KEY (guild_id, user_id, id));");

    exec("CREATE TABLE IF NOT EXISTS levels ("
         " guild_id INTEGER NOT NULL,"
         " user_id INTEGER NOT NULL,"
         " xp INTEGER NOT NULL DEFAULT 0,"
         " PRIMARY KEY (guild_id, user_id));");

    exec("CREATE TABLE IF NOT EXISTS economy ("
         " guild_id INTEGER NOT NULL,"
         " user_id INTEGER NOT NULL,"
         " wallet INTEGER NOT NULL DEFAULT 0,"
         " last_daily INTEGER NOT NULL DEFAULT 0,"
         " last_work INTEGER NOT NULL DEFAULT 0,"
         " PRIMARY KEY (guild_id, user_id));");

    exec("CREATE TABLE IF NOT EXISTS custom_commands ("
         " guild_id INTEGER NOT NULL,"
         " name TEXT NOT NULL,"
         " response TEXT NOT NULL,"
         " PRIMARY KEY (guild_id, name));");

    exec("CREATE TABLE IF NOT EXISTS role_menus ("
         " id INTEGER PRIMARY KEY AUTOINCREMENT,"
         " guild_id INTEGER NOT NULL,"
         " name TEXT NOT NULL,"
         " title TEXT NOT NULL,"
         " UNIQUE (guild_id, name));");

    exec("CREATE TABLE IF NOT EXISTS role_menu_options ("
         " menu_id INTEGER NOT NULL,"
         " role_id INTEGER NOT NULL,"
         " label TEXT NOT NULL,"
         " PRIMARY KEY (menu_id, role_id));");

    exec("CREATE TABLE IF NOT EXISTS tickets ("
         " id INTEGER PRIMARY KEY AUTOINCREMENT,"
         " guild_id INTEGER NOT NULL,"
         " channel_id INTEGER NOT NULL,"
         " user_id INTEGER NOT NULL,"
         " created INTEGER NOT NULL,"
         " status INTEGER NOT NULL DEFAULT 0);"); // 0 = open, 1 = closed

    exec("CREATE TABLE IF NOT EXISTS giveaways ("
         " id INTEGER PRIMARY KEY AUTOINCREMENT,"
         " guild_id INTEGER NOT NULL,"
         " channel_id INTEGER NOT NULL,"
         " message_id INTEGER NOT NULL,"
         " prize TEXT NOT NULL,"
         " winners INTEGER NOT NULL DEFAULT 1,"
         " end_time INTEGER NOT NULL,"
         " host_id INTEGER NOT NULL,"
         " ended INTEGER NOT NULL DEFAULT 0);");

    exec("CREATE TABLE IF NOT EXISTS giveaway_entries ("
         " giveaway_id INTEGER NOT NULL,"
         " user_id INTEGER NOT NULL,"
         " PRIMARY KEY (giveaway_id, user_id));");

    exec("CREATE TABLE IF NOT EXISTS polls ("
         " id INTEGER PRIMARY KEY AUTOINCREMENT,"
         " guild_id INTEGER NOT NULL,"
         " channel_id INTEGER NOT NULL,"
         " message_id INTEGER NOT NULL,"
         " author_id INTEGER NOT NULL,"
         " question TEXT NOT NULL,"
         " options TEXT NOT NULL,"
         " voters TEXT NOT NULL,"
         " ended INTEGER NOT NULL DEFAULT 0);");

    std::cout << "[db] Database ready at " << DB_PATH << "\n";

    migrate_json();
}

} // namespace db

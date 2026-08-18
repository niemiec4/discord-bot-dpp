#pragma once

#include <cstdint>
#include <string>

/**
 * @brief Thin SQLite wrapper used by every persistent module (settings,
 * warnings, levels, economy, custom commands, role menus, tickets,
 * giveaways, polls). All access is serialized with an internal mutex so it
 * is safe to call from any thread.
 *
 * The sqlite3.h header is bundled in this project (src/sqlite3.h) and the
 * binary links against the system libsqlite3.
 */
namespace db {

/**
 * @brief RAII prepared statement.
 *
 * Usage:
 *   db::stmt s("SELECT x FROM t WHERE id = ?");
 *   s.bind(1, 42);
 *   while (s.step()) { int64_t x = s.col_int(0); }
 */
class stmt {
public:
    stmt() = default;
    explicit stmt(const std::string& sql);
    ~stmt();

    stmt(stmt&& other) noexcept;
    stmt& operator=(stmt&& other) noexcept;
    stmt(const stmt&) = delete;
    stmt& operator=(const stmt&) = delete;

    /** @brief true once the statement was prepared without an error. */
    bool ok() const;

    /** @brief Advance to the next row. @return true while a row is available. */
    bool step();

    /** @brief Reset so the statement can be executed again. */
    void reset();

    void bind(int index, int64_t value);
    void bind(int index, double value);
    void bind(int index, const std::string& value);
    void bind_null(int index);

    int64_t col_int(int index) const;
    double col_double(int index) const;
    std::string col_text(int index) const;
    bool col_is_null(int index) const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

/**
 * @brief Open (or create) the database, build the schema and import any
 * legacy JSON data files. Safe to call once at startup.
 */
void init();

/** @brief Run a statement that returns no rows (CREATE TABLE, ...). */
void exec(const std::string& sql);

/** @brief Row id of the last successful INSERT. */
int64_t last_insert_rowid();

/** @brief Number of rows changed by the last statement. */
int changes();

/** @brief Human readable description of the last error. */
std::string error();

} // namespace db

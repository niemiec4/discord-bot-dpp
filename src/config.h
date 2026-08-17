#pragma once

#include <cstdint>
#include <string>

namespace cfg {

/**
 * @brief Loads KEY=VALUE pairs from a .env file into the process environment.
 *
 * Lines starting with '#' and empty lines are ignored. Existing environment
 * variables are NOT overwritten, so real environment variables take priority
 * over values in the file.
 *
 * @param filename Path to the environment file.
 */
void load_env(const std::string& filename = ".env");

/**
 * @brief Get a string value from the environment.
 * @param key Variable name.
 * @param fallback Value to return if the variable is not set.
 */
std::string get(const std::string& key, const std::string& fallback = "");

/**
 * @brief Get an integer value from the environment.
 * @param key Variable name.
 * @param fallback Value to return if the variable is missing or invalid.
 */
int64_t get_int(const std::string& key, int64_t fallback = 0);

/**
 * @brief Get a snowflake (Discord id) value from the environment.
 * @param key Variable name.
 * @param fallback Value to return if the variable is missing or invalid.
 */
uint64_t get_id(const std::string& key, uint64_t fallback = 0);

} // namespace cfg

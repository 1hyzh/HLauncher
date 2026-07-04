#include "app/account_store.hpp"

#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace {

std::string escape_field(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        if (ch == '\\' || ch == '\t' || ch == '\n' || ch == '\r') {
            escaped.push_back('\\');
        }
        if (ch == '\n') {
            escaped.push_back('n');
        } else if (ch == '\r') {
            escaped.push_back('r');
        } else {
            escaped.push_back(ch);
        }
    }
    return escaped;
}

std::string unescape_field(const std::string& value) {
    std::string unescaped;
    unescaped.reserve(value.size());
    bool escaping = false;
    for (char ch : value) {
        if (escaping) {
            if (ch == 'n') {
                unescaped.push_back('\n');
            } else if (ch == 'r') {
                unescaped.push_back('\r');
            } else {
                unescaped.push_back(ch);
            }
            escaping = false;
        } else if (ch == '\\') {
            escaping = true;
        } else {
            unescaped.push_back(ch);
        }
    }
    if (escaping) {
        unescaped.push_back('\\');
    }
    return unescaped;
}

std::vector<std::string> split_fields(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool escaping = false;
    for (char ch : line) {
        if (escaping) {
            current.push_back('\\');
            current.push_back(ch);
            escaping = false;
            continue;
        }
        if (ch == '\\') {
            escaping = true;
            continue;
        }
        if (ch == '\t') {
            fields.push_back(unescape_field(current));
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    fields.push_back(unescape_field(current));
    return fields;
}

} // namespace

AccountStore::AccountStore(std::filesystem::path storage_path)
    : storage_path_(std::move(storage_path)) {}

std::vector<AccountRecord> AccountStore::load() const {
    std::vector<AccountRecord> accounts;

    std::ifstream file(storage_path_);
    if (!file.is_open()) {
        return accounts;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        const auto fields = split_fields(line);
        if (fields.size() != 4) {
            continue;
        }
        accounts.push_back(AccountRecord{fields[0], fields[1], fields[2], fields[3]});
    }

    return accounts;
}

void AccountStore::save(const std::vector<AccountRecord>& accounts) const {
    if (!storage_path_.parent_path().empty()) {
        std::filesystem::create_directories(storage_path_.parent_path());
    }

    std::ofstream file(storage_path_, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open account store for writing: " + storage_path_.string());
    }

    for (const auto& account : accounts) {
        file << escape_field(account.provider) << '\t'
             << escape_field(account.display_name) << '\t'
             << escape_field(account.status) << '\t'
             << escape_field(account.added_at) << '\n';
    }
}

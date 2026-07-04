#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct AccountRecord {
    std::string provider;
    std::string display_name;
    std::string status;
    std::string added_at;
};

class AccountStore {
public:
    explicit AccountStore(std::filesystem::path storage_path);

    std::vector<AccountRecord> load() const;
    void save(const std::vector<AccountRecord>& accounts) const;

private:
    std::filesystem::path storage_path_;
};

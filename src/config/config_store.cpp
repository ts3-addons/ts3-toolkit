#include "ts3toolkit/config/config_store.h"

#include <cctype>
#include <fstream>
#include <sstream>

#include <filesystem>

namespace ts3toolkit::config {

namespace fs = std::filesystem;

namespace {

std::string trim(const std::string& input) {
    std::size_t begin = 0;
    while (begin < input.size() &&
           std::isspace(static_cast<unsigned char>(input[begin]))) {
        ++begin;
    }
    std::size_t end = input.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(input[end - 1]))) {
        --end;
    }
    return input.substr(begin, end - begin);
}

bool parseBool(const std::string& raw, bool& output) {
    if (raw == "true") {
        output = true;
        return true;
    }
    if (raw == "false") {
        output = false;
        return true;
    }
    return false;
}

bool parseInt(const std::string& raw, int& output) {
    std::size_t pos = 0;
    bool has_digit = false;
    int value = 0;
    int sign = 1;
    if (pos < raw.size() && (raw[pos] == '+' || raw[pos] == '-')) {
        if (raw[pos] == '-') {
            sign = -1;
        }
        ++pos;
    }
    while (pos < raw.size() &&
           std::isdigit(static_cast<unsigned char>(raw[pos]))) {
        has_digit = true;
        value = value * 10 + (raw[pos] - '0');
        ++pos;
    }
    if (!has_digit || pos != raw.size()) {
        return false;
    }
    output = value * sign;
    return true;
}

bool parseFloat(const std::string& raw, float& output) {
    std::istringstream stream(raw);
    float value = 0.0f;
    stream >> value;
    if (stream.fail()) {
        return false;
    }
    stream >> std::ws;
    if (!stream.eof()) {
        return false;
    }
    output = value;
    return true;
}

bool parseQuoted(const std::string& raw, std::string& output) {
    if (raw.size() < 2 || raw.front() != '"' || raw.back() != '"') {
        return false;
    }
    std::string result;
    for (std::size_t i = 1; i + 1 < raw.size(); ++i) {
        char current = raw[i];
        if (current == '\\') {
            if (i + 2 >= raw.size()) {
                return false;
            }
            char next = raw[i + 1];
            switch (next) {
                case '"':
                    result.push_back('"');
                    break;
                case '\\':
                    result.push_back('\\');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                default:
                    return false;
            }
            ++i;
        } else {
            result.push_back(current);
        }
    }
    output = std::move(result);
    return true;
}

std::string quote(const std::string& value) {
    std::string result = "\"";
    for (char c : value) {
        if (c == '"' || c == '\\') {
            result.push_back('\\');
        }
        result.push_back(c);
    }
    result.push_back('"');
    return result;
}

const char* boolString(bool value) {
    return value ? "true" : "false";
}

std::string serialize(const SettingsDocument& document) {
    const EntranceSoundSettings& sound = document.entrance_sound;
    std::ostringstream out;
    out << "# TS3Toolkit configuration. Do not edit while the client is running.\n";
    out << "schema_version = " << document.schema_version << "\n\n";
    out << "[entrance_sound]\n";
    out << "enabled = " << boolString(sound.enabled) << "\n";
    out << "sound_file = " << quote(sound.sound_file) << "\n";
    out << "volume = " << sound.volume << "\n";
    out << "cooldown_seconds = " << sound.cooldown_seconds << "\n";
    out << "local_monitor = " << boolString(sound.local_monitor) << "\n";
    out << "delay_ms = " << sound.delay_ms << "\n";
    return out.str();
}

bool parse(const std::string& text, SettingsDocument& document) {
    document = SettingsDocument{};
    bool saw_schema = false;
    bool saw_delay = false;
    std::string section;
    std::istringstream stream(text);
    std::string line;

    while (std::getline(stream, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        if (trimmed[0] == '[') {
            if (trimmed.size() < 2 || trimmed.back() != ']') {
                return false;
            }
            section = trimmed.substr(1, trimmed.size() - 2);
            if (section != "entrance_sound") {
                return false;
            }
            continue;
        }

        const std::size_t equals = trimmed.find('=');
        if (equals == std::string::npos) {
            return false;
        }
        const std::string key = trim(trimmed.substr(0, equals));
        const std::string value = trim(trimmed.substr(equals + 1));
        if (key.empty()) {
            return false;
        }

        if (section.empty()) {
            if (key == "schema_version") {
                if (!parseInt(value, document.schema_version)) {
                    return false;
                }
                saw_schema = true;
            } else {
                return false;
            }
            continue;
        }

        EntranceSoundSettings& sound = document.entrance_sound;
        if (key == "enabled") {
            if (!parseBool(value, sound.enabled)) {
                return false;
            }
        } else if (key == "sound_file") {
            if (!parseQuoted(value, sound.sound_file)) {
                return false;
            }
        } else if (key == "volume") {
            if (!parseFloat(value, sound.volume)) {
                return false;
            }
        } else if (key == "cooldown_seconds") {
            if (!parseInt(value, sound.cooldown_seconds)) {
                return false;
            }
        } else if (key == "local_monitor") {
            if (!parseBool(value, sound.local_monitor)) {
                return false;
            }
        } else if (key == "delay_ms") {
            saw_delay = true;
            if (!parseInt(value, sound.delay_ms)) {
                return false;
            }
        } else {
            return false;
        }
    }

    if (!saw_schema) {
        return false;
    }
    if ((document.schema_version != 1 &&
         document.schema_version != kCurrentSchemaVersion) ||
        (document.schema_version == 1 && saw_delay)) {
        return false;
    }
    document.schema_version = kCurrentSchemaVersion;
    return validate(document.entrance_sound).valid;
}

}  // namespace

ConfigStore::ConfigStore(std::string file_path)
    : file_path_(std::move(file_path)) {}

LoadOutcome ConfigStore::load() const {
    LoadOutcome outcome;
    std::error_code error;
    if (!fs::exists(file_path_, error)) {
        outcome.status = LoadStatus::kCreatedDefaults;
        return outcome;
    }

    std::ifstream input(file_path_, std::ios::binary);
    if (!input) {
        outcome.status = LoadStatus::kRejected;
        return outcome;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();

    SettingsDocument parsed;
    if (parse(buffer.str(), parsed)) {
        outcome.document = parsed;
        outcome.status = LoadStatus::kLoaded;
    } else {
        outcome.status = LoadStatus::kRejected;
    }
    return outcome;
}

bool ConfigStore::save(const SettingsDocument& document) const {
    if (document.schema_version != kCurrentSchemaVersion ||
        !validate(document.entrance_sound).valid) {
        return false;
    }

    const std::string temporary_path = file_path_ + ".tmp";
    {
        std::ofstream output(temporary_path,
                             std::ios::binary | std::ios::trunc);
        if (!output) {
            return false;
        }
        output << serialize(document);
        output.flush();
        if (!output) {
            return false;
        }
    }

    std::error_code error;
    fs::rename(temporary_path, file_path_, error);
    if (error) {
        std::error_code remove_error;
        fs::remove(temporary_path, remove_error);
        return false;
    }
    return true;
}

}  // namespace ts3toolkit::config

#pragma once

#include <string>
#include <string_view>

#include "ts3toolkit/config/settings.h"

namespace ts3toolkit::plugin {

inline std::wstring formatRangeTime(int milliseconds) {
    const int minutes = milliseconds / 60000;
    const int seconds = (milliseconds / 1000) % 60;
    const int fraction = milliseconds % 1000;
    std::wstring result;
    if (minutes < 10) result += L'0';
    result += std::to_wstring(minutes) + L':';
    if (seconds < 10) result += L'0';
    result += std::to_wstring(seconds) + L'.';
    if (fraction < 100) result += L'0';
    if (fraction < 10) result += L'0';
    result += std::to_wstring(fraction);
    return result;
}

inline bool parseRangeTime(std::wstring_view value, int& milliseconds) {
    const auto colon = value.find(L':');
    if (colon == std::wstring_view::npos || colon < 1 || colon > 2) return false;
    const auto dot = value.find(L'.', colon + 1);
    const auto seconds_end = dot == std::wstring_view::npos ? value.size() : dot;
    if (seconds_end != colon + 3) return false;
    const auto fractional_digits = dot == std::wstring_view::npos ? 0 : value.size() - dot - 1;
    if (dot != std::wstring_view::npos && (fractional_digits < 1 || fractional_digits > 3)) return false;

    int minutes = 0;
    for (std::size_t i = 0; i < colon; ++i) {
        if (value[i] < L'0' || value[i] > L'9') return false;
        minutes = minutes * 10 + (value[i] - L'0');
    }
    if (value[colon + 1] < L'0' || value[colon + 1] > L'9' ||
        value[colon + 2] < L'0' || value[colon + 2] > L'9') return false;
    const int seconds = (value[colon + 1] - L'0') * 10 + value[colon + 2] - L'0';
    if (seconds >= 60) return false;
    int fraction = 0;
    for (std::size_t i = 0; i < fractional_digits; ++i) {
        const wchar_t digit = value[dot + 1 + i];
        if (digit < L'0' || digit > L'9') return false;
        fraction = fraction * 10 + (digit - L'0');
    }
    for (std::size_t i = fractional_digits; i < 3; ++i) fraction *= 10;
    const int parsed = minutes * 60000 + seconds * 1000 + fraction;
    if (parsed > config::kMaxSourceDurationMs) return false;
    milliseconds = parsed;
    return true;
}

}  // namespace ts3toolkit::plugin

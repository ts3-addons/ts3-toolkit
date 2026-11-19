#include "settings_dialog.h"
#include "settings_resource.h"
#include "range_time.h"

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>

#include <cmath>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

#include "ts3toolkit/audio/sound_decoder.h"
#include "ts3toolkit/audio/audio_engine.h"
#include "ts3toolkit/config/config_store.h"

namespace ts3toolkit::plugin {
namespace {

class LocalPreview {
public:
    ~LocalPreview() { stop(); }

    bool play(const audio::DecodedSound& sound, float volume) {
        stop();
        frames_played_.store(0);
        total_frames_ = sound.samples.size();
        engine_.playSound(sound, volume);
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.sampleRate = core::kSampleRateHz;
        config.playback.format = ma_format_s16;
        config.playback.channels = core::kChannelCount;
        config.dataCallback = &LocalPreview::callback;
        config.pUserData = this;
        if (ma_device_init(nullptr, &config, &device_) != MA_SUCCESS) {
            engine_.stopSound();
            return false;
        }
        if (ma_device_start(&device_) != MA_SUCCESS) {
            ma_device_uninit(&device_);
            engine_.stopSound();
            return false;
        }
        running_ = true;
        return true;
    }

    void stop() {
        if (running_) {
            ma_device_uninit(&device_);
            running_ = false;
        }
        engine_.stopSound();
    }

    bool playing() const { return running_ && engine_.state() == audio::PlaybackState::kPlaying; }
    std::size_t framesPlayed() const {
        return (std::min)(frames_played_.load(), total_frames_);
    }

private:
    static void callback(ma_device* device, void* output, const void*,
                         ma_uint32 frames) {
        auto* self = static_cast<LocalPreview*>(device->pUserData);
        self->engine_.fillFrame(static_cast<core::Sample*>(output), frames);
        self->frames_played_.fetch_add(frames);
    }

    audio::AudioEngine engine_{core::kFrameSamples * 10};
    ma_device device_{};
    bool running_ = false;
    std::atomic<std::size_t> frames_played_{0};
    std::size_t total_frames_ = 0;
};

struct DialogState {
    config::ConfigStore store;
    config::SettingsDocument document;
    LocalPreview preview;
    std::wstring range_path;
};

std::wstring wide(const std::string& value) {
    if (value.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                   static_cast<int>(value.size()), nullptr, 0);
    if (!size) return {};
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                   static_cast<int>(value.size()), nullptr, 0,
                                   nullptr, nullptr);
    if (!size) return {};
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), size,
                        nullptr, nullptr);
    return result;
}

std::wstring text(HWND dialog, int id) {
    const int length = GetWindowTextLengthW(GetDlgItem(dialog, id));
    std::vector<wchar_t> buffer(static_cast<std::size_t>(length) + 1);
    GetDlgItemTextW(dialog, id, buffer.data(), length + 1);
    return buffer.data();
}

void updateControls(HWND dialog, DialogState& state) {
    const bool enabled = IsDlgButtonChecked(dialog, IDC_ENABLED) == BST_CHECKED;
    const int controls[] = {IDC_SOUND_PATH, IDC_BROWSE, IDC_PREVIEW, IDC_EDIT_RANGE, IDC_VOLUME_SLIDER,
                            IDC_VOLUME, IDC_DELAY, IDC_COOLDOWN, IDC_MONITOR};
    for (int id : controls) EnableWindow(GetDlgItem(dialog, id), enabled);
    EnableWindow(GetDlgItem(dialog, IDC_STOP_PREVIEW), enabled && state.preview.playing());
}

void updateRangeSummary(HWND dialog, const config::EntranceSoundSettings& sound) {
    std::wstring summary = L"播放范围：完整音频";
    if (sound.start_ms != 0 || sound.end_ms != -1) {
        summary = L"播放范围：" + formatRangeTime(sound.start_ms) + L"–" +
                  (sound.end_ms == -1 ? L"结束" : formatRangeTime(sound.end_ms));
    }
    SetDlgItemTextW(dialog, IDC_RANGE_SUMMARY, summary.c_str());
}

struct RangeState {
    audio::DecodedSound source;
    LocalPreview preview;
    int start = 0;
    int end = 0;
    int position = 0;
    int play_start = 0;
    int duration = 0;
    float volume = 1.0f;
    bool updating = false;
};

void rangeControls(HWND dialog, RangeState& state) {
    state.updating = true;
    SetDlgItemTextW(dialog, IDC_RANGE_START, formatRangeTime(state.start).c_str());
    SetDlgItemTextW(dialog, IDC_RANGE_END, formatRangeTime(state.end).c_str());
    SendDlgItemMessageW(dialog, IDC_RANGE_START_SLIDER, TBM_SETPOS, TRUE, state.start);
    SendDlgItemMessageW(dialog, IDC_RANGE_END_SLIDER, TBM_SETPOS, TRUE, state.end);
    SendDlgItemMessageW(dialog, IDC_RANGE_POSITION, TBM_SETSELSTART, FALSE, state.start);
    SendDlgItemMessageW(dialog, IDC_RANGE_POSITION, TBM_SETSELEND, TRUE, state.end);
    SendDlgItemMessageW(dialog, IDC_RANGE_POSITION, TBM_SETPOS, TRUE, state.position);
    const std::wstring position = L"当前位置：" + formatRangeTime(state.position) +
                                  L" / " + formatRangeTime(state.duration);
    SetDlgItemTextW(dialog, IDC_RANGE_POSITION_TEXT, position.c_str());
    EnableWindow(GetDlgItem(dialog, IDC_RANGE_STOP), state.preview.playing());
    state.updating = false;
}

bool validRange(const RangeState& state) {
    return audio::selectRange(state.source, state.start, state.end).status == audio::DecodeStatus::kOk;
}

void stopRangePreview(HWND dialog, RangeState& state) {
    KillTimer(dialog, 2);
    state.preview.stop();
}

INT_PTR CALLBACK rangeProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<RangeState*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (message == WM_INITDIALOG) {
        state = reinterpret_cast<RangeState*>(lparam);
        SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
        for (int id : {IDC_RANGE_START_SLIDER, IDC_RANGE_END_SLIDER, IDC_RANGE_POSITION}) {
            SendDlgItemMessageW(dialog, id, TBM_SETRANGEMIN, FALSE, 0);
            SendDlgItemMessageW(dialog, id, TBM_SETRANGEMAX, TRUE, state->duration);
            SendDlgItemMessageW(dialog, id, TBM_SETPAGESIZE, 0, 1000);
        }
        rangeControls(dialog, *state);
        return TRUE;
    }
    if (!state) return FALSE;
    if (message == WM_TIMER && wparam == 2) {
        state->position = (std::min)(state->end, state->play_start +
            static_cast<int>(state->preview.framesPlayed() / (core::kSampleRateHz / 1000)));
        if (!state->preview.playing()) {
            state->position = state->end;
            stopRangePreview(dialog, *state);
        }
        rangeControls(dialog, *state);
        return TRUE;
    }
    if (message == WM_HSCROLL) {
        const HWND control = reinterpret_cast<HWND>(lparam);
        int id = 0;
        if (control == GetDlgItem(dialog, IDC_RANGE_START_SLIDER)) id = IDC_RANGE_START_SLIDER;
        if (control == GetDlgItem(dialog, IDC_RANGE_END_SLIDER)) id = IDC_RANGE_END_SLIDER;
        if (control == GetDlgItem(dialog, IDC_RANGE_POSITION)) id = IDC_RANGE_POSITION;
        if (!id) return FALSE;
        const int value = static_cast<int>(SendDlgItemMessageW(dialog, id, TBM_GETPOS, 0, 0));
        stopRangePreview(dialog, *state);
        if (id == IDC_RANGE_START_SLIDER) state->start = value;
        if (id == IDC_RANGE_END_SLIDER) state->end = value;
        if (id == IDC_RANGE_POSITION) state->position = value;
        else state->position = state->start;
        rangeControls(dialog, *state);
        return TRUE;
    }
    if (message != WM_COMMAND) return FALSE;
    switch (LOWORD(wparam)) {
        case IDC_RANGE_START:
        case IDC_RANGE_END:
            if (HIWORD(wparam) == EN_CHANGE && !state->updating) {
                int value = 0;
                const bool valid = parseRangeTime(text(dialog, LOWORD(wparam)), value);
                stopRangePreview(dialog, *state);
                if (valid && value <= state->duration) {
                    if (LOWORD(wparam) == IDC_RANGE_START) state->start = value;
                    else state->end = value;
                    state->position = state->start;
                    const int slider = LOWORD(wparam) == IDC_RANGE_START ? IDC_RANGE_START_SLIDER : IDC_RANGE_END_SLIDER;
                    SendDlgItemMessageW(dialog, slider, TBM_SETPOS, TRUE, value);
                    SendDlgItemMessageW(dialog, IDC_RANGE_POSITION, TBM_SETPOS, TRUE, state->position);
                    SendDlgItemMessageW(dialog, IDC_RANGE_POSITION, TBM_SETSELSTART, FALSE, state->start);
                    SendDlgItemMessageW(dialog, IDC_RANGE_POSITION, TBM_SETSELEND, TRUE, state->end);
                    const std::wstring position = L"当前位置：" + formatRangeTime(state->position) +
                                                  L" / " + formatRangeTime(state->duration);
                    SetDlgItemTextW(dialog, IDC_RANGE_POSITION_TEXT, position.c_str());
                }
                EnableWindow(GetDlgItem(dialog, IDC_RANGE_STOP), FALSE);
            }
            return TRUE;
        case IDC_RANGE_RESET:
            stopRangePreview(dialog, *state);
            state->start = 0;
            state->end = (std::min)(state->duration, config::kMaxSelectionDurationMs);
            state->position = 0;
            rangeControls(dialog, *state);
            return TRUE;
        case IDC_RANGE_PLAY: {
            int start = 0, end = 0;
            const bool start_ok = parseRangeTime(text(dialog, IDC_RANGE_START), start);
            const bool end_ok = parseRangeTime(text(dialog, IDC_RANGE_END), end);
            if (!start_ok || !end_ok || start != state->start ||
                end != state->end || !validRange(*state)) {
                MessageBoxW(dialog, L"请输入有效时间（例如 01:05.250），并确保片段最长 30 秒。", L"播放范围无效", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            if (state->position < state->start || state->position >= state->end)
                state->position = state->start;
            auto segment = audio::selectRange(state->source, state->position, state->end);
            state->play_start = state->position;
            if (!state->preview.play(segment.sound, state->volume)) {
                MessageBoxW(dialog, L"无法打开系统默认播放设备。", L"试听失败", MB_OK | MB_ICONERROR);
            } else {
                SetTimer(dialog, 2, 100, nullptr);
            }
            rangeControls(dialog, *state);
            return TRUE;
        }
        case IDC_RANGE_STOP:
            stopRangePreview(dialog, *state);
            rangeControls(dialog, *state);
            return TRUE;
        case IDOK: {
            int start = 0, end = 0;
            const bool start_ok = parseRangeTime(text(dialog, IDC_RANGE_START), start);
            const bool end_ok = parseRangeTime(text(dialog, IDC_RANGE_END), end);
            if (!start_ok || !end_ok || start != state->start ||
                end != state->end || !validRange(*state)) {
                MessageBoxW(dialog, L"请输入有效时间（例如 01:05.250），并确保片段最长 30 秒。", L"播放范围无效", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            stopRangePreview(dialog, *state);
            EndDialog(dialog, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            stopRangePreview(dialog, *state);
            EndDialog(dialog, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

INT_PTR CALLBACK dialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (message == WM_INITDIALOG) {
        state = reinterpret_cast<DialogState*>(lparam);
        SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
        const auto& sound = state->document.entrance_sound;
        state->range_path = wide(sound.sound_file);
        CheckDlgButton(dialog, IDC_ENABLED, sound.enabled ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(dialog, IDC_MONITOR, sound.local_monitor ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemTextW(dialog, IDC_SOUND_PATH, wide(sound.sound_file).c_str());
        SetDlgItemInt(dialog, IDC_VOLUME, static_cast<UINT>(std::lround(sound.volume * 100)), FALSE);
        SetDlgItemInt(dialog, IDC_DELAY, sound.delay_ms, FALSE);
        SetDlgItemInt(dialog, IDC_COOLDOWN, sound.cooldown_seconds, FALSE);
        updateRangeSummary(dialog, sound);
        SendDlgItemMessageW(dialog, IDC_VOLUME_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendDlgItemMessageW(dialog, IDC_VOLUME_SLIDER, TBM_SETPOS, TRUE,
                            static_cast<LPARAM>(std::lround(sound.volume * 100)));
        updateControls(dialog, *state);
        return TRUE;
    }
    if (state && message == WM_HSCROLL && reinterpret_cast<HWND>(lparam) ==
                                              GetDlgItem(dialog, IDC_VOLUME_SLIDER)) {
        const auto volume = SendDlgItemMessageW(dialog, IDC_VOLUME_SLIDER, TBM_GETPOS, 0, 0);
        SetDlgItemInt(dialog, IDC_VOLUME, static_cast<UINT>(volume), FALSE);
        return TRUE;
    }
    if (state && message == WM_TIMER && wparam == 1) {
        if (!state->preview.playing()) {
            KillTimer(dialog, 1);
            state->preview.stop();
            updateControls(dialog, *state);
        }
        return TRUE;
    }
    if (message != WM_COMMAND || !state) return FALSE;
    switch (LOWORD(wparam)) {
        case IDC_SOUND_PATH:
            if (HIWORD(wparam) == EN_CHANGE && text(dialog, IDC_SOUND_PATH) != state->range_path) {
                state->range_path = text(dialog, IDC_SOUND_PATH);
                state->document.entrance_sound.start_ms = 0;
                state->document.entrance_sound.end_ms = -1;
                updateRangeSummary(dialog, state->document.entrance_sound);
                KillTimer(dialog, 1);
                state->preview.stop();
                updateControls(dialog, *state);
            }
            return TRUE;
        case IDC_ENABLED:
            if (IsDlgButtonChecked(dialog, IDC_ENABLED) != BST_CHECKED) {
                KillTimer(dialog, 1);
                state->preview.stop();
            }
            updateControls(dialog, *state);
            return TRUE;
        case IDC_VOLUME:
            if (HIWORD(wparam) == EN_CHANGE) {
                BOOL valid = FALSE;
                const UINT volume = GetDlgItemInt(dialog, IDC_VOLUME, &valid, FALSE);
                if (valid && volume <= 100) {
                    SendDlgItemMessageW(dialog, IDC_VOLUME_SLIDER, TBM_SETPOS, TRUE, volume);
                }
            }
            return TRUE;
        case IDC_BROWSE: {
            wchar_t path[32768] = {};
            OPENFILENAMEW picker{};
            picker.lStructSize = sizeof(picker);
            picker.hwndOwner = dialog;
            picker.lpstrFilter = L"音频文件 (*.wav;*.mp3;*.flac)\0*.wav;*.mp3;*.flac\0所有文件\0*.*\0";
            picker.lpstrFile = path;
            picker.nMaxFile = static_cast<DWORD>(std::size(path));
            picker.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameW(&picker)) SetDlgItemTextW(dialog, IDC_SOUND_PATH, path);
            return TRUE;
        }
        case IDC_EDIT_RANGE: {
            BOOL volume_ok = FALSE;
            const UINT volume = GetDlgItemInt(dialog, IDC_VOLUME, &volume_ok, FALSE);
            if (!volume_ok || volume > 100) {
                MessageBoxW(dialog, L"请输入 0 到 100 之间的音量。", L"音量无效", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            const std::wstring path = text(dialog, IDC_SOUND_PATH);
            const std::string encoded = utf8(path);
            audio::SoundDecoder decoder;
            auto decoded = decoder.decodeFile(encoded, audio::kMaxSourceSoundSamples);
            if (path.empty() || encoded.empty() || !std::filesystem::path(path).is_absolute() ||
                decoded.status != audio::DecodeStatus::kOk || decoded.sound.empty()) {
                MessageBoxW(dialog, L"请选择有效的音频文件（最长 5 分钟）。", L"音效不可用", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            KillTimer(dialog, 1);
            state->preview.stop();
            updateControls(dialog, *state);
            RangeState range;
            range.source = std::move(decoded.sound);
            constexpr int samples_per_ms = core::kSampleRateHz / 1000;
            range.duration = static_cast<int>((range.source.samples.size() + samples_per_ms - 1) / samples_per_ms);
            range.start = state->document.entrance_sound.start_ms;
            range.end = state->document.entrance_sound.end_ms == -1 ? range.duration : state->document.entrance_sound.end_ms;
            if (!validRange(range)) {
                range.start = 0;
                range.end = (std::min)(range.duration, config::kMaxSelectionDurationMs);
            }
            range.position = range.start;
            range.volume = static_cast<float>(volume) / 100.0f;
            HMODULE module = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&showSettingsDialog), &module);
            if (DialogBoxParamW(module, MAKEINTRESOURCEW(IDD_RANGE), dialog, rangeProc,
                                reinterpret_cast<LPARAM>(&range)) == IDOK) {
                state->document.entrance_sound.start_ms = range.start;
                state->document.entrance_sound.end_ms = range.start == 0 && range.end == range.duration ? -1 : range.end;
                updateRangeSummary(dialog, state->document.entrance_sound);
            }
            return TRUE;
        }
        case IDC_PREVIEW: {
            BOOL volume_ok = FALSE;
            const UINT volume = GetDlgItemInt(dialog, IDC_VOLUME, &volume_ok, FALSE);
            if (!volume_ok || volume > 100) {
                MessageBoxW(dialog, L"请输入 0 到 100 之间的音量。", L"音量无效", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            const std::wstring path = text(dialog, IDC_SOUND_PATH);
            const std::string encoded = utf8(path);
            audio::SoundDecoder decoder;
            auto decoded = decoder.decodeFile(encoded, audio::kMaxSourceSoundSamples);
            if (path.empty() || encoded.empty() ||
                !std::filesystem::path(path).is_absolute() ||
                decoded.status != audio::DecodeStatus::kOk || decoded.sound.empty()) {
                MessageBoxW(dialog, L"请选择有效的音频文件（最长 5 分钟）。", L"音效不可用", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            decoded = audio::selectRange(decoded.sound, state->document.entrance_sound.start_ms,
                                         state->document.entrance_sound.end_ms);
            if (decoded.status != audio::DecodeStatus::kOk) {
                MessageBoxW(dialog, L"请编辑播放范围，选择不超过 30 秒的片段。", L"播放范围无效", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            if (!state->preview.play(decoded.sound, static_cast<float>(volume) / 100.0f)) {
                MessageBoxW(dialog, L"无法打开系统默认播放设备。", L"试听失败", MB_OK | MB_ICONERROR);
            }
            if (state->preview.playing()) SetTimer(dialog, 1, 100, nullptr);
            updateControls(dialog, *state);
            return TRUE;
        }
        case IDC_STOP_PREVIEW:
            KillTimer(dialog, 1);
            state->preview.stop();
            updateControls(dialog, *state);
            return TRUE;
        case IDOK: {
            BOOL volume_ok = FALSE, delay_ok = FALSE, cooldown_ok = FALSE;
            UINT volume = GetDlgItemInt(dialog, IDC_VOLUME, &volume_ok, FALSE);
            UINT delay = GetDlgItemInt(dialog, IDC_DELAY, &delay_ok, FALSE);
            UINT cooldown = GetDlgItemInt(dialog, IDC_COOLDOWN, &cooldown_ok, FALSE);
            if (!volume_ok || volume > 100 || !delay_ok || delay > 30000 ||
                !cooldown_ok || cooldown > 300) {
                const bool invalid_volume = !volume_ok || volume > 100;
                const bool invalid_delay = !delay_ok || delay > 30000;
                const int field = invalid_volume ? IDC_VOLUME : invalid_delay ? IDC_DELAY : IDC_COOLDOWN;
                const wchar_t* message = invalid_volume ? L"请输入 0 到 100 之间的音量。" :
                    invalid_delay ? L"请输入 0 到 30000 毫秒之间的播放延迟。" :
                                    L"请输入 0 到 300 秒之间的冷却时间。";
                MessageBoxW(dialog, message, L"设置无效", MB_OK | MB_ICONWARNING);
                SetFocus(GetDlgItem(dialog, field));
                return TRUE;
            }
            const std::wstring path = text(dialog, IDC_SOUND_PATH);
            if (!path.empty()) {
                const std::string encoded = utf8(path);
                audio::SoundDecoder decoder;
                auto decoded = decoder.decodeFile(encoded, audio::kMaxSourceSoundSamples);
                if (encoded.empty() || !std::filesystem::path(path).is_absolute() ||
                    decoded.status != audio::DecodeStatus::kOk || decoded.sound.empty()) {
                    MessageBoxW(dialog, L"请选择有效的音频文件（最长 5 分钟）。", L"音效不可用", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                decoded = audio::selectRange(decoded.sound, state->document.entrance_sound.start_ms,
                                             state->document.entrance_sound.end_ms);
                if (decoded.status != audio::DecodeStatus::kOk) {
                    MessageBoxW(dialog, L"请编辑播放范围，选择不超过 30 秒的片段。", L"播放范围无效", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
            }
            auto& sound = state->document.entrance_sound;
            sound.enabled = IsDlgButtonChecked(dialog, IDC_ENABLED) == BST_CHECKED;
            sound.local_monitor = IsDlgButtonChecked(dialog, IDC_MONITOR) == BST_CHECKED;
            sound.sound_file = utf8(path);
            sound.volume = static_cast<float>(volume) / 100.0f;
            sound.delay_ms = static_cast<int>(delay);
            sound.cooldown_seconds = static_cast<int>(cooldown);
            if (!state->store.save(state->document)) {
                MessageBoxW(dialog, L"无法保存设置，请检查配置目录权限。", L"保存失败", MB_OK | MB_ICONERROR);
                return TRUE;
            }
            MessageBoxW(dialog, L"设置已保存。请重新启动 TeamSpeak 以应用。", L"TS3Toolkit", MB_OK | MB_ICONINFORMATION);
            EndDialog(dialog, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}
}  // namespace

void showSettingsDialog(void* parent, const std::string& config_directory) {
    if (config_directory.empty()) return;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);
    DialogState state{config::ConfigStore((std::filesystem::u8path(config_directory) / "ts3toolkit.ini").u8string()), {}};
    auto loaded = state.store.load();
    if (loaded.status == config::LoadStatus::kRejected) {
        MessageBoxW(static_cast<HWND>(parent), L"配置文件无效，未覆盖原文件。", L"TS3Toolkit", MB_OK | MB_ICONERROR);
        return;
    }
    state.document = std::move(loaded.document);
    if (!state.document.entrance_sound.sound_file.empty() &&
        wide(state.document.entrance_sound.sound_file).empty()) {
        MessageBoxW(static_cast<HWND>(parent), L"音效路径编码无效，未覆盖原配置。", L"TS3Toolkit", MB_OK | MB_ICONERROR);
        return;
    }
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&showSettingsDialog), &module);
    DialogBoxParamW(module, MAKEINTRESOURCEW(IDD_SETTINGS), static_cast<HWND>(parent), dialogProc,
                    reinterpret_cast<LPARAM>(&state));
}
}  // namespace ts3toolkit::plugin

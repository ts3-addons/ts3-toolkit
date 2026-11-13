#include "settings_dialog.h"
#include "settings_resource.h"

#include <windows.h>
#include <commdlg.h>

#include <cmath>
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

private:
    static void callback(ma_device* device, void* output, const void*,
                         ma_uint32 frames) {
        auto* self = static_cast<LocalPreview*>(device->pUserData);
        self->engine_.fillFrame(static_cast<core::Sample*>(output), frames);
    }

    audio::AudioEngine engine_{core::kFrameSamples * 10};
    ma_device device_{};
    bool running_ = false;
};

struct DialogState {
    config::ConfigStore store;
    config::SettingsDocument document;
    LocalPreview preview;
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

INT_PTR CALLBACK dialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (message == WM_INITDIALOG) {
        state = reinterpret_cast<DialogState*>(lparam);
        SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
        const auto& sound = state->document.entrance_sound;
        CheckDlgButton(dialog, IDC_ENABLED, sound.enabled ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(dialog, IDC_MONITOR, sound.local_monitor ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemTextW(dialog, IDC_SOUND_PATH, wide(sound.sound_file).c_str());
        SetDlgItemInt(dialog, IDC_VOLUME, static_cast<UINT>(std::lround(sound.volume * 100)), FALSE);
        SetDlgItemInt(dialog, IDC_DELAY, sound.delay_ms, FALSE);
        SetDlgItemInt(dialog, IDC_COOLDOWN, sound.cooldown_seconds, FALSE);
        return TRUE;
    }
    if (message != WM_COMMAND || !state) return FALSE;
    switch (LOWORD(wparam)) {
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
            const auto decoded = decoder.decodeFile(encoded);
            if (path.empty() || encoded.empty() ||
                !std::filesystem::path(path).is_absolute() ||
                decoded.status != audio::DecodeStatus::kOk || decoded.sound.empty()) {
                MessageBoxW(dialog, L"请选择有效的音频文件（最长 30 秒）。", L"音效不可用", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            if (!state->preview.play(decoded.sound, static_cast<float>(volume) / 100.0f)) {
                MessageBoxW(dialog, L"无法打开系统默认播放设备。", L"试听失败", MB_OK | MB_ICONERROR);
            }
            return TRUE;
        }
        case IDC_STOP_PREVIEW:
            state->preview.stop();
            return TRUE;
        case IDOK: {
            BOOL volume_ok = FALSE, delay_ok = FALSE, cooldown_ok = FALSE;
            UINT volume = GetDlgItemInt(dialog, IDC_VOLUME, &volume_ok, FALSE);
            UINT delay = GetDlgItemInt(dialog, IDC_DELAY, &delay_ok, FALSE);
            UINT cooldown = GetDlgItemInt(dialog, IDC_COOLDOWN, &cooldown_ok, FALSE);
            if (!volume_ok || volume > 100 || !delay_ok || delay > 30000 ||
                !cooldown_ok || cooldown > 300) {
                MessageBoxW(dialog, L"请检查音量、延迟和冷却时间的范围。", L"设置无效", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            const std::wstring path = text(dialog, IDC_SOUND_PATH);
            if (!path.empty()) {
                const std::string encoded = utf8(path);
                audio::SoundDecoder decoder;
                const auto decoded = decoder.decodeFile(encoded);
                if (encoded.empty() || !std::filesystem::path(path).is_absolute() ||
                    decoded.status != audio::DecodeStatus::kOk || decoded.sound.empty()) {
                    MessageBoxW(dialog, L"请选择有效的音频文件（最长 30 秒）。", L"音效不可用", MB_OK | MB_ICONWARNING);
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
    DialogState state{config::ConfigStore((std::filesystem::path(config_directory) / "ts3toolkit.ini").string()), {}};
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

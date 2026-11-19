# TS3Toolkit

简体中文 | [English](README.md)

TS3Toolkit 是一个面向 Windows x64 的 TeamSpeak 3 原生客户端插件。目前提供
“个人进频道音效”：进入频道时，将指定音频混入本地麦克风流，频道内其他人都能
听到；无需服务端组件，也不要求其他用户安装插件。

## 功能

- 通过 TeamSpeak 麦克风音频回调混入进频道音效。
- 功能空闲或发生错误时，麦克风仍保持正常透传。
- 可配置启用状态、音量、延迟、冷却时间和音频片段范围。
- 支持 WAV、MP3、FLAC，并统一转换为 48 kHz、单声道、16-bit PCM。
- 可在设置窗口预览选中的音频片段。
- 可生成双击安装的 `.ts3_plugin` 插件包。

## 安装

1. 按下方命令构建插件包。
2. 在 TeamSpeak 3 中打开插件管理器，安装
   `build/package/ts3toolkit-0.1.0.ts3_plugin`。
3. 根据提示重启 TeamSpeak。
4. 打开 TS3Toolkit 设置并选择音频文件，在 TeamSpeak 的采集设置中选择正常的麦克风。

## 构建与测试

依赖：Windows x64、Visual Studio/MSVC（含 Windows SDK）、CMake，以及 CMake
支持的构建生成器。

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Release --target ts3toolkit_package
```

插件包输出到 `build/package/`。

## 架构

项目按层组织，依赖方向由外向内：

- `core` — PCM 类型、混音、数学运算和有界缓冲区。
- `audio` — 麦克风采集、解码、重采样和播放。
- `config` — 版本化设置存储与校验。
- `entrance` — 触发、延迟、冷却和播放状态。
- `plugin` — TeamSpeak 3 入口、事件和音频回调。
- `platform` — Windows 平台辅助代码。

目录结构：

```text
include/ts3toolkit/  公共头文件
src/                 按层组织的实现
tests/               单元测试
docs/                行为与架构契约
packaging/           .ts3_plugin 打包脚本
third_party/         第三方 SDK 与 miniaudio
```

## 文档

- [架构说明](docs/architecture.md) — 分层边界与数据流。
- [进频道音效契约](docs/entrance-sound.md) — 触发、音频和设置规则。

## 许可证

本项目采用 [MIT License](LICENSE) 许可协议，版权所有 © 2026 TheSkyC。

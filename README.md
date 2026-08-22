# SimpleAimAssist

> Fallout 4 的原生 F4SE 辅助瞄准与目标锁定模组，支持键鼠、手柄和目标标记。  
> A native F4SE aim-assist and target-lock mod for Fallout 4 with keyboard, gamepad, and target markers.

**暂时不支持最新的 Fallout 4 AE 版本（1.11.240）。**
**The latest Fallout 4 AE version (1.11.240) is not supported at this time.**


[![Status](https://img.shields.io/badge/status-stable-2ea44f?style=for-the-badge)](releases/v1.2.4/RELEASE_NOTES.md)
[![Release](https://img.shields.io/badge/release-v1.2.4-0969da?style=for-the-badge)](releases/v1.2.4/)
[![Fallout 4](https://img.shields.io/badge/Fallout%204-1.10.163-4e73df?style=for-the-badge)](https://f4se.silverlock.org/)
[![F4SE](https://img.shields.io/badge/F4SE-0.6.23-8250df?style=for-the-badge)](https://f4se.silverlock.org/)
[![Input](https://img.shields.io/badge/input-gamepad%20%7C%20keyboard%2Fmouse-f0883e?style=for-the-badge)](#features)
[![License](https://img.shields.io/badge/license-Non--Commercial%20Open%20License-dc2626?style=for-the-badge)](LICENSE.md)

<p align="center">
  <b>[AIM] Aim Assist</b>&nbsp;&nbsp;·&nbsp;&nbsp;
  <b>[LOCK] Target Lock</b>&nbsp;&nbsp;·&nbsp;&nbsp;
  <b>[RANGE] Melee / Ranged</b>&nbsp;&nbsp;·&nbsp;&nbsp;
  <b>[HUD] Target Marker</b>
</p>

## 中文

SimpleAimAssist 为 Fallout 4 提供轻量的辅助瞄准和目标锁定功能。项目包含 F4SE 原生插件、MCM 配置、Papyrus 接口和可选的 Scaleform 目标标记。

### 功能

| 标记 | 功能 |
|---|---|
| `[AIM]` | 普通瞄准状态下的辅助瞄准 |
| `[LOCK]` | 独立目标锁定模式 |
| `[INPUT]` | 键鼠和手柄参数分离 |
| `[RANGE]` | 近战与远程武器使用不同锁定距离 |
| `[ANCHOR]` | 目标锚点和移动准星后的延迟回锚点 |
| `[MCM]` | 目标切换、吸附范围、吸附强度和高倍镜限制设置 |
| `[HUD]` | 可选的目标头顶 HUD 标记 |
| `[LAB]` | 可选的未进入战斗敌对目标锁定实验功能 |

### 前置要求

- Fallout 4。
- 与游戏版本匹配的 F4SE。
- Address Library for F4SE Plugins。
- MCM / F4MCM。
- Microsoft Visual C++ x64 Redistributable。

当前最终包主要验证于 Fallout 4 runtime 1.10.163 与 F4SE 0.6.23。其他 runtime 需要安装对应版本的 F4SE 和 Address Library，并进行实际测试。

核心辅助瞄准不需要 ESP、ESL、HUDFramework 或 FallUI。目标标记由 F4SE 和 Scaleform SWF 提供。

### 仓库结构

```text
Source/                         源码和构建脚本
releases/v1.2.4/                中文和英文安装包
```

### 构建

源码不绑定开发者本机路径。先设置 CommonLibF4 路径：

```powershell
$env:COMMONLIBF4_ROOT = 'D:\path\to\CommonLibF4'
cd Source
xmake build -P . -r -j2
```

Papyrus 和 SWF 构建脚本需要分别提供 Caprica、Papyrus flags、Fallout 4 source、Apache Flex SDK 和 playerglobal.swc 的路径。详见 `Source/build_papyrus.ps1` 与 `Source/build_target_marker_swf.ps1`。

可使用以下环境变量：`CAPRICA_PATH`、`PAPYRUS_FLAGS`、`PAPYRUS_GAME_SOURCE`、`FLEX_SDK` 和 `PLAYERGLOBAL_SWC`。

### 最终版本

- [SimpleAimAssist 1.2.4 CHS](releases/v1.2.4/SimpleAimAssist_1.2.4_CHS.zip)
- [SimpleAimAssist 1.2.4 EN](releases/v1.2.4/SimpleAimAssist_1.2.4_EN.zip)
- [1.2.4 Release Notes](releases/v1.2.4/RELEASE_NOTES.md)

## English

SimpleAimAssist is a lightweight Fallout 4 F4SE mod that provides aim assistance and target locking. The repository includes the native F4SE plugin, MCM configuration, Papyrus bindings, and an optional Scaleform target marker.

### Features

| Marker | Feature |
|---|---|
| `[AIM]` | Aim assistance driven by the actual game aim state |
| `[LOCK]` | Optional independent target-lock mode |
| `[INPUT]` | Separate keyboard/mouse and gamepad tuning |
| `[RANGE]` | Different lock distances for melee and ranged weapons |
| `[ANCHOR]` | Target anchors and delayed return after manual crosshair movement |
| `[MCM]` | Configurable target switching, search cone, aim strength, and high-zoom behavior |
| `[HUD]` | Optional HUD marker above the selected target |
| `[LAB]` | Experimental support for hostile targets that have not entered combat |

### Requirements

- Fallout 4.
- The F4SE build matching the installed game runtime.
- Address Library for F4SE Plugins.
- MCM / F4MCM.
- Microsoft Visual C++ x64 Redistributable.

The current release was primarily validated on Fallout 4 runtime 1.10.163 with F4SE 0.6.23. Other runtimes require their matching F4SE and Address Library versions and should be tested in-game.

The core aim assist does not require ESP, ESL, HUDFramework, or FallUI. The target marker is provided by F4SE and a Scaleform SWF.

### Build

The source tree does not depend on the original developer's local paths. Set the CommonLibF4 checkout before building:

```powershell
$env:COMMONLIBF4_ROOT = 'D:\path\to\CommonLibF4'
cd Source
xmake build -P . -r -j2
```

Papyrus and SWF builds require paths to Caprica, Papyrus flags, Fallout 4 source files, Apache Flex SDK, and playerglobal.swc. See `Source/build_papyrus.ps1` and `Source/build_target_marker_swf.ps1`.

The scripts accept the environment variables `CAPRICA_PATH`, `PAPYRUS_FLAGS`, `PAPYRUS_GAME_SOURCE`, `FLEX_SDK`, and `PLAYERGLOBAL_SWC`.

## License

This project uses a custom **Non-Commercial Open License**. Non-commercial use, modification, patches, forks, non-commercial mod packs, and redistribution are allowed with attribution. Commercial use, paid bundling, paid support, and selling modified or compiled versions require prior permission from the author.

本项目采用自定义的**非商业开放许可**。允许保留署名后的非商业使用、修改、补丁、分支、非商业整合包和再发布；商业使用、付费整合、付费支持以及销售修改版或编译版需要事先联系作者。

See [LICENSE.md](LICENSE.md) for the complete bilingual terms. / 完整双语条款请查看 [LICENSE.md](LICENSE.md)。

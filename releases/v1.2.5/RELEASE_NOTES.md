# SimpleAimAssist 1.2.5

## 中文

### 新增功能

- 增加“键鼠（仅键鼠）”模式，只允许键盘/鼠标作为普通辅助的输入来源。
- 增加“手柄（仅手柄）”模式，只允许手柄作为普通辅助的输入来源。
- 增加“键鼠共存（自动切换）”模式，根据最近一次输入设备自动切换键鼠或手柄参数。
- 输入设备检测只观察输入事件的设备类型，不监控具体按键，也不抢占普通输入，减少与键鼠共存类 Mod 的冲突。
- 增加“持续准星追踪”开关。
  - 开启时，准星会持续跟随当前锁定目标。
  - 关闭时，普通辅助仍使用正常的校正逻辑，直到准星到达当前选择的锚点后停止跟随。
  - 退出瞄准、目标失效、视线丢失或高倍镜限制触发时立即停止。
  - 独立锁定模式不受此开关影响，仍保持持续跟踪。
- 增加“快速切换锚点”热键，按身体、头部、下体的顺序循环切换。
- 中文版和英文版的锚点切换提示分别显示本地化文本：
  - 中文：瞄准身体、瞄准头部、瞄准下体
  - English：Aim at Body、Aim at Head、Aim at Lower Body

### 修复与兼容

- 修复关闭持续追踪时，首次校正可能被首帧零时间差跳过的问题。
- 修复重新进入瞄准后无法重新执行初始校正的问题。
- 原生插件加入 Fallout 4 AE 1.11.240 兼容声明。
- 使用支持 AE 240 的多运行时 CommonLibF4 基线构建，同时保留旧版运行时声明。

本版本已完成源码、DLL、Papyrus 和包体静态验证；键鼠共存、持续追踪和 AE 240 的游戏内行为仍需在目标环境中实测。

## English

### New Features

- Added Keyboard / Mouse-only mode, which accepts keyboard and mouse as the normal-assist input source.
- Added Gamepad-only mode, which accepts the gamepad as the normal-assist input source.
- Added Keyboard / Mouse + Gamepad (Auto), which switches between keyboard/mouse and gamepad tuning from the most recent input device.
- Device detection observes only the input event's device type. It does not monitor specific buttons or consume normal input, reducing conflicts with keyboard-and-gamepad coexistence mods.
- Added the Continuous Crosshair Tracking switch.
  - When enabled, the crosshair continuously follows the current locked target.
  - When disabled, normal assist keeps using the standard correction logic until the selected aim anchor is reached, then stops following.
  - Tracking stops immediately when aiming ends, the target becomes invalid, line of sight is lost, or the high-zoom restriction applies.
  - Independent Lock ignores this switch and continues to track normally.
- Added the Quick Cycle Aim Anchor hotkey, cycling Body, Head, and Lower Body.
- Localized anchor-cycle HUD messages:
  - Chinese: 瞄准身体, 瞄准头部, 瞄准下体
  - English: Aim at Body, Aim at Head, Aim at Lower Body

### Fixes and Compatibility

- Fixed an issue where the initial correction could be skipped by the zero-delta first frame when continuous tracking was disabled.
- Fixed an issue where the initial correction could not be performed again after re-entering aim.
- Added Fallout 4 AE 1.11.240 to the native compatibility declaration.
- Built against a multiruntime CommonLibF4 baseline with AE 240 support while retaining the older runtime declarations.

Source, DLL, Papyrus, and package static checks are complete. Keyboard/gamepad coexistence, tracking behavior, and AE 240 in-game behavior still require testing in the target environment.

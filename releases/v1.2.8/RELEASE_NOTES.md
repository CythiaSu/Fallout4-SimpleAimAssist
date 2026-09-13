# SimpleAimAssist 1.2.8

## 中文

本版本主要修复两类运行时问题，并完善三种输入模式的独立锁定路径。

### 主要修复

- 修复上次更新导致的独立瞄准/独立锁定功能失效问题。
- 修复使用轮转机枪等需要持续攻击的武器时，辅助瞄准会失效的问题。

### 输入模式兼容

- 键鼠模式：只接受键鼠自定义热键，右摇杆不被截取。
- 手柄模式：只接受右摇杆短按，键鼠热键被忽略。
- 自动模式：键鼠热键和右摇杆短按都可用，右摇杆长按动作保持不变。
- 切换输入模式时清除旧的独立锁定状态，避免目标状态串到新模式。

本版本已完成本地 DLL 构建和包体静态检查；仍需要在目标 Fallout 4、F4SE 和 Address Library 环境中进行游戏内验证。

## English

This release fixes two runtime issues and completes the independent-lock input routing for all three input modes.

### Main fixes

- Fixed independent aim/target-lock functionality becoming unavailable after the previous update.
- Fixed aim assist stopping when using sustained-fire weapons such as rotary miniguns.

### Input-mode compatibility

- Keyboard/mouse mode accepts only the custom keyboard/mouse lock hotkey; the right stick is not intercepted.
- Gamepad mode accepts only the right-stick short press; the keyboard/mouse hotkey is ignored.
- Auto mode accepts both the keyboard/mouse hotkey and right-stick short press; the right-stick long-press action is preserved.
- Changing input mode clears the previous independent-lock state so it cannot leak into the new mode.

The local DLL build and package contents were verified; in-game validation with the target Fallout 4, F4SE, and Address Library environment is still required.

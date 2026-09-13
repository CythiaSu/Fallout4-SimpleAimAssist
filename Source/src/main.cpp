#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <chrono>
#include <unordered_set>
#include <vector>

#include "F4SE/F4SE.h"
#include "F4SE/API.h"
#include "RE/B/BSScaleformManager.h"
#include "RE/Fallout.h"
#include "RE/H/HUDMenuUtils.h"
#include "RE/M/MenuOpenCloseEvent.h"
#include "RE/U/UI.h"
#include "RE/U/UIMessageQueue.h"

#define SAA_EXPORT extern "C" [[maybe_unused]] __declspec(dllexport)

namespace
{
	namespace fs = std::filesystem;

	constexpr float kPi = 3.14159265358979323846F;
	constexpr float kDegToRad = kPi / 180.0F;
	constexpr float kRadToDeg = 180.0F / kPi;
	// Manual reticle control stays inside a small box around the locked target anchor.
	constexpr float kManualAimMaxYawOffset = 6.0F * kDegToRad;
	constexpr float kManualAimMaxPitchOffset = 4.0F * kDegToRad;
	constexpr float kTargetSwitchGestureResetDelay = 0.14F;
	constexpr float kTargetSwitchRequestDelay = 0.04F;
	constexpr float kTargetSwitchCooldown = 0.22F;
	constexpr float kGamepadTargetLockHoldSeconds = 0.25F;
	constexpr std::string_view kTargetMarkerMenuName = "SimpleAimAssistTargetMarkerMenu";
	constexpr std::string_view kTargetMarkerMoviePath = "Interface/SimpleAimAssist/TargetMarkerNative.swf";

	enum class AssistState
	{
		kIdle,
		kAssisting,
		kWaitingToRetarget,
		kHoldingNoRetarget
	};

	enum class InputMode
	{
		kKeyboardMouse = 0,
		kGamepad = 1,
		kAuto = 2
	};

	enum class InputDevice
	{
		kUnknown,
		kKeyboardMouse,
		kGamepad
	};

	enum class WeaponRangeClass
	{
		kUnknown,
		kMelee,
		kRanged
	};

	enum class AimAnchor
	{
		kTorso = 0,
		kHead = 1,
		kLowerBody = 2
	};

	enum class AimActivationMode
	{
		kFollowAimState = 0,
		kToggleLock = 1
	};

	enum class GamepadLongPressAction
	{
		kSneak = 0,
		kTogglePOV = 1
	};

	struct MeleeGuardSignals
	{
		bool         active{ false };
		bool         wantBlocking{ false };
		bool         blockedGunState{ false };
		bool         hasGraphIsBlocking{ false };
		bool         graphIsBlocking{ false };
		bool         hasGraphWantBlock{ false };
		std::int32_t graphWantBlock{ 0 };
	};

	struct TargetSearchDiagnostics
	{
		std::size_t scanned{ 0 };
		std::size_t dead{ 0 };
		std::size_t notInCombat{ 0 };
		std::size_t notHostile{ 0 };
		std::size_t invalidTarget{ 0 };
		std::size_t belowMinDistance{ 0 };
		std::size_t aboveMaxDistance{ 0 };
		std::size_t invalidDirection{ 0 };
		std::size_t behindCamera{ 0 };
		std::size_t outsideCone{ 0 };
		std::uint32_t closestRejectedFormID{ 0 };
		float closestRejectedDistance{ std::numeric_limits<float>::max() };
		float closestRejectedAngle{ -1.0F };
		bool closestRejectedInCombat{ false };
		bool closestRejectedHostile{ false };
		bool closestRejectedValidTarget{ false };
		std::string_view closestRejectedReason{ "none" };
	};

	struct Config
	{
		bool  enabled{ true };
		bool  logging{ true };
		bool  autoTriggerOnZoom{ true };
		bool  disableOnHighZoom{ true };
		bool  targetMarkerEnabled{ true };
		int   targetMarkerStyle{ 6 };
		bool  onlyCombatTargets{ true };
		bool  allowOutOfCombatHostileTargets{ false };
		bool  continueCrosshairTracking{ true };
		bool  showTargetAcquiredMessage{ false };
		bool  showLockHudHint{ false };
		bool  focusEnabled{ false };
		bool  useRaceAimPoints{ true };
		float highZoomRatio{ 1.60F };
		float searchConeDegrees{ 12.0F };
		float lockRetentionDistanceScale{ 1.15F };
		float minLockDistance{ 250.0F };
		float maxLockDistance{ 2500.0F };
		float meleeMaxLockDistance{ 900.0F };
		AimAnchor aimAnchor{ AimAnchor::kTorso };
		float retargetDelay{ 0.12F };
		float noFireTimeoutSeconds{ 0.0F };
		bool  noFireTimeoutAffectsIndependentLock{ false };
		float lockYawResponse{ 9.0F };
		float lockPitchResponse{ 6.5F };
		float lockMaxYawSpeed{ 220.0F };
		float lockMaxPitchSpeed{ 160.0F };
		float lockDeadZoneDegrees{ 0.06F };
		float targetLeadSeconds{ 0.065F };
		float targetLeadMaxDistance{ 65.0F };
		bool  requireLineOfSight{ true };
		float visibleHitFraction{ 0.30F };
		float lineOfSightCheckInterval{ 0.25F };
		float lineOfSightGracePeriod{ 0.25F };
		float assistDuration{ 1.20F };
		float assistStrength{ 0.25F };
		float maxAssistSpeed{ 95.0F };
		float assistSmoothing{ 0.22F };
		float assistDeadZoneDegrees{ 0.25F };
		float horizontalOffsetDegrees{ 0.0F };
		float torsoHeight{ 95.0F };
		float targetSpeedSoftLimit{ 750.0F };
		float targetSpeedHardLimit{ 1500.0F };
		float stickyAssistSeconds{ 1.10F };
		float stickyStrengthBonus{ 0.24F };
		float stickySpeedSoftLimit{ 1450.0F };
		float stickySpeedHardLimit{ 2800.0F };
		float closeRangeBoostDistance{ 900.0F };
		float farAssistFalloffStart{ 1100.0F };
		float farAssistMinScale{ 0.72F };
		float lockStickDegrees{ 2.10F };
		float lockStickStrength{ 0.15F };
		float lockStickMaxSpeed{ 46.0F };
		float assistStrengthScale{ 0.5F };
		float manualAimDeadzone{ 0.18F };
		float manualAimInputHold{ 0.12F };
		float manualAimReturnDelay{ 0.08F };
		float manualAimReturnDuration{ 0.22F };
		float manualAimGamepadOffsetRetention{ 1.00F };
		float manualAimKeyboardMouseOffsetRetention{ 1.00F };
		int   targetSwitchLevel{ 2 };
		AimActivationMode aimActivationMode{ AimActivationMode::kFollowAimState };
		GamepadLongPressAction gamepadLongPressAction{ GamepadLongPressAction::kSneak };
		float targetFrictionStrength{ 0.35F };
		float focusTimeScale{ 0.65F };
		float focusAPPerSecond{ 18.0F };
		float focusMinAP{ 5.0F };
		float hudHintCooldown{ 1.25F };
	};

	struct AssistRuntime
	{
		AssistState     state{ AssistState::kIdle };
		RE::ActorHandle target{};
		float           startTime{ 0.0F };
		float           assistStartTime{ 0.0F };
		float           nextRetargetTime{ 0.0F };
		float           baseFOV{ 80.0F };
		float           lastZoomRatio{ 1.0F };
		float           lastYawStep{ 0.0F };
		float           lastPitchStep{ 0.0F };
		float           lastYawError{ 0.0F };
		float           lastPitchError{ 0.0F };
		float           lastAimErrorDegrees{ std::numeric_limits<float>::max() };
		float           lastTargetSampleTime{ 0.0F };
		float           nextLineOfSightCheck{ 0.0F };
		float           lostLineOfSightSince{ -1.0F };
		float           targetSpeed{ 0.0F };
		float           targetDistance{ 0.0F };
		RE::NiPoint3    lastTargetPoint{};
		RE::NiPoint3    smoothedTargetVelocity{};
		std::uint32_t   targetFormID{ 0 };
		bool            hasLastTargetPoint{ false };
		bool            autoActive{ false };
		bool            manualAimWasActive{ false };
		float           manualAimOverrideUntil{ 0.0F };
		float           manualAimRecoveryStart{ -1.0F };
		bool            manualAimOffsetPending{ false };
		bool            manualAimOffsetValid{ false };
		float           manualAimCaptureAfter{ 0.0F };
		float           manualAimOffsetYaw{ 0.0F };
		float           manualAimOffsetPitch{ 0.0F };
		float           lastManualAimInputTime{ 0.0F };
		bool            targetSwitchGestureActive{ false };
		bool            targetSwitchRequested{ false };
		float           targetSwitchAt{ 0.0F };
		float           targetSwitchCooldownUntil{ 0.0F };
		float           lastAimActivityTime{ 0.0F };
		bool            initialAssistCorrectionDone{ false };
		bool            normalAssistSourceLatched{ false };
	};

	Config        g_config{};
	AssistRuntime g_runtime{};
	std::mutex    g_logMutex;
	bool          g_inputSinkRegistered = false;
	bool          g_zoomHeld = false;
	bool          g_keyboardMouseFireHeld = false;
	bool          g_gamepadFireHeld = false;
	bool          g_toggleLockActive = false;
	bool          g_gamepadLockButtonHeld = false;
	float         g_gamepadLockButtonPressedAt = 0.0F;
	bool          g_userEnabled = true;
	bool          g_focusRequested = false;
	bool          g_focusActive = false;
	float         g_mcmAssistStrengthScale = 0.5F;
	float         g_mcmTargetFrictionStrength = 0.35F;
	int           g_mcmTargetSwitchLevel = 2;
	int           g_mcmTargetMarkerStyle = 5;
	float         g_mcmSearchConeDegrees = 14.0F;
	float         g_mcmMeleeLockDistance = 900.0F;
	float         g_mcmRangedLockDistance = 2400.0F;
	int           g_mcmAimAnchor = 0;
	int           g_mcmManualAimReturnDelay = 1;
	InputMode     g_inputMode = InputMode::kGamepad;
	InputDevice   g_lastInputDevice = InputDevice::kUnknown;
	float         g_lastInputDeviceAt = -1.0F;
	bool          g_inputModeInitialized = false;
	bool          g_targetMarkerMenuRegistered = false;
	bool          g_targetMarkerMenuReady = false;
	bool          g_targetMarkerMenuSinkRegistered = false;
	bool          g_lastLoggedMeleeGuardState = false;
	bool          g_hasLoggedMeleeGuardState = false;
	WeaponRangeClass g_lastLoggedWeaponRangeClass = WeaponRangeClass::kUnknown;
	bool             g_hasLoggedWeaponRangeClass = false;
	WeaponRangeClass g_cachedWeaponRangeClass = WeaponRangeClass::kUnknown;
	std::uint32_t    g_cachedWeaponFormID = 0;
	float            g_nextWeaponRangeRefreshTime = -100.0F;
	float         g_lastFrameTime = 0.0F;
	float         g_nextTargetMarkerEnsureTime = 0.0F;
	float         g_nextTargetSearchDiagnosticTime = -100.0F;
	float         g_lastHudHintTime = -100.0F;
	std::unordered_set<std::string> g_loggedInputEvents;

	void Log(const std::string& a_msg);
	float NowSeconds();

	float Clamp(float a_value, float a_min, float a_max)
	{
		return std::clamp(a_value, a_min, a_max);
	}

	InputMode InputModeFromInt(int a_mode)
	{
		switch (a_mode) {
		case 0:
			return InputMode::kKeyboardMouse;
		case 2:
			return InputMode::kAuto;
		default:
			return InputMode::kGamepad;
		}
	}

	int InputModeToInt(InputMode a_mode)
	{
		switch (a_mode) {
		case InputMode::kKeyboardMouse:
			return 0;
		case InputMode::kAuto:
			return 2;
		default:
			return 1;
		}
	}

	InputDevice InputDeviceFromRuntime(RE::INPUT_DEVICE a_device)
	{
		switch (a_device) {
		case RE::INPUT_DEVICE::kKeyboard:
		case RE::INPUT_DEVICE::kMouse:
			return InputDevice::kKeyboardMouse;
		case RE::INPUT_DEVICE::kGamepad:
			return InputDevice::kGamepad;
		default:
			return InputDevice::kUnknown;
		}
	}

	InputDevice SelectedInputDevice()
	{
		return g_inputMode == InputMode::kGamepad ?
			InputDevice::kGamepad : InputDevice::kKeyboardMouse;
	}

	InputDevice ActiveProfileDevice()
	{
		if (g_inputMode == InputMode::kAuto) {
			return g_lastInputDevice == InputDevice::kGamepad ?
				InputDevice::kGamepad : InputDevice::kKeyboardMouse;
		}
		return SelectedInputDevice();
	}

	bool IsInputDeviceAllowedByMode(InputDevice a_device)
	{
		return a_device != InputDevice::kUnknown &&
			(g_inputMode == InputMode::kAuto || a_device == SelectedInputDevice());
	}

	bool IsNormalAssistSourceAllowed()
	{
		return g_inputMode == InputMode::kAuto ||
			g_lastInputDevice == SelectedInputDevice();
	}

	bool IsFireInputHeld()
	{
		switch (g_inputMode) {
		case InputMode::kKeyboardMouse:
			return g_keyboardMouseFireHeld;
		case InputMode::kGamepad:
			return g_gamepadFireHeld;
		case InputMode::kAuto:
			return g_keyboardMouseFireHeld || g_gamepadFireHeld;
		default:
			return false;
		}
	}

	void SetFireInputHeld(InputDevice a_device, bool a_held)
	{
		switch (a_device) {
		case InputDevice::kKeyboardMouse:
			g_keyboardMouseFireHeld = a_held;
			break;
		case InputDevice::kGamepad:
			g_gamepadFireHeld = a_held;
			break;
		default:
			break;
		}
	}

	std::string_view InputDeviceName(InputDevice a_device)
	{
		switch (a_device) {
		case InputDevice::kKeyboardMouse:
			return "keyboard/mouse";
		case InputDevice::kGamepad:
			return "gamepad";
		default:
			return "unknown";
		}
	}

	std::string_view InputModeName(InputMode a_mode)
	{
		switch (a_mode) {
		case InputMode::kKeyboardMouse:
			return "keyboard/mouse only";
		case InputMode::kAuto:
			return "keyboard/mouse + gamepad auto";
		default:
			return "gamepad only";
		}
	}

	std::string_view AimAnchorName(AimAnchor a_anchor)
	{
		switch (a_anchor) {
		case AimAnchor::kHead:
			return "Head";
		case AimAnchor::kLowerBody:
			return "Lower Body";
		default:
			return "Body";
		}
	}

	std::string_view AimAnchorChineseName(AimAnchor a_anchor)
	{
		switch (a_anchor) {
		case AimAnchor::kHead:
			return "瞄准头部";
		case AimAnchor::kLowerBody:
			return "瞄准下体";
		default:
			return "瞄准身体";
		}
	}

	struct TargetSwitchThresholds
	{
		float gamepad;
		float keyboardMouse;
	};

	TargetSwitchThresholds GetTargetSwitchThresholds()
	{
		switch (g_config.targetSwitchLevel) {
		case 1:
			return { 0.58F, 20.0F };
		case 2:
			return { 0.44F, 14.0F };
		case 3:
			return { 0.32F, 9.0F };
		case 4:
			return { 0.22F, 6.0F };
		default:
			return { 1.01F, std::numeric_limits<float>::max() };
		}
	}

	float NormalizeAngle(float a_angle)
	{
		while (a_angle > kPi) {
			a_angle -= 2.0F * kPi;
		}
		while (a_angle < -kPi) {
			a_angle += 2.0F * kPi;
		}
		return a_angle;
	}

	float PointDistance(const RE::NiPoint3& a_lhs, const RE::NiPoint3& a_rhs)
	{
		const auto delta = a_lhs - a_rhs;
		return std::sqrt((delta.x * delta.x) + (delta.y * delta.y) + (delta.z * delta.z));
	}

	float ReadIniFloat(const std::string& a_section, const std::string& a_key, float a_default)
	{
		std::ifstream in("Data/F4SE/Plugins/SimpleAimAssist.ini");
		if (!in) {
			in.open("Y:/Workspace/FO4辅助瞄准/F4SE/Plugins/SimpleAimAssist.ini");
		}
		if (!in) {
			return a_default;
		}

		std::string line;
		std::string currentSection;
		while (std::getline(in, line)) {
			const auto comment = line.find_first_of(";#");
			if (comment != std::string::npos) {
				line.erase(comment);
			}

			const auto first = line.find_first_not_of(" \t\r\n");
			if (first == std::string::npos) {
				continue;
			}
			const auto last = line.find_last_not_of(" \t\r\n");
			line = line.substr(first, last - first + 1);

			if (line.size() >= 3 && line.front() == '[' && line.back() == ']') {
				currentSection = line.substr(1, line.size() - 2);
				continue;
			}
			if (currentSection != a_section) {
				continue;
			}

			const auto eq = line.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			auto key = line.substr(0, eq);
			auto value = line.substr(eq + 1);
			const auto keyFirst = key.find_first_not_of(" \t");
			const auto keyLast = key.find_last_not_of(" \t");
			if (keyFirst == std::string::npos) {
				continue;
			}
			key = key.substr(keyFirst, keyLast - keyFirst + 1);
			if (key == a_key) {
				return std::strtof(value.c_str(), nullptr);
			}
		}

		return a_default;
	}

	float ReadMcmFloat(const std::string& a_section, const std::string& a_key, float a_default)
	{
		std::ifstream in("Data/MCM/Settings/SimpleAimAssist.ini");
		if (!in) {
			return a_default;
		}

		std::string line;
		std::string currentSection;
		while (std::getline(in, line)) {
			const auto comment = line.find_first_of(";#");
			if (comment != std::string::npos) {
				line.erase(comment);
			}

			const auto first = line.find_first_not_of(" \t\r\n");
			if (first == std::string::npos) {
				continue;
			}
			const auto last = line.find_last_not_of(" \t\r\n");
			line = line.substr(first, last - first + 1);

			if (line.size() >= 3 && line.front() == '[' && line.back() == ']') {
				currentSection = line.substr(1, line.size() - 2);
				continue;
			}
			if (currentSection != a_section) {
				continue;
			}

			const auto eq = line.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			auto key = line.substr(0, eq);
			auto value = line.substr(eq + 1);
			const auto keyFirst = key.find_first_not_of(" \t");
			const auto keyLast = key.find_last_not_of(" \t");
			if (keyFirst == std::string::npos) {
				continue;
			}
			key = key.substr(keyFirst, keyLast - keyFirst + 1);
			if (key == a_key) {
				return std::strtof(value.c_str(), nullptr);
			}
		}

		return a_default;
	}

	bool ReadIniBool(const std::string& a_section, const std::string& a_key, bool a_default)
	{
		return ReadIniFloat(a_section, a_key, a_default ? 1.0F : 0.0F) != 0.0F;
	}

	int ReadIniInt(const std::string& a_section, const std::string& a_key, int a_default)
	{
		return static_cast<int>(ReadIniFloat(a_section, a_key, static_cast<float>(a_default)));
	}

	int ReadMcmInt(const std::string& a_section, const std::string& a_key, int a_default)
	{
		return static_cast<int>(ReadMcmFloat(a_section, a_key, static_cast<float>(a_default)));
	}

	float ManualAimReturnDelayForOption(int a_option)
	{
		switch (a_option) {
		case 0:
			return 0.0F;
		case 2:
			return 0.35F;
		default:
			return 0.18F;
		}
	}

	std::string TrimCopy(std::string a_text)
	{
		const auto first = a_text.find_first_not_of(" \t\r\n");
		if (first == std::string::npos) {
			return {};
		}
		const auto last = a_text.find_last_not_of(" \t\r\n");
		return a_text.substr(first, last - first + 1);
	}

	bool WriteIniValueAtPath(const fs::path& a_iniPath, const std::string& a_section, const std::string& a_key, const std::string& a_value)
	{
		std::vector<std::string> lines;
		{
			std::ifstream in(a_iniPath);
			std::string line;
			while (std::getline(in, line)) {
				lines.push_back(line);
			}
		}

		bool inSection = false;
		bool sectionFound = false;
		bool keyWritten = false;
		std::vector<std::string> updated;
		updated.reserve(lines.size() + 3);

		for (const auto& line : lines) {
			const auto trimmed = TrimCopy(line);
			if (trimmed.size() >= 3 && trimmed.front() == '[' && trimmed.back() == ']') {
				if (inSection && !keyWritten) {
					updated.push_back(a_key + "=" + a_value);
					keyWritten = true;
				}
				inSection = trimmed.substr(1, trimmed.size() - 2) == a_section;
				sectionFound = sectionFound || inSection;
				updated.push_back(line);
				continue;
			}

			if (inSection) {
				auto valueLine = line;
				const auto comment = valueLine.find_first_of(";#");
				if (comment != std::string::npos) {
					valueLine.erase(comment);
				}
				const auto equal = valueLine.find('=');
				if (equal != std::string::npos && TrimCopy(valueLine.substr(0, equal)) == a_key) {
					updated.push_back(a_key + "=" + a_value);
					keyWritten = true;
					continue;
				}
			}

			updated.push_back(line);
		}

		if (sectionFound && !keyWritten) {
			updated.push_back(a_key + "=" + a_value);
		} else if (!sectionFound) {
			if (!updated.empty() && !updated.back().empty()) {
				updated.emplace_back();
			}
			updated.push_back("[" + a_section + "]");
			updated.push_back(a_key + "=" + a_value);
		}

		std::error_code ec;
		fs::create_directories(a_iniPath.parent_path(), ec);
		std::ofstream out(a_iniPath, std::ios::trunc);
		if (!out) {
			return false;
		}
		for (const auto& line : updated) {
			out << line << '\n';
		}
		return static_cast<bool>(out);
	}

	bool WriteIniValue(const std::string& a_section, const std::string& a_key, const std::string& a_value)
	{
		return WriteIniValueAtPath("Data/F4SE/Plugins/SimpleAimAssist.ini", a_section, a_key, a_value);
	}

	bool WriteMcmValue(const std::string& a_section, const std::string& a_key, const std::string& a_value)
	{
		return WriteIniValueAtPath("Data/MCM/Settings/SimpleAimAssist.ini", a_section, a_key, a_value);
	}

	void LoadConfig()
	{
		if (!g_inputModeInitialized) {
			g_inputMode = InputModeFromInt(ReadIniInt("InputMode", "iInputMode", 1));
			g_inputModeInitialized = true;
		}
		const bool hasMcmSettings = fs::exists("Data/MCM/Settings/SimpleAimAssist.ini");
		if (hasMcmSettings) {
			g_userEnabled = ReadMcmFloat("Main", "bEnabled", g_userEnabled ? 1.0F : 0.0F) != 0.0F;
			g_inputMode = InputModeFromInt(ReadMcmInt("Main", "iInputMode", InputModeToInt(g_inputMode)));
			g_inputModeInitialized = true;
		}

		g_config.enabled = ReadIniBool("General", "bEnableAimAssist", g_config.enabled);
		g_config.logging = ReadIniBool("General", "bEnableLogging", g_config.logging);
		g_config.autoTriggerOnZoom = ReadIniBool("General", "bAutoTriggerOnZoom", g_config.autoTriggerOnZoom);
		g_config.aimActivationMode = ReadIniBool("General", "bToggleAimMode", false) ?
			AimActivationMode::kToggleLock : AimActivationMode::kFollowAimState;
		g_config.gamepadLongPressAction = static_cast<GamepadLongPressAction>(std::clamp(
			ReadIniInt("General", "iGamepadLongPressAction", 0),
			0,
			1));
		g_config.disableOnHighZoom = ReadIniBool("Zoom", "bDisableOnHighZoom", g_config.disableOnHighZoom);
		g_config.targetMarkerEnabled = ReadIniBool("Feedback", "bEnableTargetMarker", g_config.targetMarkerEnabled);
		g_config.targetMarkerStyle = std::clamp(ReadIniInt("Feedback", "iTargetMarkerStyle", g_config.targetMarkerStyle), 1, 6);
		g_mcmTargetMarkerStyle = g_config.targetMarkerStyle - 1;
		g_config.highZoomRatio = Clamp(ReadIniFloat("Zoom", "fHighZoomRatio", g_config.highZoomRatio), 1.05F, 8.0F);
		g_config.searchConeDegrees = Clamp(ReadIniFloat("Targeting", "fSearchConeDegrees", g_config.searchConeDegrees), 3.0F, 45.0F);
		g_config.lockRetentionDistanceScale = Clamp(ReadIniFloat("Targeting", "fLockRetentionDistanceScale", g_config.lockRetentionDistanceScale), 1.0F, 2.0F);
		g_config.minLockDistance = Clamp(ReadIniFloat("Targeting", "fMinLockDistance", g_config.minLockDistance), 0.0F, 2000.0F);
		g_config.maxLockDistance = Clamp(ReadIniFloat("Targeting", "fMaxLockDistance", g_config.maxLockDistance), 500.0F, 10000.0F);
		g_config.meleeMaxLockDistance = Clamp(ReadIniFloat("Targeting", "fMeleeLockDistance", g_config.meleeMaxLockDistance), 100.0F, 3000.0F);
		g_config.aimAnchor = static_cast<AimAnchor>(std::clamp(
			ReadIniInt("Targeting", "iAimAnchor", static_cast<int>(g_config.aimAnchor)),
			0,
			2));
		g_mcmRangedLockDistance = g_config.maxLockDistance;
		g_mcmMeleeLockDistance = g_config.meleeMaxLockDistance;
		g_mcmAimAnchor = static_cast<int>(g_config.aimAnchor);
		g_config.retargetDelay = Clamp(ReadIniFloat("Targeting", "fRetargetDelay", g_config.retargetDelay), 0.0F, 1.0F);
		g_config.noFireTimeoutSeconds = Clamp(ReadIniFloat("Assist", "fNoFireTimeoutSeconds", g_config.noFireTimeoutSeconds), 0.0F, 15.0F);
		g_config.onlyCombatTargets = ReadIniBool("Targeting", "bOnlyCombatTargets", g_config.onlyCombatTargets);
		// Preserve legacy INI behavior until the new MCM switch is written.
		g_config.allowOutOfCombatHostileTargets = ReadIniBool(
			"Targeting",
			"bAllowOutOfCombatHostileTargets",
			!g_config.onlyCombatTargets);
		g_config.noFireTimeoutAffectsIndependentLock = ReadIniBool(
			"Assist",
			"bNoFireTimeoutAffectsIndependentLock",
			g_config.noFireTimeoutAffectsIndependentLock);
		g_config.useRaceAimPoints = ReadIniBool("Targeting", "bUseRaceAimPoints", g_config.useRaceAimPoints);
		g_config.continueCrosshairTracking = ReadIniBool(
			"Assist",
			"bEnableCrosshairTracking",
			ReadIniBool("Assist", "bContinueWhileHoldingZoom", g_config.continueCrosshairTracking));
		g_config.assistDuration = Clamp(ReadIniFloat("Assist", "fAssistDuration", g_config.assistDuration), 0.05F, 5.0F);
		g_config.assistStrength = Clamp(ReadIniFloat("Assist", "fAssistStrength", g_config.assistStrength), 0.05F, 1.0F);
		g_config.maxAssistSpeed = Clamp(ReadIniFloat("Assist", "fMaxAssistSpeed", g_config.maxAssistSpeed), 1.0F, 360.0F);
		g_config.assistSmoothing = Clamp(ReadIniFloat("Assist", "fAssistSmoothing", g_config.assistSmoothing), 0.02F, 0.50F);
		g_config.assistDeadZoneDegrees = Clamp(ReadIniFloat("Assist", "fAssistDeadZoneDegrees", g_config.assistDeadZoneDegrees), 0.0F, 5.0F);
		g_config.horizontalOffsetDegrees = Clamp(ReadIniFloat("Assist", "fHorizontalOffsetDegrees", g_config.horizontalOffsetDegrees), -5.0F, 5.0F);
		g_config.torsoHeight = Clamp(ReadIniFloat("Assist", "fTorsoHeight", g_config.torsoHeight), 20.0F, 180.0F);
		g_config.targetSpeedSoftLimit = Clamp(ReadIniFloat("Assist", "fTargetSpeedSoftLimit", g_config.targetSpeedSoftLimit), 100.0F, 5000.0F);
		g_config.targetSpeedHardLimit = Clamp(ReadIniFloat("Assist", "fTargetSpeedHardLimit", g_config.targetSpeedHardLimit), g_config.targetSpeedSoftLimit + 1.0F, 8000.0F);
		g_config.stickyAssistSeconds = Clamp(ReadIniFloat("Assist", "fStickyAssistSeconds", g_config.stickyAssistSeconds), 0.0F, 3.0F);
		g_config.stickyStrengthBonus = Clamp(ReadIniFloat("Assist", "fStickyStrengthBonus", g_config.stickyStrengthBonus), 0.0F, 1.0F);
		g_config.stickySpeedSoftLimit = Clamp(ReadIniFloat("Assist", "fStickySpeedSoftLimit", g_config.stickySpeedSoftLimit), 100.0F, 8000.0F);
		g_config.stickySpeedHardLimit = Clamp(ReadIniFloat("Assist", "fStickySpeedHardLimit", g_config.stickySpeedHardLimit), g_config.stickySpeedSoftLimit + 1.0F, 12000.0F);
		g_config.closeRangeBoostDistance = Clamp(ReadIniFloat("Assist", "fCloseRangeBoostDistance", g_config.closeRangeBoostDistance), 0.0F, 3000.0F);
		g_config.farAssistFalloffStart = Clamp(ReadIniFloat("Assist", "fFarAssistFalloffStart", g_config.farAssistFalloffStart), 500.0F, 8000.0F);
		g_config.farAssistMinScale = Clamp(ReadIniFloat("Assist", "fFarAssistMinScale", g_config.farAssistMinScale), 0.20F, 1.0F);
		g_config.lockStickDegrees = Clamp(ReadIniFloat("Assist", "fLockStickDegrees", g_config.lockStickDegrees), 0.0F, 5.0F);
		g_config.lockStickStrength = Clamp(ReadIniFloat("Assist", "fLockStickStrength", g_config.lockStickStrength), 0.0F, 0.50F);
		g_config.lockStickMaxSpeed = Clamp(ReadIniFloat("Assist", "fLockStickMaxSpeed", g_config.lockStickMaxSpeed), 1.0F, 180.0F);
		g_config.manualAimReturnDelay = Clamp(ReadIniFloat("ManualAim", "fReturnDelay", g_config.manualAimReturnDelay), 0.0F, 0.50F);
		g_config.manualAimGamepadOffsetRetention = Clamp(ReadIniFloat("ManualAim", "fGamepadOffsetRetention", g_config.manualAimGamepadOffsetRetention), 0.25F, 1.0F);
		g_config.manualAimKeyboardMouseOffsetRetention = Clamp(ReadIniFloat("ManualAim", "fKeyboardMouseOffsetRetention", g_config.manualAimKeyboardMouseOffsetRetention), 0.25F, 1.0F);
		g_mcmTargetSwitchLevel = std::clamp(ReadIniInt("ManualAim", "iTargetSwitchLevel", g_mcmTargetSwitchLevel), 0, 4);
		g_mcmAssistStrengthScale = Clamp(ReadIniFloat("Assist", "fAssistStrengthScale", g_mcmAssistStrengthScale), 0.10F, 1.00F);
		if (hasMcmSettings) {
			g_mcmAssistStrengthScale = Clamp(ReadMcmFloat("Main", "fAssistStrengthScale", g_mcmAssistStrengthScale), 0.10F, 1.00F);
			const int savedReturnDelay = ReadMcmInt("Main", "iManualAimReturnDelay", -1);
			const int returnDelaySchema = ReadMcmInt("Main", "iManualAimReturnDelaySchema", -1);
			if (returnDelaySchema < 2) {
				// In the previous build, option 2 meant 180 ms. Migrate that
				// old value to the new Short preset exactly once.
				g_mcmManualAimReturnDelay = savedReturnDelay == 0 ? 0 : 1;
				const bool delayWritten = WriteMcmValue(
					"Main",
					"iManualAimReturnDelay",
					std::to_string(g_mcmManualAimReturnDelay));
				const bool schemaWritten = WriteMcmValue("Main", "iManualAimReturnDelaySchema", "2");
				if (!delayWritten || !schemaWritten) {
					Log("Failed to migrate MCM manual aim return delay; using short delay in memory");
				}
			} else {
				g_mcmManualAimReturnDelay = static_cast<int>(Clamp(
					static_cast<float>(savedReturnDelay < 0 ? 1 : savedReturnDelay),
					0.0F,
					2.0F));
			}
			g_config.manualAimReturnDelay = ManualAimReturnDelayForOption(g_mcmManualAimReturnDelay);
			g_mcmTargetSwitchLevel = std::clamp(
				ReadMcmInt("Main", "iTargetSwitchLevel", g_mcmTargetSwitchLevel),
				0,
				4);
		}
		g_config.targetSwitchLevel = g_mcmTargetSwitchLevel;
		g_config.assistStrengthScale = g_mcmAssistStrengthScale;
		g_config.targetFrictionStrength = Clamp(ReadIniFloat("Assist", "fTargetFrictionStrength", g_mcmTargetFrictionStrength), 0.0F, 1.0F);
		g_config.lockYawResponse = Clamp(ReadIniFloat("LockOn", "fYawResponse", g_config.lockYawResponse), 1.0F, 30.0F);
		g_config.lockPitchResponse = Clamp(ReadIniFloat("LockOn", "fPitchResponse", g_config.lockPitchResponse), 1.0F, 30.0F);
		g_config.lockMaxYawSpeed = Clamp(ReadIniFloat("LockOn", "fMaxYawSpeed", g_config.lockMaxYawSpeed), 30.0F, 720.0F);
		g_config.lockMaxPitchSpeed = Clamp(ReadIniFloat("LockOn", "fMaxPitchSpeed", g_config.lockMaxPitchSpeed), 30.0F, 720.0F);
		g_config.lockDeadZoneDegrees = Clamp(ReadIniFloat("LockOn", "fDeadZoneDegrees", g_config.lockDeadZoneDegrees), 0.0F, 1.0F);
		g_config.targetLeadSeconds = Clamp(ReadIniFloat("LockOn", "fTargetLeadSeconds", g_config.targetLeadSeconds), 0.0F, 0.20F);
		g_config.targetLeadMaxDistance = Clamp(ReadIniFloat("LockOn", "fTargetLeadMaxDistance", g_config.targetLeadMaxDistance), 0.0F, 200.0F);
		g_config.requireLineOfSight = ReadIniBool("Targeting", "bRequireLineOfSight", g_config.requireLineOfSight);
		g_config.visibleHitFraction = Clamp(ReadIniFloat("Targeting", "fVisibleHitFraction", g_config.visibleHitFraction), 0.25F, 0.99F);
		g_config.lineOfSightCheckInterval = Clamp(ReadIniFloat("Targeting", "fLineOfSightCheckInterval", g_config.lineOfSightCheckInterval), 0.15F, 0.75F);
		g_config.lineOfSightGracePeriod = Clamp(ReadIniFloat("Targeting", "fLineOfSightGracePeriod", g_config.lineOfSightGracePeriod), 0.15F, 1.0F);
		g_config.focusEnabled = ReadIniBool("Focus", "bEnableFocusMode", g_config.focusEnabled);
		g_config.focusTimeScale = Clamp(ReadIniFloat("Focus", "fFocusTimeScale", g_config.focusTimeScale), 0.30F, 1.0F);
		g_config.focusAPPerSecond = Clamp(ReadIniFloat("Focus", "fFocusAPPerSecond", g_config.focusAPPerSecond), 1.0F, 100.0F);
		g_config.focusMinAP = Clamp(ReadIniFloat("Focus", "fFocusMinAP", g_config.focusMinAP), 0.0F, 50.0F);
		if (ActiveProfileDevice() == InputDevice::kGamepad) {
			g_config.searchConeDegrees = Clamp(ReadIniFloat("Gamepad", "fSearchConeDegrees", 16.0F), 3.0F, 45.0F);
			g_config.lockYawResponse = Clamp(ReadIniFloat("Gamepad", "fLockYawResponse", 10.0F), 1.0F, 30.0F);
			g_config.lockPitchResponse = Clamp(ReadIniFloat("Gamepad", "fLockPitchResponse", 8.0F), 1.0F, 30.0F);
			g_config.lockMaxYawSpeed = Clamp(ReadIniFloat("Gamepad", "fLockMaxYawSpeed", 300.0F), 30.0F, 720.0F);
			g_config.lockMaxPitchSpeed = Clamp(ReadIniFloat("Gamepad", "fLockMaxPitchSpeed", 220.0F), 30.0F, 720.0F);
			g_config.assistStrength = Clamp(ReadIniFloat("Gamepad", "fAssistStrength", 0.32F), 0.05F, 1.0F);
			g_config.maxAssistSpeed = Clamp(ReadIniFloat("Gamepad", "fMaxAssistSpeed", 125.0F), 1.0F, 360.0F);
			g_config.assistSmoothing = Clamp(ReadIniFloat("Gamepad", "fAssistSmoothing", 0.24F), 0.02F, 0.50F);
			g_config.assistDeadZoneDegrees = Clamp(ReadIniFloat("Gamepad", "fAssistDeadZoneDegrees", 0.45F), 0.0F, 5.0F);
			g_config.targetSpeedSoftLimit = Clamp(ReadIniFloat("Gamepad", "fTargetSpeedSoftLimit", 850.0F), 100.0F, 5000.0F);
			g_config.targetSpeedHardLimit = Clamp(ReadIniFloat("Gamepad", "fTargetSpeedHardLimit", 1700.0F), g_config.targetSpeedSoftLimit + 1.0F, 8000.0F);
			g_config.stickyAssistSeconds = Clamp(ReadIniFloat("Gamepad", "fStickyAssistSeconds", 1.20F), 0.0F, 3.0F);
			g_config.stickyStrengthBonus = Clamp(ReadIniFloat("Gamepad", "fStickyStrengthBonus", 0.30F), 0.0F, 1.0F);
			g_config.stickySpeedSoftLimit = Clamp(ReadIniFloat("Gamepad", "fStickySpeedSoftLimit", 1650.0F), 100.0F, 8000.0F);
			g_config.stickySpeedHardLimit = Clamp(ReadIniFloat("Gamepad", "fStickySpeedHardLimit", 3200.0F), g_config.stickySpeedSoftLimit + 1.0F, 12000.0F);
			g_config.closeRangeBoostDistance = Clamp(ReadIniFloat("Gamepad", "fCloseRangeBoostDistance", 950.0F), 0.0F, 3000.0F);
			g_config.farAssistFalloffStart = Clamp(ReadIniFloat("Gamepad", "fFarAssistFalloffStart", 1200.0F), 500.0F, 8000.0F);
			g_config.farAssistMinScale = Clamp(ReadIniFloat("Gamepad", "fFarAssistMinScale", 0.76F), 0.20F, 1.0F);
			g_config.lockStickDegrees = Clamp(ReadIniFloat("Gamepad", "fLockStickDegrees", 2.35F), 0.0F, 5.0F);
			g_config.lockStickStrength = Clamp(ReadIniFloat("Gamepad", "fLockStickStrength", 0.18F), 0.0F, 0.50F);
			g_config.lockStickMaxSpeed = Clamp(ReadIniFloat("Gamepad", "fLockStickMaxSpeed", 58.0F), 1.0F, 180.0F);
		}
		if (hasMcmSettings) {
			g_config.aimActivationMode = ReadMcmFloat(
				"Main",
				"bToggleAimMode",
				g_config.aimActivationMode == AimActivationMode::kToggleLock ? 1.0F : 0.0F) != 0.0F ?
				AimActivationMode::kToggleLock : AimActivationMode::kFollowAimState;
			g_config.gamepadLongPressAction = static_cast<GamepadLongPressAction>(std::clamp(
				ReadMcmInt("Main", "iGamepadLongPressAction", static_cast<int>(g_config.gamepadLongPressAction)),
				0,
				1));
			g_mcmSearchConeDegrees = Clamp(
				ReadMcmFloat("Main", "fSearchConeDegrees", g_mcmSearchConeDegrees),
				3.0F,
				24.0F);
			g_config.searchConeDegrees = g_mcmSearchConeDegrees;
			g_config.noFireTimeoutSeconds = Clamp(
				ReadMcmFloat("Main", "fNoFireTimeoutSeconds", g_config.noFireTimeoutSeconds),
				0.0F,
				15.0F);
			g_config.noFireTimeoutAffectsIndependentLock =
				ReadMcmFloat(
					"Main",
					"bNoFireTimeoutAffectsIndependentLock",
					g_config.noFireTimeoutAffectsIndependentLock ? 1.0F : 0.0F) != 0.0F;
			g_config.allowOutOfCombatHostileTargets =
				ReadMcmFloat(
					"Main",
					"bAllowOutOfCombatHostileTargets",
					g_config.allowOutOfCombatHostileTargets ? 1.0F : 0.0F) != 0.0F;
			g_config.targetMarkerEnabled =
				ReadMcmFloat("Main", "bEnableTargetMarker", g_config.targetMarkerEnabled ? 1.0F : 0.0F) != 0.0F;
			const bool allowHighZoom =
				ReadMcmFloat("Main", "bAllowHighZoom", g_config.disableOnHighZoom ? 0.0F : 1.0F) != 0.0F;
			g_config.disableOnHighZoom = !allowHighZoom;
			g_mcmTargetMarkerStyle = std::clamp(
				ReadMcmInt("Main", "iTargetMarkerStyle", g_mcmTargetMarkerStyle),
				0,
				5);
			g_config.targetMarkerStyle = g_mcmTargetMarkerStyle + 1;
			g_config.continueCrosshairTracking =
				ReadMcmFloat(
					"Main",
					"bEnableCrosshairTracking",
					g_config.continueCrosshairTracking ? 1.0F : 0.0F) != 0.0F;
			g_mcmRangedLockDistance = Clamp(
				ReadMcmFloat("Main", "fRangedLockDistance", g_mcmRangedLockDistance),
				500.0F,
				10000.0F);
			g_mcmMeleeLockDistance = Clamp(
				ReadMcmFloat("Main", "fMeleeLockDistance", g_mcmMeleeLockDistance),
				100.0F,
				3000.0F);
			g_config.maxLockDistance = g_mcmRangedLockDistance;
			g_config.meleeMaxLockDistance = g_mcmMeleeLockDistance;
			g_mcmAimAnchor = std::clamp(
				ReadMcmInt("Main", "iAimAnchor", g_mcmAimAnchor),
				0,
				2);
			g_config.aimAnchor = static_cast<AimAnchor>(g_mcmAimAnchor);
		}
		g_config.assistStrengthScale = g_mcmAssistStrengthScale;
		g_config.targetFrictionStrength = Clamp(g_mcmTargetFrictionStrength, 0.0F, 1.0F);
		g_config.showTargetAcquiredMessage = ReadIniBool("Feedback", "bShowTargetAcquiredMessage", g_config.showTargetAcquiredMessage);
		g_config.showLockHudHint = ReadIniBool("Feedback", "bShowLockHudHint", g_config.showLockHudHint);
		g_config.hudHintCooldown = Clamp(ReadIniFloat("Feedback", "fHudHintCooldown", g_config.hudHintCooldown), 0.2F, 10.0F);
	}

	void ObserveInputDevice(RE::INPUT_DEVICE a_device)
	{
		const auto inputDevice = InputDeviceFromRuntime(a_device);
		if (inputDevice == InputDevice::kUnknown) {
			return;
		}

		if (g_lastInputDevice == inputDevice) {
			g_lastInputDeviceAt = NowSeconds();
			return;
		}

		g_lastInputDevice = inputDevice;
		g_lastInputDeviceAt = NowSeconds();
		if (g_inputMode == InputMode::kAuto) {
			LoadConfig();
		}
		Log(std::format(
			"Last input device changed: device={} mode={} profileDevice={}",
			InputDeviceName(g_lastInputDevice),
			InputModeName(g_inputMode),
			InputDeviceName(ActiveProfileDevice())));
	}

	bool IsChineseMcmConfiguration()
	{
		std::ifstream in("Data/MCM/Config/SimpleAimAssist/config.json", std::ios::binary);
		if (!in) {
			return false;
		}

		std::ostringstream contents;
		contents << in.rdbuf();
		return contents.str().find("简单辅助瞄准") != std::string::npos;
	}

	std::string AimAnchorHudMessage(AimAnchor a_anchor)
	{
		if (IsChineseMcmConfiguration()) {
			return std::string(AimAnchorChineseName(a_anchor));
		}
		return std::format("Aim at {}", AimAnchorName(a_anchor));
	}

	void ShowTargetHudHint(float a_now)
	{
		if (!g_config.showLockHudHint || (a_now - g_lastHudHintTime) < g_config.hudHintCooldown) {
			return;
		}
		g_lastHudHintTime = a_now;
		RE::SendHUDMessage::ShowHUDMessage("Aim Assist", nullptr, true, false);
	}

	bool IsFeatureEnabled()
	{
		return g_config.enabled && g_userEnabled;
	}

	void Log(const std::string& a_msg)
	{
		if (!g_config.logging) {
			return;
		}

		std::lock_guard lock(g_logMutex);
		std::error_code ec;
		fs::create_directories("Data/F4SE/Plugins", ec);
		std::ofstream out("Data/F4SE/Plugins/SimpleAimAssist.log", std::ios::app);
		if (out) {
			out << "[SimpleAimAssist] " << a_msg << '\n';
		}
	}

	bool IsFireEvent(const RE::ButtonEvent* a_event)
	{
		if (!a_event) {
			return false;
		}

		const auto eventName = a_event->QUserEvent();
		if (eventName == "Attack" || eventName == "FireWeapon" ||
			eventName == "LeftAttack" || eventName == "PrimaryAttack" ||
			eventName == "Fire") {
			return true;
		}
		if (a_event->device == RE::INPUT_DEVICE::kMouse) {
			return a_event->QIDCode() == static_cast<std::uint32_t>(RE::BS_BUTTON_CODE::kLeftButton);
		}
		if (a_event->device == RE::INPUT_DEVICE::kGamepad) {
			return a_event->GetBSButtonCode() == RE::BS_BUTTON_CODE::kRTrigger ||
			       a_event->QIDCode() == 10;
		}
		return false;
	}

	bool IsGamepadTargetLockButton(const RE::ButtonEvent* a_event)
	{
		return a_event &&
		       a_event->device == RE::INPUT_DEVICE::kGamepad &&
		       a_event->GetBSButtonCode() == RE::BS_BUTTON_CODE::kRStick;
	}

	bool BeginAssistInternal(float a_now, bool a_auto);
	bool StopAssist(std::monostate);
	void Log(const std::string& a_msg);

	void SetFocusTimeScale(float a_scale)
	{
		if (auto* timer = RE::BSTimer::GetSingleton()) {
			timer->SetGlobalTimeMultiplier(a_scale, true);
		}
	}

	void StopFocusMode(bool a_clearRequest)
	{
		if (g_focusActive) {
			SetFocusTimeScale(1.0F);
			Log("Focus mode stopped");
		}
		g_focusActive = false;
		if (a_clearRequest) {
			g_focusRequested = false;
		}
	}

	bool SpendFocusAP(RE::PlayerCharacter* a_player, float a_delta)
	{
		auto* values = RE::ActorValue::GetSingleton();
		if (!a_player || !values || !values->actionPoints) {
			return false;
		}

		const float cost = g_config.focusAPPerSecond * std::max(a_delta, 0.0F);
		const float ap = a_player->GetActorValue(*values->actionPoints);
		if (ap <= std::max(g_config.focusMinAP, cost)) {
			return false;
		}

		a_player->RestoreActorValue(*values->actionPoints, -cost);
		return true;
	}

	void UpdateFocusMode(float a_delta)
	{
		if (!g_config.focusEnabled || !g_focusRequested || !g_zoomHeld || !IsFeatureEnabled() || !g_config.autoTriggerOnZoom) {
			StopFocusMode(!g_zoomHeld);
			return;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || player->IsDead(false) || !SpendFocusAP(player, a_delta)) {
			StopFocusMode(true);
			return;
		}

		if (!g_focusActive) {
			SetFocusTimeScale(g_config.focusTimeScale);
			g_focusActive = true;
			Log("Focus mode started");
		}
	}

	void StartZoomAssist(float a_now)
	{
		g_zoomHeld = true;
		g_lastFrameTime = a_now;
		BeginAssistInternal(a_now, true);
		g_runtime.normalAssistSourceLatched = !g_toggleLockActive;
	}

	void StopZoomAssist()
	{
		g_zoomHeld = false;
		StopFocusMode(true);
		StopAssist(std::monostate{});
	}

	void LogInputEventOnce(const RE::ButtonEvent* a_event)
	{
		if (!a_event || (!a_event->QJustPressed() && !a_event->QReleased())) {
			return;
		}

		const auto eventName = a_event->QUserEvent();
		std::ostringstream keyStream;
		keyStream << (eventName.empty() ? "<empty>" : eventName.c_str())
			      << ":device=" << std::to_underlying(a_event->device.get())
			      << ":id=" << a_event->QIDCode()
			      << ":bs=" << std::to_underlying(a_event->GetBSButtonCode())
			      << (a_event->QJustPressed() ? ":pressed" : ":released");
		const auto key = keyStream.str();
		if (g_loggedInputEvents.size() >= 80 || !g_loggedInputEvents.insert(key).second) {
			return;
		}

		std::ostringstream ss;
		ss << "Input event " << (eventName.empty() ? "<empty>" : eventName.c_str())
		   << " device=" << std::to_underlying(a_event->device.get())
		   << " id=" << a_event->QIDCode()
		   << " bs=" << std::to_underlying(a_event->GetBSButtonCode())
		   << (a_event->QJustPressed() ? " pressed" : " released");
		Log(ss.str());
	}

	void LogThumbstickEventOnce(const RE::ThumbstickEvent* a_event)
	{
		if (!a_event || a_event->device != RE::INPUT_DEVICE::kGamepad) {
			return;
		}

		std::ostringstream keyStream;
		keyStream << "thumbstick:device=" << std::to_underlying(a_event->device.get())
			      << ":id=" << a_event->QIDCode();
		const auto key = keyStream.str();
		if (g_loggedInputEvents.size() >= 80 || !g_loggedInputEvents.insert(key).second) {
			return;
		}

		std::ostringstream ss;
		ss << "Thumbstick event device=" << std::to_underlying(a_event->device.get())
		   << " id=" << a_event->QIDCode()
		   << " x=" << a_event->xValue
		   << " y=" << a_event->yValue;
		Log(ss.str());
	}

	float EffectiveFOV()
	{
		auto* camera = RE::PlayerCamera::GetSingleton();
		if (!camera) {
			return 80.0F;
		}

		const float base = camera->worldFOV > 1.0F ? camera->worldFOV : camera->firstPersonFOV;
		const float adjusted = base + camera->fovAdjustCurrent + camera->fovAnimatorAdjust;
		return Clamp(adjusted, 5.0F, 160.0F);
	}

	float CurrentZoomRatio()
	{
		auto* camera = RE::PlayerCamera::GetSingleton();
		if (!camera) {
			return 1.0F;
		}

		const float base = camera->worldFOV > 1.0F ? camera->worldFOV : g_runtime.baseFOV;
		const float current = EffectiveFOV();
		return Clamp(base / std::max(current, 1.0F), 1.0F, 12.0F);
	}

	bool IsAimCameraStateActive()
	{
		auto* camera = RE::PlayerCamera::GetSingleton();
		if (!camera || !camera->currentState) {
			return false;
		}
		if (camera->currentState->id == RE::CameraStates::kIronSights) {
			return true;
		}
		if (camera->currentState->id == RE::CameraStates::k3rdPerson) {
			return static_cast<RE::ThirdPersonState*>(camera->currentState.get())->ironSights;
		}
		return false;
	}

	WeaponRangeClass ClassifyWeaponRange(const RE::TESObjectWEAP* a_weapon)
	{
		if (!a_weapon) {
			return WeaponRangeClass::kUnknown;
		}

		// WEAPON_TYPE is a numeric enum, not a bit mask. In particular, using
		// .any(kHandToHand) makes kGun (9) overlap kHandToHand (1).
		switch (a_weapon->weaponData.type.get()) {
		case RE::WEAPON_TYPE::kGun:
		case RE::WEAPON_TYPE::kBow:
		case RE::WEAPON_TYPE::kStaff:
		case RE::WEAPON_TYPE::kGrenade:
		case RE::WEAPON_TYPE::kMine:
			return WeaponRangeClass::kRanged;
		case RE::WEAPON_TYPE::kHandToHand:
		case RE::WEAPON_TYPE::kOneHandSword:
		case RE::WEAPON_TYPE::kOneHandDagger:
		case RE::WEAPON_TYPE::kOneHandAxe:
		case RE::WEAPON_TYPE::kOneHandMace:
		case RE::WEAPON_TYPE::kTwoHandSword:
		case RE::WEAPON_TYPE::kTwoHandAxe:
			return WeaponRangeClass::kMelee;
		default:
			break;
		}

		if (a_weapon->IsRangedWeapon()) {
			return WeaponRangeClass::kRanged;
		}
		if (a_weapon->IsMeleeWeapon()) {
			return WeaponRangeClass::kMelee;
		}
		return WeaponRangeClass::kUnknown;
	}

	WeaponRangeClass GetPlayerWeaponRangeClass(RE::Actor* a_player)
	{
		const float now = NowSeconds();
		if (now < g_nextWeaponRangeRefreshTime) {
			return g_cachedWeaponRangeClass;
		}
		g_nextWeaponRangeRefreshTime = now + 0.05F;

		if (!a_player || !a_player->inventoryList) {
			g_cachedWeaponRangeClass = WeaponRangeClass::kUnknown;
			g_cachedWeaponFormID = 0;
			return WeaponRangeClass::kUnknown;
		}

		const RE::TESObjectWEAP* selectedWeapon = nullptr;
		int selectedPriority = -1;
		std::uint32_t equippedWeaponCount = 0;

		// Use the same source of truth as Aozora Favorites: the inventory stack's
		// equipped flags. This stays correct when the active weapon changes while
		// the older weapon remains in the inventory array.
		for (auto& item : a_player->inventoryList->data) {
			auto* weapon = item.object ? item.object->As<RE::TESObjectWEAP>() : nullptr;
			if (!weapon) {
				continue;
			}

			for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
				if (!stack->IsEquipped()) {
					continue;
				}

				++equippedWeaponCount;
				const auto weaponType = weapon->weaponData.type.get();
				const bool throwable =
					weaponType == RE::WEAPON_TYPE::kGrenade ||
					weaponType == RE::WEAPON_TYPE::kMine;
				const bool primaryEquipSlot =
					stack->flags.any(RE::BGSInventoryItem::Stack::Flag::kSlotIndex1);
				const int priority =
					(throwable ? 0 : 100) +
					(primaryEquipSlot ? 20 : 0);
				if (priority > selectedPriority) {
					selectedWeapon = weapon;
					selectedPriority = priority;
				}
				break;
			}
		}

		const auto selectedClass = ClassifyWeaponRange(selectedWeapon);
		const auto selectedFormID = selectedWeapon ? selectedWeapon->formID : 0;
		if (selectedClass != g_cachedWeaponRangeClass ||
			selectedFormID != g_cachedWeaponFormID) {
			Log(std::format(
				"Equipped weapon inventory scan changed: form={:08X} type={} "
				"weaponClass={} equippedWeaponCount={}",
				selectedFormID,
				selectedWeapon ?
					static_cast<int>(selectedWeapon->weaponData.type.get()) :
					-1,
				static_cast<int>(selectedClass),
				equippedWeaponCount));
		}
		g_cachedWeaponRangeClass = selectedClass;
		g_cachedWeaponFormID = selectedFormID;
		return selectedClass;
	}

	bool IsPlayerUsingMeleeWeapon(RE::Actor* a_player)
	{
		return GetPlayerWeaponRangeClass(a_player) == WeaponRangeClass::kMelee;
	}

	MeleeGuardSignals QueryMeleeGuardSignals(RE::PlayerCharacter* a_player)
	{
		MeleeGuardSignals signals;
		if (!a_player) {
			return signals;
		}

		signals.wantBlocking = a_player->wantBlocking;
		signals.blockedGunState = a_player->gunState == RE::GUN_STATE::kBlocked;

		// These are animation-graph state variables, not raw button input. Some
		// melee animation graphs expose guard state here without updating ActorState.
		static const RE::BSFixedString isBlockingVariable{ "IsBlocking" };
		static const RE::BSFixedString wantBlockVariable{ "iWantBlock" };
		signals.hasGraphIsBlocking =
			a_player->GetGraphVariableImplBool(isBlockingVariable, signals.graphIsBlocking);
		signals.hasGraphWantBlock =
			a_player->GetGraphVariableImplInt(wantBlockVariable, signals.graphWantBlock);
		signals.active =
			signals.wantBlocking ||
			signals.blockedGunState ||
			(signals.hasGraphIsBlocking && signals.graphIsBlocking) ||
			(signals.hasGraphWantBlock && signals.graphWantBlock != 0);
		return signals;
	}

	float MaxLockDistanceForActor(RE::Actor* a_player)
	{
		return IsPlayerUsingMeleeWeapon(a_player) ? g_config.meleeMaxLockDistance : g_config.maxLockDistance;
	}

	bool IsActualAimStateActive()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return false;
		}
		if (IsAimCameraStateActive()) {
			return true;
		}
		if (player->gunState == RE::GUN_STATE::kSighted ||
		    player->gunState == RE::GUN_STATE::kFireSighted) {
			return true;
		}

		// Fallout 4 can switch a continuously-fired weapon from kFireSighted to
		// kFire, or temporarily expose no useful aim state, while the player is
		// still aiming. Accept that transition only after the current assist
		// session has already entered and the fire input is held; this keeps
		// sustained-fire weapons locked without enabling assist for hip-fire.
		if (g_zoomHeld && (player->gunState == RE::GUN_STATE::kFire || IsFireInputHeld())) {
			return true;
		}

		return false;
	}

	RE::NiPoint3 CameraPosition(RE::Actor* a_player)
	{
		RE::NiPoint3 position{};
		if (auto* camera = RE::PlayerCamera::GetSingleton(); camera && camera->GetCameraPosition(position, true)) {
			return position;
		}
		if (a_player) {
			position = a_player->GetPosition();
			position.z += 120.0F;
		}
		return position;
	}

	RE::NiPoint3 ForwardFromAngles(const RE::NiPoint3& a_angle)
	{
		const float pitch = a_angle.x;
		const float yaw = a_angle.z;
		const float cp = std::cos(pitch);
		return RE::NiPoint3(std::sin(yaw) * cp, std::cos(yaw) * cp, -std::sin(pitch));
	}

	RE::NiPoint3 CameraForward(RE::Actor* a_player)
	{
		auto* camera = RE::PlayerCamera::GetSingleton();
		if (!camera || !camera->currentState) {
			return a_player ? ForwardFromAngles(a_player->data.angle) : RE::NiPoint3(0.0F, 1.0F, 0.0F);
		}

		RE::NiQuaternion rotation{};
		camera->currentState->GetRotation(rotation);
		const RE::NiPoint3 q(rotation.x, rotation.y, rotation.z);
		const RE::NiPoint3 forward(0.0F, 1.0F, 0.0F);
		const RE::NiPoint3 firstCross(
			(q.y * forward.z) - (q.z * forward.y),
			(q.z * forward.x) - (q.x * forward.z),
			(q.x * forward.y) - (q.y * forward.x));
		const RE::NiPoint3 secondCross(
			(q.y * firstCross.z) - (q.z * firstCross.y),
			(q.z * firstCross.x) - (q.x * firstCross.z),
			(q.x * firstCross.y) - (q.y * firstCross.x));
		auto result = forward + (firstCross * (2.0F * rotation.w)) + (secondCross * 2.0F);
		if (result.Unitize() <= 0.001F) {
			return a_player ? ForwardFromAngles(a_player->data.angle) : forward;
		}
		return result;
	}

	std::string LowerCopy(std::string_view a_text)
	{
		std::string lowered(a_text);
		std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
			return static_cast<char>(std::tolower(ch));
		});
		return lowered;
	}

	bool ContainsAny(std::string_view a_text, std::initializer_list<std::string_view> a_needles)
	{
		for (const auto needle : a_needles) {
			if (!needle.empty() && a_text.find(needle) != std::string_view::npos) {
				return true;
			}
		}
		return false;
	}

	std::string RaceEditorIDLower(RE::Actor* a_actor)
	{
		if (!a_actor || !a_actor->race) {
			return {};
		}

		const auto editorID = a_actor->race->formEditorID.c_str();
		return editorID ? LowerCopy(editorID) : std::string{};
	}

	float TargetHeightForActor(RE::Actor* a_actor)
	{
		if (!g_config.useRaceAimPoints) {
			return g_config.torsoHeight;
		}

		const auto race = RaceEditorIDLower(a_actor);
		if (race.empty()) {
			return g_config.torsoHeight;
		}

		if (ContainsAny(race, { "mirelurkqueen", "queen" })) {
			return 150.0F;
		}
		if (ContainsAny(race, { "behemoth", "deathclaw" })) {
			return 135.0F;
		}
		if (ContainsAny(race, { "supermutant", "super mutant" })) {
			return 112.0F;
		}
		if (ContainsAny(race, { "human", "raider", "gunner", "ghoul", "synth", "child" })) {
			return 92.0F;
		}
		if (ContainsAny(race, { "assaultron", "protectron" })) {
			return 90.0F;
		}
		if (ContainsAny(race, { "sentrybot", "sentry bot" })) {
			return 105.0F;
		}
		if (ContainsAny(race, { "misterhandy", "mrhandy", "mr handy", "gutsy", "eyebot" })) {
			return 58.0F;
		}
		if (ContainsAny(race, { "yaoguai", "yao guai" })) {
			return 78.0F;
		}
		if (ContainsAny(race, { "brahmin", "radstag" })) {
			return 70.0F;
		}
		if (ContainsAny(race, { "dog", "mongrel", "wolf", "hound" })) {
			return 48.0F;
		}
		if (ContainsAny(race, { "mirelurk", "radscorpion" })) {
			return 48.0F;
		}
		if (ContainsAny(race, { "molerat", "radroach" })) {
			return 30.0F;
		}
		if (ContainsAny(race, { "bloodbug", "bloatfly", "stingwing" })) {
			return 36.0F;
		}

		return g_config.torsoHeight;
	}

	RE::NiPoint3 TorsoTargetPoint(RE::Actor* a_actor)
	{
		RE::NiPoint3 point{};
		if (a_actor) {
			if (g_config.useRaceAimPoints && a_actor->race && a_actor->race->bodyPartData) {
				auto* torso = a_actor->race->bodyPartData->partArray[RE::BGSBodyPartData::Torso];
				auto* root = a_actor->Get3D();
				if (torso && root && !torso->targetName.empty()) {
					if (auto* torsoNode = RE::BSUtilities::GetObjectByName(root, torso->targetName, true, false)) {
						return torsoNode->GetWorldTranslate();
					}
				}
			}
			point = a_actor->GetPosition();
			point.z += Clamp(TargetHeightForActor(a_actor), 20.0F, 180.0F);
		}
		return point;
	}

	bool TryGetBodyPartPoint(
		RE::Actor* a_actor,
		std::initializer_list<RE::BGSBodyPartData::PartType> a_partTypes,
		RE::NiPoint3& a_point)
	{
		if (!a_actor || !g_config.useRaceAimPoints || !a_actor->race ||
			!a_actor->race->bodyPartData) {
			return false;
		}

		auto* root = a_actor->Get3D();
		if (!root) {
			return false;
		}

		for (const auto partType : a_partTypes) {
			auto* part = a_actor->race->bodyPartData->partArray[partType];
			if (!part || part->targetName.empty()) {
				continue;
			}
			if (auto* node = RE::BSUtilities::GetObjectByName(root, part->targetName, true, false)) {
				a_point = node->GetWorldTranslate();
				return true;
			}
		}
		return false;
	}

	RE::NiPoint3 TargetPoint(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return {};
		}

		RE::NiPoint3 point{};
		if (g_config.aimAnchor == AimAnchor::kHead &&
			TryGetBodyPartPoint(
				a_actor,
				{ RE::BGSBodyPartData::Head1,
				  RE::BGSBodyPartData::Head2,
				  RE::BGSBodyPartData::Eye,
				  RE::BGSBodyPartData::LookAt },
				point)) {
			return point;
		}

		if (g_config.aimAnchor == AimAnchor::kLowerBody &&
			TryGetBodyPartPoint(
				a_actor,
				{ RE::BGSBodyPartData::Pelvis,
				  RE::BGSBodyPartData::LeftLeg1,
				  RE::BGSBodyPartData::RightLeg1 },
				point)) {
			return point;
		}

		// Missing head or pelvis nodes are common on creatures and custom races;
		// fall back to the proven torso point instead of losing the lock.
		return TorsoTargetPoint(a_actor);
	}

	RE::NiPoint3 TargetMarkerPoint(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return {};
		}

		// Prefer the animated head node. This keeps the pointer attached to
		// crouching, prone, and cover-peeking actors instead of their torso.
		if (a_actor->race && a_actor->race->bodyPartData) {
			if (auto* root = a_actor->Get3D()) {
				constexpr RE::BGSBodyPartData::PartType headParts[] = {
					RE::BGSBodyPartData::Head1,
					RE::BGSBodyPartData::Head2,
					RE::BGSBodyPartData::Eye,
					RE::BGSBodyPartData::LookAt
				};
				for (const auto partType : headParts) {
					auto* part = a_actor->race->bodyPartData->partArray[partType];
					if (!part || part->targetName.empty()) {
						continue;
					}
					if (auto* headNode = RE::BSUtilities::GetObjectByName(root, part->targetName, true, false)) {
						// Marker placement is independent from the selected aim
						// anchor. Follow the complete animated head transform so
						// crouching, leaning, and cover poses stay attached.
						auto point = headNode->GetWorldTranslate();
						point.z += Clamp(TargetHeightForActor(a_actor) * 0.18F, 12.0F, 32.0F);
						return point;
					}
				}
			}
		}

		// Fallback for creatures without a usable head body-part node.
		RE::NiPoint3 point = a_actor->GetPosition();
		point.z += Clamp(TargetHeightForActor(a_actor) + 25.0F, 45.0F, 240.0F);
		return point;
	}

	bool ProjectTargetMarkerPoint(
		const RE::NiPoint3& a_world,
		float& a_screenX,
		float& a_screenY,
		float& a_depth)
	{
		const auto screen = RE::HUDMenuUtils::WorldPtToScreenPt3(a_world);
		if (!std::isfinite(screen.x) ||
			!std::isfinite(screen.y) ||
			!std::isfinite(screen.z) ||
			screen.z < 0.0F ||
			screen.x < -0.05F ||
			screen.x > 1.05F ||
			screen.y < -0.05F ||
			screen.y > 1.05F) {
			return false;
		}

		a_screenX = Clamp(screen.x, 0.0F, 1.0F);
		a_screenY = Clamp(1.0F - screen.y, 0.0F, 1.0F);
		a_depth = screen.z;
		return true;
	}

	class TargetMarkerHUDMenu final : public RE::GameMenuBase
	{
	public:
		TargetMarkerHUDMenu()
		{
			menuFlags.set(RE::UI_MENU_FLAGS::kAlwaysOpen);
			menuFlags.set(RE::UI_MENU_FLAGS::kRequiresUpdate);
			menuFlags.set(RE::UI_MENU_FLAGS::kAdvancesUnderPauseMenu);
			menuFlags.set(RE::UI_MENU_FLAGS::kRendersUnderPauseMenu);
			menuFlags.set(RE::UI_MENU_FLAGS::kAllowSaving);
			depthPriority = RE::UI_DEPTH_PRIORITY::kHUD;

			auto* scaleform = RE::BSScaleformManager::GetSingleton();
			if (scaleform &&
				scaleform->LoadMovieEx(
					*this,
					kTargetMarkerMoviePath,
					"",
					Scaleform::GFx::Movie::ScaleModeType::kExactFit,
					0.0F)) {
				g_targetMarkerMenuReady = true;
				Log("Nameplates-style target marker HUD menu loaded");
			} else {
				Log("Nameplates-style target marker HUD menu failed to load SWF");
			}
		}

		~TargetMarkerHUDMenu() override
		{
			g_targetMarkerMenuReady = false;
		}

		void AdvanceMovie(float a_timeDelta, std::uint64_t a_time) override
		{
			if (!advanceLogged_) {
				Log("Target marker HUD AdvanceMovie entered");
				advanceLogged_ = true;
			}
			UpdateMarker(a_timeDelta);
			RE::GameMenuBase::AdvanceMovie(a_timeDelta, a_time);
		}

	private:
		bool advanceLogged_{ false };
		bool stageLogged_{ false };
		bool targetLogged_{ false };
		std::uint32_t smoothedTargetFormID_{ 0 };
		bool smoothedPointValid_{ false };
		float smoothedScreenX_{ 0.0F };
		float smoothedScreenY_{ 0.0F };
		std::uint32_t smoothedScaleTargetFormID_{ 0 };
		bool smoothedMarkerScaleValid_{ false };
		float smoothedMarkerScale_{ 1.0F };
		int lastSentMarkerStyle_{ 0 };

		void UpdateMarker(float a_timeDelta)
		{
			if (!uiMovie) {
				return;
			}

			Scaleform::GFx::Value stageWidth;
			Scaleform::GFx::Value stageHeight;
			// Movie::Invoke addresses the AVM root by path. The SWF document
			// class is exposed below root, so the unqualified calls never reach
			// the TargetMarkerNative instance.
			const bool widthCallSucceeded = uiMovie->Invoke("root.GetStageWidth", &stageWidth, nullptr, 0);
			const bool heightCallSucceeded = uiMovie->Invoke("root.GetStageHeight", &stageHeight, nullptr, 0);

			const double width = stageWidth.IsNumber() ? stageWidth.GetNumber() : 0.0;
			const double height = stageHeight.IsNumber() ? stageHeight.GetNumber() : 0.0;
			if (!stageLogged_) {
				Log(std::format(
					"Target marker stage width={} height={} widthCall={} heightCall={}",
					width,
					height,
					widthCallSucceeded,
					heightCallSucceeded));
				stageLogged_ = true;
			}

			const int markerStyle = std::clamp(g_config.targetMarkerStyle, 1, 6);
			if (markerStyle != lastSentMarkerStyle_) {
				Scaleform::GFx::Value styleArg{ static_cast<double>(markerStyle) };
				const bool styleCallSucceeded = uiMovie->Invoke("root.setMarkerStyle", nullptr, &styleArg, 1);
				if (styleCallSucceeded) {
					lastSentMarkerStyle_ = markerStyle;
				} else {
					Log("Target marker SWF setMarkerStyle invoke failed");
				}
			}

			bool visible = false;
			float screenX = 0.0F;
			float screenY = 0.0F;
			float markerScale = 1.0F;

			if (g_config.targetMarkerEnabled &&
				IsFeatureEnabled() &&
				g_runtime.state != AssistState::kIdle) {
				auto target = g_runtime.target.get();
				if (target && !target->IsDead(false) && width > 1.0 && height > 1.0) {
					float depth = 0.0F;
					visible = ProjectTargetMarkerPoint(
						TargetMarkerPoint(target.get()),
						screenX,
						screenY,
						depth);

					if (visible) {
						if (!smoothedPointValid_ ||
							smoothedTargetFormID_ != g_runtime.targetFormID) {
							smoothedTargetFormID_ = g_runtime.targetFormID;
							smoothedScreenX_ = screenX;
							smoothedScreenY_ = screenY;
							smoothedPointValid_ = true;
						} else {
							const float deltaX = screenX - smoothedScreenX_;
							const float deltaY = screenY - smoothedScreenY_;
							const float movement = std::sqrt((deltaX * deltaX) + (deltaY * deltaY));
							if (movement > 0.10F) {
								smoothedScreenX_ = screenX;
								smoothedScreenY_ = screenY;
							} else {
								// Suppress tiny head-animation jitter while
								// following deliberate movement quickly enough
								// to avoid a visible trailing offset.
								const float stableAlpha = Clamp(
									1.0F - std::exp(-Clamp(a_timeDelta, 0.001F, 0.05F) * 10.0F),
									0.10F,
									0.25F);
								const float movementBlend = Clamp(
									(movement - 0.0015F) / 0.012F,
									0.0F,
									1.0F);
								const float alpha =
									stableAlpha + ((0.75F - stableAlpha) * movementBlend);
								smoothedScreenX_ += deltaX * alpha;
								smoothedScreenY_ += deltaY * alpha;
							}
						}
						screenX = smoothedScreenX_;
						screenY = smoothedScreenY_;

						auto* player = RE::PlayerCharacter::GetSingleton();
						const float distance = player ?
							player->GetDistanceFromReference(target.get(), false, true) :
							0.0F;
						const float maxMarkerDistance = std::max(
							MaxLockDistanceForActor(player),
							500.0F);
						const float nearMarkerDistance = std::min(400.0F, maxMarkerDistance * 0.20F);
						const float scaleRange = std::max(maxMarkerDistance - nearMarkerDistance, 1.0F);
						const float distanceRatio =
							Clamp((distance - nearMarkerDistance) / scaleRange, 0.0F, 1.0F);
						const float easedDistance =
							distanceRatio * distanceRatio * (3.0F - (2.0F * distanceRatio));
						// Match the SWF scale limits so distance scaling stays
						// continuous instead of flattening near the player.
						const float desiredMarkerScale = 1.20F - (0.5125F * easedDistance);
						if (!smoothedMarkerScaleValid_ ||
							smoothedScaleTargetFormID_ != g_runtime.targetFormID) {
							smoothedScaleTargetFormID_ = g_runtime.targetFormID;
							smoothedMarkerScale_ = desiredMarkerScale;
							smoothedMarkerScaleValid_ = true;
						} else {
							const float scaleAlpha = Clamp(
								1.0F - std::exp(-Clamp(a_timeDelta, 0.001F, 0.05F) * 10.0F),
								0.10F,
								0.35F);
							smoothedMarkerScale_ +=
								(desiredMarkerScale - smoothedMarkerScale_) * scaleAlpha;
						}
						markerScale = smoothedMarkerScale_;
					}
					if (!targetLogged_) {
						Log(std::format(
							"Target marker target={} ptr={} visible={} screen=({}, {}) depth={}",
							g_runtime.targetFormID,
							static_cast<const void*>(target.get()),
							visible,
							screenX,
							screenY,
							visible ? 1.0F : 0.0F));
						targetLogged_ = true;
					}
				}
			}

			if (!visible) {
				smoothedTargetFormID_ = 0;
				smoothedPointValid_ = false;
				smoothedScaleTargetFormID_ = 0;
				smoothedMarkerScaleValid_ = false;
				smoothedMarkerScale_ = 1.0F;
			}

			Scaleform::GFx::Value args[4]{
				visible ? static_cast<double>(screenX * width) : 0.0,
				visible ? static_cast<double>(screenY * height) : 0.0,
				visible,
				static_cast<double>(markerScale)
			};
			const bool updateCallSucceeded = uiMovie->Invoke("root.updateMarker", nullptr, args, 4);
			if (visible && !updateCallSucceeded) {
				Log("Target marker SWF updateMarker invoke failed");
			}
		}
	};

	RE::IMenu* CreateTargetMarkerHUDMenu(const RE::UIMessage&)
	{
		return new TargetMarkerHUDMenu();
	}

	bool EnsureTargetMarkerHUDMenu()
	{
		auto* ui = RE::UI::GetSingleton();
		if (!ui) {
			return false;
		}

		if (!g_targetMarkerMenuRegistered) {
			ui->RegisterMenu(kTargetMarkerMenuName.data(), CreateTargetMarkerHUDMenu);
			g_targetMarkerMenuRegistered = true;
			Log("Registered Nameplates-style target marker HUD menu");
		}

		if (auto* messageQueue = RE::UIMessageQueue::GetSingleton();
			messageQueue && !ui->GetMenuOpen(RE::BSFixedString(kTargetMarkerMenuName.data()))) {
			messageQueue->AddMessage(
				RE::BSFixedString(kTargetMarkerMenuName.data()),
				RE::UI_MESSAGE_TYPE::kShow);
		}

		return true;
	}

	class TargetMarkerMenuOpenCloseSink final :
		public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(
			const RE::MenuOpenCloseEvent& a_event,
			RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (a_event.menuName != "HUDMenu") {
				return RE::BSEventNotifyControl::kContinue;
			}

			auto* messageQueue = RE::UIMessageQueue::GetSingleton();
			if (!messageQueue) {
				return RE::BSEventNotifyControl::kContinue;
			}

			// Keep the custom HUD menu alive across HUD/cell transitions. Hiding it
			// when HUDMenu closes leaves it permanently invisible after some loads.
			if (a_event.opening) {
				messageQueue->AddMessage(
					RE::BSFixedString(kTargetMarkerMenuName.data()),
					RE::UI_MESSAGE_TYPE::kShow);
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	TargetMarkerMenuOpenCloseSink g_targetMarkerMenuOpenCloseSink;

	void RegisterTargetMarkerMenuSink()
	{
		if (g_targetMarkerMenuSinkRegistered) {
			return;
		}

		if (auto* ui = RE::UI::GetSingleton()) {
			ui->RegisterSink<RE::MenuOpenCloseEvent>(&g_targetMarkerMenuOpenCloseSink);
			g_targetMarkerMenuSinkRegistered = true;
			Log("Registered target marker HUD menu open/close sink");
		}
	}

	float NowSeconds()
	{
		using clock = std::chrono::steady_clock;
		static const auto start = clock::now();
		return std::chrono::duration<float>(clock::now() - start).count();
	}

	void RegisterManualAimInput(float a_magnitude)
	{
		if (!g_zoomHeld || !IsFeatureEnabled() || !g_config.autoTriggerOnZoom || a_magnitude < g_config.manualAimDeadzone) {
			return;
		}

		const float now = NowSeconds();
		const float inputHold = std::max(0.016F, std::min(
			g_config.manualAimInputHold,
			std::max(g_config.manualAimReturnDelay, 0.016F)));
		g_runtime.lastManualAimInputTime = now;
		g_runtime.lastAimActivityTime = now;
		g_runtime.manualAimOverrideUntil = now + inputHold;
		g_runtime.manualAimRecoveryStart = -1.0F;
		g_runtime.manualAimOffsetPending = true;
		g_runtime.manualAimCaptureAfter = now + 0.016F;
	}

	void RegisterFireActivity(const RE::ButtonEvent* a_event, InputDevice a_device)
	{
		if (!a_event) {
			return;
		}
		if (a_event->QReleased()) {
			SetFireInputHeld(a_device, false);
			return;
		}
		if (!IsFeatureEnabled() || !g_config.autoTriggerOnZoom) {
			return;
		}
		if (!a_event->QPressed()) {
			return;
		}

		SetFireInputHeld(a_device, true);
		if (!g_zoomHeld) {
			return;
		}
		g_runtime.lastAimActivityTime = NowSeconds();
	}

	void RequestTargetSwitch(float a_delay = kTargetSwitchRequestDelay)
	{
		if (!g_zoomHeld || !IsFeatureEnabled() || !g_config.autoTriggerOnZoom ||
			g_runtime.state != AssistState::kAssisting || g_config.targetSwitchLevel <= 0 ||
			(!g_config.continueCrosshairTracking && !g_toggleLockActive)) {
			return;
		}

		const float now = NowSeconds();
		if (now < g_runtime.targetSwitchCooldownUntil) {
			return;
		}

		g_runtime.lastAimActivityTime = now;
		g_runtime.targetSwitchRequested = true;
		g_runtime.targetSwitchAt = now + a_delay;
	}

	void RegisterTargetSwitchGesture(float a_magnitude, float a_threshold)
	{
		if (g_config.targetSwitchLevel <= 0) {
			g_runtime.targetSwitchGestureActive = false;
			return;
		}
		if (a_magnitude < (a_threshold * 0.60F)) {
			g_runtime.targetSwitchGestureActive = false;
			return;
		}
		if (a_magnitude >= a_threshold && !g_runtime.targetSwitchGestureActive) {
			g_runtime.targetSwitchGestureActive = true;
			RequestTargetSwitch();
		}
	}

	void RecordTargetRejection(
		TargetSearchDiagnostics* a_diagnostics,
		RE::Actor* a_target,
		std::string_view a_reason,
		float a_distance,
		float a_angle,
		bool a_inCombat,
		bool a_hostile,
		bool a_validTarget)
	{
		if (!a_diagnostics || !a_target ||
			a_distance >= a_diagnostics->closestRejectedDistance) {
			return;
		}

		a_diagnostics->closestRejectedFormID = a_target->GetFormID();
		a_diagnostics->closestRejectedDistance = a_distance;
		a_diagnostics->closestRejectedAngle = a_angle;
		a_diagnostics->closestRejectedInCombat = a_inCombat;
		a_diagnostics->closestRejectedHostile = a_hostile;
		a_diagnostics->closestRejectedValidTarget = a_validTarget;
		a_diagnostics->closestRejectedReason = a_reason;
	}

	bool IsValidTarget(
		RE::Actor* a_player,
		RE::Actor* a_target,
		const RE::NiPoint3& a_cameraPos,
		const RE::NiPoint3& a_forward,
		float& a_angleOut,
		bool a_meleeWeapon,
		float a_maxLockDistance,
		TargetSearchDiagnostics* a_diagnostics)
	{
		if (!a_player || !a_target || a_target == a_player) {
			return false;
		}
		if (a_diagnostics) {
			++a_diagnostics->scanned;
		}

		if (a_target->IsDead(false)) {
			if (a_diagnostics) {
				++a_diagnostics->dead;
			}
			RecordTargetRejection(
				a_diagnostics,
				a_target,
				"dead",
				a_diagnostics ?
					a_player->GetDistanceFromReference(a_target, false, true) :
					-1.0F,
				-1.0F,
				false,
				false,
				false);
			return false;
		}

		const bool inCombat = a_target->IsInCombat();
		if (!g_config.allowOutOfCombatHostileTargets && !inCombat) {
			if (a_diagnostics) {
				++a_diagnostics->notInCombat;
			}
			RecordTargetRejection(
				a_diagnostics,
				a_target,
				"not-in-combat",
				a_diagnostics ?
					a_player->GetDistanceFromReference(a_target, false, true) :
					-1.0F,
				-1.0F,
				false,
				false,
				false);
			return false;
		}
		// Check the candidate's relationship to the player.  Checking the
		// relationship from the player's side is not equivalent here: before
		// combat begins, the player may not yet be hostile to an enemy that is
		// already hostile to the player.  That made the out-of-combat option
		// reject legitimate distant enemies as "not-hostile".
		const bool hostile = a_target->GetHostileToActor(a_player);
		if (!hostile) {
			if (a_diagnostics) {
				++a_diagnostics->notHostile;
			}
			RecordTargetRejection(
				a_diagnostics,
				a_target,
				"not-hostile",
				a_diagnostics ?
					a_player->GetDistanceFromReference(a_target, false, true) :
					-1.0F,
				-1.0F,
				inCombat,
				false,
				false);
			return false;
		}
		const bool validTarget = a_player->CheckValidTarget(*a_target);
		// CheckValidTarget is primarily the game's active-combat target gate.
		// The experimental out-of-combat mode deliberately bypasses that gate;
		// the hostile, distance, cone, and LOS checks below still apply.
		if (!validTarget && !g_config.allowOutOfCombatHostileTargets) {
			if (a_diagnostics) {
				++a_diagnostics->invalidTarget;
			}
			RecordTargetRejection(
				a_diagnostics,
				a_target,
				"game-invalid-target",
				a_diagnostics ?
					a_player->GetDistanceFromReference(a_target, false, true) :
					-1.0F,
				-1.0F,
				inCombat,
				true,
				false);
			return false;
		}

		const float distance = a_player->GetDistanceFromReference(a_target, false, true);
		auto toTarget = TorsoTargetPoint(a_target) - a_cameraPos;
		const float len = toTarget.Unitize();
		const float dot = len > 1.0F ?
			Clamp(a_forward.Dot(toTarget), -1.0F, 1.0F) :
			-1.0F;
		const float angle = dot > 0.0F ?
			std::acos(dot) * kRadToDeg :
			-1.0F;
		if (!a_meleeWeapon && distance < g_config.minLockDistance) {
			if (a_diagnostics) {
				++a_diagnostics->belowMinDistance;
			}
			RecordTargetRejection(
				a_diagnostics,
				a_target,
				"below-min-distance",
				distance,
				angle,
				inCombat,
				true,
				true);
			return false;
		}
		if (distance > a_maxLockDistance) {
			if (a_diagnostics) {
				++a_diagnostics->aboveMaxDistance;
			}
			RecordTargetRejection(
				a_diagnostics,
				a_target,
				"above-max-distance",
				distance,
				angle,
				inCombat,
				true,
				true);
			return false;
		}
		if (len <= 1.0F) {
			if (a_diagnostics) {
				++a_diagnostics->invalidDirection;
			}
			RecordTargetRejection(
				a_diagnostics,
				a_target,
				"invalid-direction",
				distance,
				angle,
				inCombat,
				true,
				true);
			return false;
		}

		if (dot <= 0.0F) {
			if (a_diagnostics) {
				++a_diagnostics->behindCamera;
			}
			RecordTargetRejection(
				a_diagnostics,
				a_target,
				"behind-camera",
				distance,
				angle,
				inCombat,
				true,
				true);
			return false;
		}

		a_angleOut = angle;
		if (a_angleOut > g_config.searchConeDegrees) {
			if (a_diagnostics) {
				++a_diagnostics->outsideCone;
			}
			RecordTargetRejection(
				a_diagnostics,
				a_target,
				"outside-search-cone",
				distance,
				a_angleOut,
				inCombat,
				true,
				true);
			return false;
		}
		return true;
	}

	float TargetAngleFromCamera(RE::Actor* a_player, RE::Actor* a_target)
	{
		if (!a_player || !a_target) {
			return 180.0F;
		}
		auto toTarget = TorsoTargetPoint(a_target) - CameraPosition(a_player);
		if (toTarget.Unitize() <= 1.0F) {
			return 180.0F;
		}
		const auto forward = CameraForward(a_player);
		return std::acos(Clamp(forward.Dot(toTarget), -1.0F, 1.0F)) * kRadToDeg;
	}

	// Use Fallout 4's own player LOS test.  It applies the same visibility rules
	// as the game instead of guessing from a raw physics hit fraction.
	bool HasClearLineOfSight(RE::Actor* a_player, RE::Actor* a_target)
	{
		if (!a_player || !a_target || a_player == a_target) {
			return false;
		}

		auto* playerCharacter = RE::PlayerCharacter::GetSingleton();
		if (!playerCharacter || playerCharacter != a_player) {
			return false;
		}

		bool pickPerformed = false;
		return playerCharacter->HasLOSToTarget(a_target, &pickPerformed);
	}

	void VisitActorArray(RE::BSTArray<RE::ActorHandle>& a_handles, const std::function<void(RE::Actor*)>& a_callback)
	{
		for (auto& handle : a_handles) {
			auto actorPtr = handle.get();
			if (actorPtr) {
				a_callback(actorPtr.get());
			}
		}
	}

	RE::Actor* FindBestTarget(RE::Actor* a_player, RE::Actor* a_excludedTarget = nullptr)
	{
		auto* processLists = RE::ProcessLists::GetSingleton();
		if (!processLists || !a_player) {
			return nullptr;
		}

		const auto cameraPos = CameraPosition(a_player);
		const auto forward = CameraForward(a_player);
		const auto rangeClass = GetPlayerWeaponRangeClass(a_player);
		const bool meleeWeapon = rangeClass == WeaponRangeClass::kMelee;
		const float activeMaxDistance =
			meleeWeapon ? g_config.meleeMaxLockDistance : g_config.maxLockDistance;
		struct Candidate
		{
			RE::Actor* actor;
			float      score;
		};
		std::vector<Candidate> candidates;
		candidates.reserve(16);
		TargetSearchDiagnostics diagnostics;
		const bool collectDiagnostics =
			NowSeconds() >= g_nextTargetSearchDiagnosticTime;

		auto consider = [&](RE::Actor* a_actor) {
			if (a_actor == a_excludedTarget) {
				return;
			}
			float angle = 0.0F;
			if (!IsValidTarget(
					a_player,
					a_actor,
					cameraPos,
					forward,
					angle,
					meleeWeapon,
					activeMaxDistance,
					collectDiagnostics ? &diagnostics : nullptr)) {
				return;
			}

			const float distance = a_player->GetDistanceFromReference(a_actor, false, true);
			const float angleScore = angle / std::max(g_config.searchConeDegrees, 0.1F);
			const float distanceScore = Clamp(
				distance / std::max(activeMaxDistance, 1.0F),
				0.0F,
				1.0F);
			candidates.push_back({ a_actor, (angleScore * 0.82F) + (distanceScore * 0.18F) });
		};

		VisitActorArray(processLists->highActorHandles, consider);
		VisitActorArray(processLists->middleHighActorHandles, consider);
		if (candidates.empty()) {
			const float now = NowSeconds();
			if (now >= g_nextTargetSearchDiagnosticTime) {
				g_nextTargetSearchDiagnosticTime = now + 0.35F;
				Log(std::format(
					"Target search found no eligible candidates: weaponClass={} melee={} "
					"minDistance={} maxDistance={} cone={} scanned={} dead={} notCombat={} "
					"notHostile={} invalidTarget={} belowMin={} aboveMax={} invalidDirection={} "
					"behind={} outsideCone={} closestRejectedForm={:08X} reason={} "
					"distance={} angle={} inCombat={} hostile={} validTarget={}",
					static_cast<int>(rangeClass),
					meleeWeapon,
					meleeWeapon ? 0.0F : g_config.minLockDistance,
					activeMaxDistance,
					g_config.searchConeDegrees,
					diagnostics.scanned,
					diagnostics.dead,
					diagnostics.notInCombat,
					diagnostics.notHostile,
					diagnostics.invalidTarget,
					diagnostics.belowMinDistance,
					diagnostics.aboveMaxDistance,
					diagnostics.invalidDirection,
					diagnostics.behindCamera,
					diagnostics.outsideCone,
					diagnostics.closestRejectedFormID,
					diagnostics.closestRejectedReason,
					diagnostics.closestRejectedDistance == std::numeric_limits<float>::max() ?
						-1.0F :
						diagnostics.closestRejectedDistance,
					diagnostics.closestRejectedAngle,
					diagnostics.closestRejectedInCombat,
					diagnostics.closestRejectedHostile,
					diagnostics.closestRejectedValidTarget));
			}
			return nullptr;
		}

		std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& a_lhs, const Candidate& a_rhs) {
			return a_lhs.score < a_rhs.score;
		});

		// A blocked top candidate should not hide a visible target behind it.
		// Limit physics checks to the best few candidates to keep retargeting cheap.
		constexpr std::size_t kMaxVisibilityCandidates = 8;
		const auto visibilityCount = std::min(kMaxVisibilityCandidates, candidates.size());
		std::size_t visibilityRejects = 0;
		for (std::size_t i = 0; i < visibilityCount; ++i) {
			const auto candidate = candidates[i].actor;
			if (!g_config.requireLineOfSight || HasClearLineOfSight(a_player, candidate)) {
				if (visibilityRejects > 0) {
					Log("Target selection skipped blocked candidates and acquired a visible target");
				}
				return candidate;
			}
			++visibilityRejects;
		}

		if (visibilityRejects > 0) {
			Log("Target selection rejected blocked candidates");
		}
		return nullptr;
	}

	bool IsLockedTargetValid(RE::Actor* a_player, RE::Actor* a_target)
	{
		if (!a_player || !a_target || a_target == a_player || a_target->IsDead(false)) {
			return false;
		}

		const float maxRetentionDistance =
			MaxLockDistanceForActor(a_player) * g_config.lockRetentionDistanceScale;
		return a_player->GetDistanceFromReference(a_target, false, true) <= maxRetentionDistance;
	}

	bool ShouldSwitchToTarget(RE::Actor* a_player, RE::Actor* a_currentTarget, RE::Actor* a_candidate)
	{
		if (!a_player || !a_currentTarget || !a_candidate || a_currentTarget == a_candidate) {
			return false;
		}

		const float currentDistance = a_player->GetDistanceFromReference(a_currentTarget, false, true);
		const float candidateDistance = a_player->GetDistanceFromReference(a_candidate, false, true);
		const float candidateAngle = TargetAngleFromCamera(a_player, a_candidate);
		const float currentAngle = TargetAngleFromCamera(a_player, a_currentTarget);
		const float closeTargetCone = std::max(5.0F, g_config.searchConeDegrees * 0.70F);

		if (currentAngle > (g_config.searchConeDegrees * 1.25F) && candidateAngle <= g_config.searchConeDegrees) {
			return true;
		}
		return candidateAngle <= closeTargetCone && candidateDistance < (currentDistance * 0.70F);
	}

	bool ApplyAssistStep(RE::Actor* a_player, RE::Actor* a_target, float a_delta, float a_assistMultiplier = 1.0F)
	{
		if (!a_player || !a_target || a_delta <= 0.0F) {
			return false;
		}

		const auto cameraPos = CameraPosition(a_player);
		const auto targetPoint = TargetPoint(a_target);
		const float now = NowSeconds();
		if (g_runtime.hasLastTargetPoint) {
			const float sampleDelta = now - g_runtime.lastTargetSampleTime;
			if (sampleDelta > 0.001F && sampleDelta <= 0.20F) {
				RE::NiPoint3 measuredVelocity{
					(targetPoint.x - g_runtime.lastTargetPoint.x) / sampleDelta,
					(targetPoint.y - g_runtime.lastTargetPoint.y) / sampleDelta,
					0.0F
				};
				const float velocityBlend = 1.0F - std::exp(-12.0F * sampleDelta);
				g_runtime.smoothedTargetVelocity.x += (measuredVelocity.x - g_runtime.smoothedTargetVelocity.x) * velocityBlend;
				g_runtime.smoothedTargetVelocity.y += (measuredVelocity.y - g_runtime.smoothedTargetVelocity.y) * velocityBlend;
			}
		} else {
			g_runtime.hasLastTargetPoint = true;
		}
		g_runtime.lastTargetPoint = targetPoint;
		g_runtime.lastTargetSampleTime = now;

		RE::NiPoint3 lead{
			g_runtime.smoothedTargetVelocity.x * g_config.targetLeadSeconds,
			g_runtime.smoothedTargetVelocity.y * g_config.targetLeadSeconds,
			0.0F
		};
		const float leadLength = std::sqrt((lead.x * lead.x) + (lead.y * lead.y));
		if (leadLength > g_config.targetLeadMaxDistance && leadLength > 0.001F) {
			const float scale = g_config.targetLeadMaxDistance / leadLength;
			lead.x *= scale;
			lead.y *= scale;
		}
		g_runtime.targetSpeed = std::sqrt((g_runtime.smoothedTargetVelocity.x * g_runtime.smoothedTargetVelocity.x) +
		                                  (g_runtime.smoothedTargetVelocity.y * g_runtime.smoothedTargetVelocity.y));

		const RE::NiPoint3 predictedTargetPoint{ targetPoint.x + lead.x, targetPoint.y + lead.y, targetPoint.z };
		auto toTarget = predictedTargetPoint - cameraPos;
		g_runtime.targetDistance = PointDistance(targetPoint, cameraPos);
		if (toTarget.Unitize() <= 1.0F) {
			return false;
		}

		float desiredYaw = std::atan2(toTarget.x, toTarget.y) + (g_config.horizontalOffsetDegrees * kDegToRad);
		const float flatLen = std::sqrt((toTarget.x * toTarget.x) + (toTarget.y * toTarget.y));
		float desiredPitch = -std::atan2(toTarget.z, flatLen);

		if (g_runtime.manualAimOffsetPending && now >= g_runtime.manualAimCaptureAfter) {
			const RE::NiPoint3 currentAngle = a_player->data.angle;
			const float offsetRetention = ActiveProfileDevice() == InputDevice::kGamepad ?
				g_config.manualAimGamepadOffsetRetention : g_config.manualAimKeyboardMouseOffsetRetention;
			g_runtime.manualAimOffsetYaw = Clamp(NormalizeAngle(currentAngle.z - desiredYaw) * offsetRetention, -kManualAimMaxYawOffset, kManualAimMaxYawOffset);
			g_runtime.manualAimOffsetPitch = Clamp(NormalizeAngle(currentAngle.x - desiredPitch) * offsetRetention, -kManualAimMaxPitchOffset, kManualAimMaxPitchOffset);
			g_runtime.manualAimOffsetPending = false;
			g_runtime.manualAimOffsetValid = true;
		}

		if (g_runtime.manualAimOffsetValid) {
			float offsetScale = 1.0F;
			if (g_runtime.manualAimRecoveryStart >= 0.0F) {
				const float recoveryDuration = std::max(g_config.manualAimReturnDuration, 0.01F);
				const float recoveryElapsed = now - g_runtime.manualAimRecoveryStart;
				const float recoveryT = Clamp(recoveryElapsed / recoveryDuration, 0.0F, 1.0F);
				const float smoothRecoveryT = recoveryT * recoveryT * (3.0F - (2.0F * recoveryT));
				if (recoveryT >= 1.0F) {
					g_runtime.manualAimRecoveryStart = -1.0F;
					g_runtime.manualAimOffsetValid = false;
				} else {
					offsetScale = 1.0F - smoothRecoveryT;
				}
			}

			if (g_runtime.manualAimOffsetValid) {
				desiredYaw += g_runtime.manualAimOffsetYaw * offsetScale;
				desiredPitch += g_runtime.manualAimOffsetPitch * offsetScale;
			}
		}

		RE::NiPoint3 newAngle = a_player->data.angle;
		const float yawDelta = NormalizeAngle(desiredYaw - newAngle.z);
		const float pitchDelta = NormalizeAngle(desiredPitch - newAngle.x);
		const float errorDegrees = std::sqrt((yawDelta * yawDelta) + (pitchDelta * pitchDelta)) * kRadToDeg;
		g_runtime.lastYawError = yawDelta;
		g_runtime.lastPitchError = pitchDelta;
		g_runtime.lastAimErrorDegrees = errorDegrees;
		if (errorDegrees <= g_config.lockDeadZoneDegrees) {
			return true;
		}

		const float strength = Clamp(g_config.assistStrengthScale * Clamp(a_assistMultiplier, 0.0F, 1.0F), 0.10F, 1.00F);
		const float movementBoost = 1.0F + (Clamp((g_runtime.targetSpeed - 120.0F) / 900.0F, 0.0F, 1.0F) * 0.25F);
		const float yawAlpha = 1.0F - std::exp(-(g_config.lockYawResponse * strength * movementBoost) * a_delta);
		const float pitchAlpha = 1.0F - std::exp(-(g_config.lockPitchResponse * strength * movementBoost) * a_delta);
		const float maxYawStep = g_config.lockMaxYawSpeed * strength * kDegToRad * a_delta;
		const float maxPitchStep = g_config.lockMaxPitchSpeed * strength * kDegToRad * a_delta;
		const float yawStep = Clamp(yawDelta * yawAlpha, -maxYawStep, maxYawStep);
		const float pitchStep = Clamp(pitchDelta * pitchAlpha, -maxPitchStep, maxPitchStep);

		newAngle.z += yawStep;
		newAngle.x += pitchStep;
		newAngle.x = Clamp(newAngle.x, -1.45F, 1.45F);
		a_player->SetAngleOnReference(newAngle);
		return true;
	}

	bool BeginAssistInternal(float a_now, bool a_auto)
	{
		LoadConfig();
		if (!IsFeatureEnabled()) {
			return false;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || player->IsDead(false)) {
			return false;
		}

		g_runtime = {};
		g_runtime.state = AssistState::kWaitingToRetarget;
		g_runtime.startTime = a_now;
		g_runtime.nextRetargetTime = a_now;
		g_runtime.lastAimActivityTime = a_now;
		g_runtime.baseFOV = EffectiveFOV();
		g_runtime.lastZoomRatio = 1.0F;
		g_runtime.autoActive = a_auto;
		Log("BeginAssist ready for immediate target search");
		return true;
	}

	bool BeginAssist(std::monostate, float a_now)
	{
		return BeginAssistInternal(a_now, false);
	}

	bool UpdateAssistInternal(float a_now, float a_delta)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || player->IsDead(false)) {
			g_runtime.state = AssistState::kIdle;
			return false;
		}
		const auto tryIndependentRetarget = [&](RE::Actor* a_previousTarget) {
			auto* nextTarget = FindBestTarget(player, a_previousTarget);
			if (!nextTarget) {
				return false;
			}

			g_runtime.target =
				RE::BSPointerHandleManagerInterface<RE::Actor>::GetHandle(nextTarget);
			g_runtime.state = AssistState::kAssisting;
			g_runtime.assistStartTime = a_now;
			g_runtime.targetFormID = nextTarget->GetFormID();
			g_runtime.hasLastTargetPoint = false;
			g_runtime.smoothedTargetVelocity = {};
			g_runtime.nextLineOfSightCheck =
				a_now + g_config.lineOfSightCheckInterval;
			g_runtime.lostLineOfSightSince = -1.0F;
			Log("Independent target lock transferred to another valid target");
			return true;
		};
		const bool aimAssistActive =
			g_runtime.state == AssistState::kWaitingToRetarget ||
			g_runtime.state == AssistState::kAssisting;
		const bool manualAimActive = a_now < g_runtime.manualAimOverrideUntil;
		const bool independentLockProtected =
			g_config.aimActivationMode == AimActivationMode::kToggleLock &&
			g_toggleLockActive &&
			!g_config.noFireTimeoutAffectsIndependentLock;
		if (aimAssistActive &&
			g_config.noFireTimeoutSeconds > 0.0F &&
			!independentLockProtected &&
			(a_now - g_runtime.lastAimActivityTime) >= g_config.noFireTimeoutSeconds &&
			!manualAimActive &&
			!g_runtime.targetSwitchRequested) {
			if (g_config.aimActivationMode == AimActivationMode::kToggleLock) {
				g_toggleLockActive = false;
			}
			StopAssist(std::monostate{});
			Log("Aim assist released: no aim activity");
			return false;
		}
		if ((g_runtime.state == AssistState::kWaitingToRetarget || g_runtime.state == AssistState::kAssisting) &&
		    g_config.disableOnHighZoom && CurrentZoomRatio() >= g_config.highZoomRatio) {
			g_runtime.target.reset();
			g_runtime.state = AssistState::kHoldingNoRetarget;
			Log("Lock-on released: high zoom ratio");
			return false;
		}

		if (g_runtime.state == AssistState::kWaitingToRetarget) {
			if (a_now < g_runtime.nextRetargetTime) {
				return true;
			}

			auto* nextTarget = FindBestTarget(player);
			if (!nextTarget) {
				g_runtime.nextRetargetTime = a_now + g_config.retargetDelay;
				return true;
			}

			g_runtime.target = RE::BSPointerHandleManagerInterface<RE::Actor>::GetHandle(nextTarget);
			g_runtime.state = AssistState::kAssisting;
			g_runtime.assistStartTime = a_now;
			g_runtime.targetFormID = nextTarget->GetFormID();
			g_runtime.hasLastTargetPoint = false;
			g_runtime.smoothedTargetVelocity = {};
			g_runtime.nextLineOfSightCheck = a_now + g_config.lineOfSightCheckInterval;
			g_runtime.lostLineOfSightSince = -1.0F;
			Log("Lock-on target acquired");
		}

		if (g_runtime.state == AssistState::kAssisting) {
			auto targetPtr = g_runtime.target.get();
			if (!targetPtr || !IsLockedTargetValid(player, targetPtr.get())) {
				g_runtime.target.reset();
				g_runtime.targetFormID = 0;
				if (g_toggleLockActive) {
					if (tryIndependentRetarget(targetPtr.get())) {
						return true;
					}
					g_toggleLockActive = false;
					StopZoomAssist();
					Log("Independent target lock ended: no valid target in acquisition range");
					return false;
				}
				g_runtime.state = AssistState::kWaitingToRetarget;
				g_runtime.nextRetargetTime = a_now + g_config.retargetDelay;
				Log("Lock-on target released; waiting to retarget");
				return true;
			}
			const bool manualOverride = a_now < g_runtime.manualAimOverrideUntil;
			if (g_runtime.targetSwitchRequested && a_now >= g_runtime.targetSwitchAt) {
				g_runtime.targetSwitchRequested = false;
				if (g_config.continueCrosshairTracking || g_toggleLockActive) {
					if (auto* nextTarget = FindBestTarget(player, targetPtr.get())) {
						g_runtime.target = RE::BSPointerHandleManagerInterface<RE::Actor>::GetHandle(nextTarget);
						g_runtime.targetFormID = nextTarget->GetFormID();
						g_runtime.hasLastTargetPoint = false;
						g_runtime.smoothedTargetVelocity = {};
						g_runtime.manualAimOffsetPending = false;
						g_runtime.manualAimOffsetValid = false;
						g_runtime.targetSwitchCooldownUntil = a_now + kTargetSwitchCooldown;
						g_runtime.lastAimActivityTime = a_now;
						Log("Manual target switch acquired a nearby hostile target");
						return true;
					}
				}
			}
			if (g_config.requireLineOfSight && a_now >= g_runtime.nextLineOfSightCheck) {
				g_runtime.nextLineOfSightCheck = a_now + g_config.lineOfSightCheckInterval;
				if (!HasClearLineOfSight(player, targetPtr.get())) {
					if (g_runtime.lostLineOfSightSince < 0.0F) {
						g_runtime.lostLineOfSightSince = a_now;
					}
					if ((a_now - g_runtime.lostLineOfSightSince) >= g_config.lineOfSightGracePeriod) {
						g_runtime.target.reset();
						g_runtime.targetFormID = 0;
						if (g_toggleLockActive) {
							if (tryIndependentRetarget(targetPtr.get())) {
								return true;
							}
							g_toggleLockActive = false;
							StopZoomAssist();
							Log("Independent target lock ended: line of sight lost and no replacement found");
							return false;
						}
						g_runtime.state = AssistState::kWaitingToRetarget;
						g_runtime.nextRetargetTime = a_now + g_config.retargetDelay;
						Log("Lock-on target released: clear line of sight check failed");
					}
					return true;
				}
				g_runtime.lostLineOfSightSince = -1.0F;

				if (g_config.continueCrosshairTracking || g_toggleLockActive) {
					if (auto* closerTarget = FindBestTarget(player, targetPtr.get()); closerTarget &&
						ShouldSwitchToTarget(player, targetPtr.get(), closerTarget)) {
						g_runtime.target = RE::BSPointerHandleManagerInterface<RE::Actor>::GetHandle(closerTarget);
						g_runtime.targetFormID = closerTarget->GetFormID();
						g_runtime.hasLastTargetPoint = false;
						g_runtime.smoothedTargetVelocity = {};
						Log("Lock-on target switched to closer candidate");
						return true;
					}
				}
			}

			float assistMultiplier = 1.0F;
			if (manualOverride) {
				g_runtime.manualAimWasActive = true;
				g_runtime.manualAimRecoveryStart = -1.0F;
				// Keep following the target while allowing the player to hold a manual body-part offset.
				assistMultiplier = 0.75F;
			}

			if (!manualOverride && g_runtime.manualAimWasActive) {
				g_runtime.manualAimWasActive = false;
				g_runtime.manualAimRecoveryStart = g_runtime.lastManualAimInputTime + g_config.manualAimReturnDelay;
			}
			if (!manualOverride && g_runtime.manualAimRecoveryStart >= 0.0F) {
				if (a_now < g_runtime.manualAimRecoveryStart) {
					// Keep the manually selected point stable during the optional pause.
					assistMultiplier = 0.75F;
				} else {
					// Use the normal lock strength while the offset eases fully back to the anchor.
					assistMultiplier = 1.0F;
				}
			}
			const bool shouldTrackCrosshair =
				g_config.continueCrosshairTracking || g_toggleLockActive;
			if (!shouldTrackCrosshair && g_runtime.initialAssistCorrectionDone) {
				return true;
			}

			// Normal assist and the one-shot mode use the same correction step. The
			// one-shot mode ends only after the selected anchor is reached; it does
			// not use a wall-clock timeout.
			const float assistDelta = Clamp(a_delta, 0.0F, 0.05F);
			if (!shouldTrackCrosshair && assistDelta <= 0.0F) {
				return true;
			}
			const bool applied = ApplyAssistStep(player, targetPtr.get(), assistDelta, assistMultiplier);
			if (shouldTrackCrosshair) {
				g_runtime.initialAssistCorrectionDone = true;
			} else if (g_runtime.lastAimErrorDegrees <= g_config.lockDeadZoneDegrees) {
				g_runtime.initialAssistCorrectionDone = true;
				Log(std::format(
					"Initial aim correction converged: errorDegrees={} success={}",
					g_runtime.lastAimErrorDegrees,
					applied));
			}
			return applied;
		}

		return g_runtime.state != AssistState::kIdle;
	}

	bool UpdateAssist(std::monostate, float a_now, float a_delta)
	{
		return UpdateAssistInternal(a_now, a_delta);
	}

	bool StopAssist(std::monostate)
	{
		g_runtime.state = AssistState::kIdle;
		g_runtime.target.reset();
		g_runtime.autoActive = false;
		g_runtime.initialAssistCorrectionDone = false;
		g_runtime.normalAssistSourceLatched = false;
		return true;
	}

	bool TryBeginPersistentToggleLock(float a_now)
	{
		LoadConfig();
		if (!IsFeatureEnabled() || !g_config.autoTriggerOnZoom) {
			return false;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || player->IsDead(false)) {
			return false;
		}
		if (g_config.disableOnHighZoom &&
			CurrentZoomRatio() >= g_config.highZoomRatio) {
			Log("Target-lock one-shot search skipped: high zoom ratio");
			return false;
		}

		auto* target = FindBestTarget(player);
		if (!target) {
			Log("Target-lock one-shot search found no target");
			return false;
		}

		StartZoomAssist(a_now);
		g_toggleLockActive = true;
		g_runtime.normalAssistSourceLatched = false;
		g_runtime.target =
			RE::BSPointerHandleManagerInterface<RE::Actor>::GetHandle(target);
		g_runtime.state = AssistState::kAssisting;
		g_runtime.assistStartTime = a_now;
		g_runtime.lastZoomRatio = CurrentZoomRatio();
		g_runtime.targetFormID = target->GetFormID();
		g_runtime.hasLastTargetPoint = false;
		g_runtime.smoothedTargetVelocity = {};
		g_runtime.nextLineOfSightCheck =
			a_now + g_config.lineOfSightCheckInterval;
		g_runtime.lostLineOfSightSince = -1.0F;
		Log("Target-lock one-shot search acquired target and enabled persistent assist");
		return true;
	}

	bool TogglePersistentTargetLockForSource(InputDevice a_source)
	{
		LoadConfig();
		if (g_config.aimActivationMode != AimActivationMode::kToggleLock) {
			Log(std::format(
				"Target-lock request ignored: independent mode disabled source={}",
				InputDeviceName(a_source)));
			return false;
		}
		if (!IsInputDeviceAllowedByMode(a_source)) {
			Log(std::format(
				"Target-lock request ignored: source={} inputMode={}",
				InputDeviceName(a_source),
				InputModeName(g_inputMode)));
			return false;
		}

		if (g_toggleLockActive) {
			g_toggleLockActive = false;
			Log(std::format(
				"Persistent target lock cancelled: source={}",
				InputDeviceName(a_source)));
			return false;
		}

		const bool acquired = TryBeginPersistentToggleLock(NowSeconds());
		Log(std::format(
			"Persistent target-lock toggle processed: source={} acquired={} inputMode={}",
			InputDeviceName(a_source),
			acquired,
			InputModeName(g_inputMode)));
		return acquired;
	}

	bool TogglePersistentTargetLock(std::monostate)
	{
		// Papyrus/MCM hotkeys are the keyboard/mouse path. The gamepad path calls
		// TogglePersistentTargetLockForSource directly so gamepad-only mode does
		// not get rejected by this keyboard-specific entry point.
		return TogglePersistentTargetLockForSource(InputDevice::kKeyboardMouse);
	}

	bool IsAssistActive(std::monostate)
	{
		return g_runtime.state == AssistState::kWaitingToRetarget ||
		       g_runtime.state == AssistState::kAssisting;
	}

	float GetCurrentZoomRatio(std::monostate)
	{
		return CurrentZoomRatio();
	}

	bool ToggleEnabled(std::monostate)
	{
		g_userEnabled = !g_userEnabled;
		if (!g_userEnabled) {
			g_toggleLockActive = false;
			StopZoomAssist();
		}
		Log(g_userEnabled ? "Aim assist toggled on" : "Aim assist toggled off");
		return g_userEnabled;
	}

	bool SetEnabled(std::monostate, bool a_enabled)
	{
		g_userEnabled = a_enabled;
		g_config.enabled = a_enabled;
		WriteIniValue("General", "bEnableAimAssist", a_enabled ? "1" : "0");
		WriteMcmValue("Main", "bEnabled", a_enabled ? "1" : "0");
		if (!g_userEnabled) {
			g_toggleLockActive = false;
			StopZoomAssist();
		}
		Log(g_userEnabled ? "Aim assist set on" : "Aim assist set off");
		return g_userEnabled;
	}

	bool SetTargetMarkerEnabled(std::monostate, bool a_enabled)
	{
		WriteIniValue("Feedback", "bEnableTargetMarker", a_enabled ? "1" : "0");
		WriteMcmValue("Main", "bEnableTargetMarker", a_enabled ? "1" : "0");
		g_config.targetMarkerEnabled = a_enabled;
		if (a_enabled) {
			g_nextTargetMarkerEnsureTime = 0.0F;
			EnsureTargetMarkerHUDMenu();
		}
		Log(a_enabled ? "Target marker enabled" : "Target marker disabled");
		return a_enabled;
	}

	int SetTargetMarkerStyle(std::monostate, int a_style)
	{
		g_mcmTargetMarkerStyle = std::clamp(a_style, 0, 5);
		g_config.targetMarkerStyle = g_mcmTargetMarkerStyle + 1;
		WriteIniValue("Feedback", "iTargetMarkerStyle", std::to_string(g_config.targetMarkerStyle));
		WriteMcmValue("Main", "iTargetMarkerStyle", std::to_string(g_mcmTargetMarkerStyle));
		Log(std::format(
			"Target marker style changed: mcmIndex={} nativeStyle={}",
			g_mcmTargetMarkerStyle,
			g_config.targetMarkerStyle));
		return g_mcmTargetMarkerStyle;
	}

	bool SetAllowHighZoom(std::monostate, bool a_enabled)
	{
		const bool iniWritten = WriteIniValue("Zoom", "bDisableOnHighZoom", a_enabled ? "0" : "1");
		const bool mcmWritten = WriteMcmValue("Main", "bAllowHighZoom", a_enabled ? "1" : "0");
		LoadConfig();
		Log(std::string("High-zoom aim assist ") +
			(a_enabled ? "enabled" : "restricted") +
			"; iniWrite=" + (iniWritten ? "ok" : "failed") +
			" mcmWrite=" + (mcmWritten ? "ok" : "failed"));
		return !g_config.disableOnHighZoom;
	}

	bool SetCrosshairTrackingEnabled(std::monostate, bool a_enabled)
	{
		const bool iniWritten = WriteIniValue("Assist", "bEnableCrosshairTracking", a_enabled ? "1" : "0");
		const bool mcmWritten = WriteMcmValue("Main", "bEnableCrosshairTracking", a_enabled ? "1" : "0");
		LoadConfig();
		Log(std::string("Continuous crosshair tracking ") +
			(a_enabled ? "enabled" : "disabled") +
			"; iniWrite=" + (iniWritten ? "ok" : "failed") +
			" mcmWrite=" + (mcmWritten ? "ok" : "failed"));
		return g_config.continueCrosshairTracking;
	}

	bool ToggleFocusMode(std::monostate)
	{
		LoadConfig();
		if (!g_config.focusEnabled || !IsFeatureEnabled()) {
			StopFocusMode(true);
			return false;
		}

		g_focusRequested = !g_focusRequested;
		if (!g_focusRequested) {
			StopFocusMode(false);
		}
		Log(g_focusRequested ? "Focus mode requested" : "Focus mode cancelled");
		return g_focusRequested;
	}

	float SetAssistStrengthScale(std::monostate, float a_value)
	{
		g_mcmAssistStrengthScale = Clamp(a_value, 0.10F, 1.00F);
		WriteIniValue("Assist", "fAssistStrengthScale", std::to_string(g_mcmAssistStrengthScale));
		WriteMcmValue("Main", "fAssistStrengthScale", std::to_string(g_mcmAssistStrengthScale));
		LoadConfig();
		Log("Assist strength scale changed");
		return g_mcmAssistStrengthScale;
	}

	float SetTargetFrictionStrength(std::monostate, float a_value)
	{
		g_mcmTargetFrictionStrength = Clamp(a_value, 0.0F, 1.0F);
		LoadConfig();
		Log("Target friction changed");
		return g_mcmTargetFrictionStrength;
	}

	int SetManualAimReturnDelay(std::monostate, int a_option)
	{
		g_mcmManualAimReturnDelay = static_cast<int>(Clamp(static_cast<float>(a_option), 0.0F, 2.0F));
		g_config.manualAimReturnDelay = ManualAimReturnDelayForOption(g_mcmManualAimReturnDelay);
		const bool mcmWritten = WriteMcmValue("Main", "iManualAimReturnDelay", std::to_string(g_mcmManualAimReturnDelay));
		const bool schemaWritten = WriteMcmValue("Main", "iManualAimReturnDelaySchema", "2");
		const bool iniWritten = WriteIniValue("ManualAim", "fReturnDelay", std::to_string(g_config.manualAimReturnDelay));
		LoadConfig();
		Log(std::string("Manual aim return delay changed; option=") +
			std::to_string(g_mcmManualAimReturnDelay) +
			" mcmWrite=" +
			(mcmWritten ? "ok" : "failed") +
			" schemaWrite=" +
			(schemaWritten ? "ok" : "failed") +
			" iniWrite=" +
			(iniWritten ? "ok" : "failed"));
		return g_mcmManualAimReturnDelay;
	}

	int SetTargetSwitchLevel(std::monostate, int a_level)
	{
		g_mcmTargetSwitchLevel = std::clamp(a_level, 0, 4);
		WriteIniValue("ManualAim", "iTargetSwitchLevel", std::to_string(g_mcmTargetSwitchLevel));
		WriteMcmValue("Main", "iTargetSwitchLevel", std::to_string(g_mcmTargetSwitchLevel));
		g_runtime.targetSwitchRequested = false;
		g_runtime.targetSwitchGestureActive = false;
		LoadConfig();
		Log("Target switch gesture level changed");
		return g_mcmTargetSwitchLevel;
	}

	float SetSearchConeDegrees(std::monostate, float a_value)
	{
		g_mcmSearchConeDegrees = Clamp(a_value, 3.0F, 24.0F);
		WriteIniValue("Targeting", "fSearchConeDegrees", std::to_string(g_mcmSearchConeDegrees));
		WriteMcmValue("Main", "fSearchConeDegrees", std::to_string(g_mcmSearchConeDegrees));
		LoadConfig();
		Log("Acquisition range changed");
		return g_mcmSearchConeDegrees;
	}

	float SetNoFireTimeoutSeconds(std::monostate, float a_value)
	{
		const float timeout = Clamp(a_value, 0.0F, 15.0F);
		WriteIniValue("Assist", "fNoFireTimeoutSeconds", std::to_string(timeout));
		WriteMcmValue("Main", "fNoFireTimeoutSeconds", std::to_string(timeout));
		LoadConfig();
		Log("No-fire timeout changed");
		return g_config.noFireTimeoutSeconds;
	}

	bool SetNoFireTimeoutAffectsIndependentLock(std::monostate, bool a_enabled)
	{
		WriteIniValue("Assist", "bNoFireTimeoutAffectsIndependentLock", a_enabled ? "1" : "0");
		WriteMcmValue("Main", "bNoFireTimeoutAffectsIndependentLock", a_enabled ? "1" : "0");
		LoadConfig();
		Log(a_enabled ?
			"Independent lock now uses inactivity timeout" :
			"Independent lock ignores inactivity timeout");
		return g_config.noFireTimeoutAffectsIndependentLock;
	}

	bool SetAllowOutOfCombatHostileTargets(std::monostate, bool a_enabled)
	{
		WriteIniValue("Targeting", "bAllowOutOfCombatHostileTargets", a_enabled ? "1" : "0");
		WriteIniValue("Targeting", "bOnlyCombatTargets", a_enabled ? "0" : "1");
		WriteMcmValue("Main", "bAllowOutOfCombatHostileTargets", a_enabled ? "1" : "0");
		LoadConfig();
		Log(a_enabled ?
			"Out-of-combat hostile target acquisition enabled" :
			"Out-of-combat hostile target acquisition disabled");
		return g_config.allowOutOfCombatHostileTargets;
	}

	float SetMeleeLockDistance(std::monostate, float a_value)
	{
		g_mcmMeleeLockDistance = Clamp(a_value, 100.0F, 3000.0F);
		WriteIniValue("Targeting", "fMeleeLockDistance", std::to_string(g_mcmMeleeLockDistance));
		WriteMcmValue("Main", "fMeleeLockDistance", std::to_string(g_mcmMeleeLockDistance));
		LoadConfig();
		Log("Melee lock distance changed");
		return g_config.meleeMaxLockDistance;
	}

	float SetRangedLockDistance(std::monostate, float a_value)
	{
		g_mcmRangedLockDistance = Clamp(a_value, 500.0F, 10000.0F);
		WriteIniValue("Targeting", "fMaxLockDistance", std::to_string(g_mcmRangedLockDistance));
		WriteMcmValue("Main", "fRangedLockDistance", std::to_string(g_mcmRangedLockDistance));
		LoadConfig();
		Log("Ranged lock distance changed");
		return g_config.maxLockDistance;
	}

	int SetAimAnchor(std::monostate, int a_anchor)
	{
		g_mcmAimAnchor = std::clamp(a_anchor, 0, 2);
		WriteIniValue("Targeting", "iAimAnchor", std::to_string(g_mcmAimAnchor));
		WriteMcmValue("Main", "iAimAnchor", std::to_string(g_mcmAimAnchor));
		LoadConfig();
		Log("Aim anchor changed");
		return g_mcmAimAnchor;
	}

	int CycleAimAnchor(std::monostate)
	{
		LoadConfig();
		g_mcmAimAnchor = (std::clamp(g_mcmAimAnchor, 0, 2) + 1) % 3;
		WriteIniValue("Targeting", "iAimAnchor", std::to_string(g_mcmAimAnchor));
		WriteMcmValue("Main", "iAimAnchor", std::to_string(g_mcmAimAnchor));
		LoadConfig();

		const auto anchor = static_cast<AimAnchor>(g_mcmAimAnchor);
		const std::string message = AimAnchorHudMessage(anchor);
		RE::SendHUDMessage::ShowHUDMessage(message.c_str(), nullptr, true, false);
		Log(std::format("Aim anchor cycled: value={} name={}", g_mcmAimAnchor, AimAnchorName(anchor)));
		return g_mcmAimAnchor;
	}

	bool SetToggleAimMode(std::monostate, bool a_enabled)
	{
		WriteIniValue("General", "bToggleAimMode", a_enabled ? "1" : "0");
		WriteMcmValue("Main", "bToggleAimMode", a_enabled ? "1" : "0");
		LoadConfig();

		g_toggleLockActive = false;
		g_gamepadLockButtonHeld = false;
		g_gamepadLockButtonPressedAt = 0.0F;
		StopZoomAssist();

		Log(a_enabled ?
			"Independent target-lock control enabled" :
			"Independent target-lock control disabled");
		return g_config.aimActivationMode == AimActivationMode::kToggleLock;
	}

	int SetGamepadLongPressAction(std::monostate, int a_action)
	{
		const auto action = static_cast<GamepadLongPressAction>(std::clamp(a_action, 0, 1));
		WriteIniValue("General", "iGamepadLongPressAction", std::to_string(static_cast<int>(action)));
		WriteMcmValue("Main", "iGamepadLongPressAction", std::to_string(static_cast<int>(action)));
		LoadConfig();
		Log(action == GamepadLongPressAction::kSneak ?
			"Gamepad long press action set to Sneak" :
			"Gamepad long press action set to TogglePOV");
		return static_cast<int>(g_config.gamepadLongPressAction);
	}

	bool SetKeyboardMouseMode(std::monostate)
	{
		g_inputModeInitialized = true;
		g_inputMode = InputMode::kKeyboardMouse;
		WriteIniValue("InputMode", "iInputMode", "0");
		WriteMcmValue("Main", "iInputMode", "0");
		g_toggleLockActive = false;
		g_gamepadLockButtonHeld = false;
		g_gamepadLockButtonPressedAt = 0.0F;
		StopZoomAssist();
		LoadConfig();
		Log("Input mode set to keyboard/mouse");
		return true;
	}

	bool SetGamepadMode(std::monostate)
	{
		g_inputModeInitialized = true;
		g_inputMode = InputMode::kGamepad;
		WriteIniValue("InputMode", "iInputMode", "1");
		WriteMcmValue("Main", "iInputMode", "1");
		g_toggleLockActive = false;
		g_gamepadLockButtonHeld = false;
		g_gamepadLockButtonPressedAt = 0.0F;
		StopZoomAssist();
		LoadConfig();
		Log("Input mode set to gamepad");
		return true;
	}

	int GetInputMode(std::monostate)
	{
		return InputModeToInt(g_inputMode);
	}

	int SetInputMode(std::monostate, int a_mode)
	{
		g_inputModeInitialized = true;
		g_inputMode = InputModeFromInt(a_mode);
		const auto modeValue = std::to_string(InputModeToInt(g_inputMode));
		WriteIniValue("InputMode", "iInputMode", modeValue);
		WriteMcmValue("Main", "iInputMode", modeValue);
		g_toggleLockActive = false;
		g_gamepadLockButtonHeld = false;
		g_gamepadLockButtonPressedAt = 0.0F;
		StopZoomAssist();
		LoadConfig();
		Log(std::format("Input mode set: {}", InputModeName(g_inputMode)));
		return GetInputMode(std::monostate{});
	}

	bool ExecuteGamepadLongPressAction()
	{
		if (g_config.gamepadLongPressAction == GamepadLongPressAction::kSneak) {
			auto* controls = RE::PlayerControls::GetSingleton();
			if (!controls) {
				return false;
			}
			const bool executed = controls->DoAction(
				RE::DEFAULT_OBJECT::kActionSneak,
				RE::ActionInput::ACTIONPRIORITY::kImperative);
			Log(std::format("Gamepad long press executed Sneak action: success={}", executed));
			return executed;
		}

		auto* camera = RE::PlayerCamera::GetSingleton();
		if (!camera) {
			return false;
		}
		const bool wasThirdPerson = camera->QCameraEquals(RE::CameraState::k3rdPerson);
		const bool wasFirstPerson = camera->QCameraEquals(RE::CameraState::kFirstPerson);
		if (!wasThirdPerson && !wasFirstPerson) {
			Log("Gamepad long press TogglePOV ignored during camera transition or special camera state");
			return false;
		}
		const auto nextState = wasThirdPerson ? RE::CameraState::kFirstPerson : RE::CameraState::k3rdPerson;
		const bool changed = camera->PushState(nextState) != nullptr;
		Log(std::format("Gamepad long press executed TogglePOV: targetState={} changed={}",
			static_cast<int>(nextState),
			changed));
		return changed;
	}

	class SimpleAimAssistInputHandler :
		public RE::PlayerInputHandler
	{
	public:
		explicit SimpleAimAssistInputHandler(RE::PlayerControlsData& a_data) :
			RE::PlayerInputHandler(a_data)
		{}

		bool ShouldHandleEvent(const RE::InputEvent* a_event) override
		{
			return a_event && (a_event->eventType == RE::INPUT_EVENT_TYPE::kButton ||
			                        a_event->eventType == RE::INPUT_EVENT_TYPE::kThumbstick ||
			                        a_event->eventType == RE::INPUT_EVENT_TYPE::kMouseMove);
		}

		void OnButtonEvent(const RE::ButtonEvent* a_event) override
		{
			if (!a_event) {
				return;
			}
			ObserveInputDevice(a_event->device.get());
			LogInputEventOnce(a_event);

			if (g_config.aimActivationMode == AimActivationMode::kToggleLock &&
				IsInputDeviceAllowedByMode(InputDevice::kGamepad) &&
				IsGamepadTargetLockButton(a_event)) {
				auto* mutableEvent = const_cast<RE::ButtonEvent*>(a_event);
				mutableEvent->handled = RE::InputEvent::HANDLED_RESULT::kStop;

				if (a_event->QJustPressed()) {
					g_gamepadLockButtonHeld = true;
					g_gamepadLockButtonPressedAt = NowSeconds();
					Log("Gamepad right-stick target-lock gesture started");
				} else if (a_event->QReleased()) {
					const float duration = g_gamepadLockButtonHeld ?
						std::max(0.0F, NowSeconds() - g_gamepadLockButtonPressedAt) :
						0.0F;
					if (duration < kGamepadTargetLockHoldSeconds) {
						TogglePersistentTargetLockForSource(InputDevice::kGamepad);
						Log(std::format(
							"Gamepad right-stick short press handled as target lock: duration={}",
							duration));
					} else {
						const bool executed = ExecuteGamepadLongPressAction();
						Log(std::format(
							"Gamepad right-stick long press executed fixed action on release: action={} success={}",
							static_cast<int>(g_config.gamepadLongPressAction),
							executed));
					}
					g_gamepadLockButtonHeld = false;
					g_gamepadLockButtonPressedAt = 0.0F;
				}
				return;
			}

			if (IsFireEvent(a_event) &&
				IsInputDeviceAllowedByMode(InputDeviceFromRuntime(a_event->device.get()))) {
				RegisterFireActivity(a_event, InputDeviceFromRuntime(a_event->device.get()));
			}
		}

		void OnThumbstickEvent(const RE::ThumbstickEvent* a_event) override
		{
			if (!a_event) {
				return;
			}
			ObserveInputDevice(a_event->device.get());
			LogThumbstickEventOnce(a_event);
			if (!IsInputDeviceAllowedByMode(InputDevice::kGamepad) ||
				a_event->QIDCode() != RE::ThumbstickEvent::kRight) {
				return;
			}

			const float magnitude = std::sqrt((a_event->xValue * a_event->xValue) + (a_event->yValue * a_event->yValue));
			RegisterManualAimInput(magnitude);
			RegisterTargetSwitchGesture(magnitude, GetTargetSwitchThresholds().gamepad);
		}

		void OnMouseMoveEvent(const RE::MouseMoveEvent* a_event) override
		{
			if (!a_event) {
				return;
			}

			const float mouseX = static_cast<float>(a_event->mouseInputX);
			const float mouseY = static_cast<float>(a_event->mouseInputY);
			const float magnitude = std::sqrt((mouseX * mouseX) + (mouseY * mouseY));
			if (magnitude <= 0.0F) {
				return;
			}
			ObserveInputDevice(a_event->device.get());
			if (!IsInputDeviceAllowedByMode(InputDevice::kKeyboardMouse)) {
				return;
			}
			RegisterManualAimInput(magnitude);
			RegisterTargetSwitchGesture(magnitude, GetTargetSwitchThresholds().keyboardMouse);
		}

		void PerFrameUpdate() override
		{
			const auto now = NowSeconds();
			float delta = now - g_lastFrameTime;
			g_lastFrameTime = now;
			delta = Clamp(delta, 0.0F, 0.05F);
			if (g_runtime.targetSwitchGestureActive &&
				(now - g_runtime.lastManualAimInputTime) >= kTargetSwitchGestureResetDelay) {
				g_runtime.targetSwitchGestureActive = false;
			}

			if (g_config.targetMarkerEnabled && now >= g_nextTargetMarkerEnsureTime) {
				g_nextTargetMarkerEnsureTime = now + 0.50F;
				EnsureTargetMarkerHUDMenu();
			}

			const bool actualAimState = IsActualAimStateActive();
			auto* player = RE::PlayerCharacter::GetSingleton();
			const auto rangeClass = GetPlayerWeaponRangeClass(player);
			const bool meleeWeapon = rangeClass == WeaponRangeClass::kMelee;
			if (!g_hasLoggedWeaponRangeClass ||
				rangeClass != g_lastLoggedWeaponRangeClass) {
				g_hasLoggedWeaponRangeClass = true;
				g_lastLoggedWeaponRangeClass = rangeClass;
				const float activeMaxDistance =
					meleeWeapon ? g_config.meleeMaxLockDistance : g_config.maxLockDistance;
				Log(std::format(
					"Active weapon range class changed: weaponClass={} melee={} "
					"activeMaxDistance={} meleeMaxDistance={} rangedMaxDistance={}",
					static_cast<int>(rangeClass),
					meleeWeapon,
					activeMaxDistance,
					g_config.meleeMaxLockDistance,
					g_config.maxLockDistance));
			}
			const auto meleeGuardSignals = QueryMeleeGuardSignals(player);
			const bool meleeGuardActive = meleeWeapon && meleeGuardSignals.active;
			if (!g_hasLoggedMeleeGuardState || meleeGuardActive != g_lastLoggedMeleeGuardState) {
				g_hasLoggedMeleeGuardState = true;
				g_lastLoggedMeleeGuardState = meleeGuardActive;
				Log(std::format(
					"Melee guard state {}: meleeWeapon={} weaponClass={} wantBlocking={} "
					"gunState={} graphIsBlocking={}/{} graphWantBlock={}/{}",
					meleeGuardActive ? "entered" : "exited",
					meleeWeapon,
					static_cast<int>(rangeClass),
					static_cast<int>(meleeGuardSignals.wantBlocking),
					player ? static_cast<int>(player->gunState) : -1,
					static_cast<int>(meleeGuardSignals.hasGraphIsBlocking),
					static_cast<int>(meleeGuardSignals.graphIsBlocking),
					static_cast<int>(meleeGuardSignals.hasGraphWantBlock),
					meleeGuardSignals.graphWantBlock));
			}
			const bool normalAimAssistRequested =
				meleeWeapon ? meleeGuardActive : actualAimState;
			const bool normalAssistSourceAllowed =
				g_zoomHeld ? g_runtime.normalAssistSourceLatched : IsNormalAssistSourceAllowed();
			const bool assistRequested =
				(normalAimAssistRequested && normalAssistSourceAllowed) || g_toggleLockActive;
			const bool shouldAssist =
				assistRequested && IsFeatureEnabled() && g_config.autoTriggerOnZoom;
			if (shouldAssist && !g_zoomHeld) {
				Log(g_toggleLockActive ?
					"Persistent target-lock request entered" :
					"Actual game aim state entered");
				StartZoomAssist(now);
			} else if (!shouldAssist && g_zoomHeld) {
				const auto camera = RE::PlayerCamera::GetSingleton();
				const auto cameraState = camera && camera->currentState ?
					static_cast<int>(camera->currentState->id.get()) : -1;
				Log(std::format(
					"All aim assist requests exited: actualAimState={} fireInputHeld={} "
					"gunState={} cameraState={} lastInputDevice={}",
					actualAimState,
					IsFireInputHeld(),
					player ? static_cast<int>(player->gunState) : -1,
					cameraState,
					InputDeviceName(g_lastInputDevice)));
				StopZoomAssist();
			}
			UpdateFocusMode(delta);

			if (!g_zoomHeld || !IsFeatureEnabled() || !g_config.autoTriggerOnZoom) {
				return;
			}

			if (g_runtime.state == AssistState::kIdle || g_runtime.state == AssistState::kHoldingNoRetarget) {
				return;
			}
			UpdateAssistInternal(now, delta);
		}
	};

	void RegisterInputSink()
	{
		if (g_inputSinkRegistered) {
			return;
		}
		if (auto* controls = RE::PlayerControls::GetSingleton()) {
			auto* handler = new SimpleAimAssistInputHandler(controls->data);
			controls->RegisterHandler(handler);
			for (auto it = controls->handlers.begin(); it != controls->handlers.end(); ++it) {
				if (*it == handler) {
					controls->handlers.erase(it);
					controls->handlers.insert(controls->handlers.begin(), handler);
					break;
				}
			}
			g_inputSinkRegistered = true;
			Log("Registered aim input handler at front of player input handlers");
		}
	}

	void F4SEMessageHandler(F4SE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) {
			return;
		}
		if (a_msg->type == F4SE::MessagingInterface::kPreLoadGame ||
			a_msg->type == F4SE::MessagingInterface::kNewGame) {
			StopZoomAssist();
			g_lastInputDevice = InputDevice::kUnknown;
			g_lastInputDeviceAt = -1.0F;
			g_keyboardMouseFireHeld = false;
			g_gamepadFireHeld = false;
			g_toggleLockActive = false;
			g_gamepadLockButtonHeld = false;
			g_gamepadLockButtonPressedAt = 0.0F;
			g_nextTargetMarkerEnsureTime = 0.0F;
			g_cachedWeaponRangeClass = WeaponRangeClass::kUnknown;
			g_cachedWeaponFormID = 0;
			g_nextWeaponRangeRefreshTime = -100.0F;
			Log("Reset aim assist state before game transition");
		} else if (a_msg->type == F4SE::MessagingInterface::kInputLoaded ||
			a_msg->type == F4SE::MessagingInterface::kGameLoaded ||
			a_msg->type == F4SE::MessagingInterface::kPostLoadGame) {
			LoadConfig();
			RegisterInputSink();
			RegisterTargetMarkerMenuSink();
			EnsureTargetMarkerHUDMenu();
		} else if (a_msg->type == F4SE::MessagingInterface::kGameDataReady) {
			LoadConfig();
			RegisterTargetMarkerMenuSink();
			EnsureTargetMarkerHUDMenu();
		}
	}

	bool RegisterPapyrus(RE::BSScript::IVirtualMachine* a_vm)
	{
		if (!a_vm) {
			return false;
		}

		a_vm->BindNativeMethod("SimpleAimAssistNative", "BeginAssist", BeginAssist);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "UpdateAssist", UpdateAssist);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "StopAssist", StopAssist);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "IsAssistActive", IsAssistActive);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "GetCurrentZoomRatio", GetCurrentZoomRatio);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "ToggleEnabled", ToggleEnabled);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetEnabled", SetEnabled);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetTargetMarkerEnabled", SetTargetMarkerEnabled);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetTargetMarkerStyle", SetTargetMarkerStyle);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetAllowHighZoom", SetAllowHighZoom);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetCrosshairTrackingEnabled", SetCrosshairTrackingEnabled);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "ToggleFocusMode", ToggleFocusMode);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetAssistStrengthScale", SetAssistStrengthScale);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetTargetFrictionStrength", SetTargetFrictionStrength);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetManualAimReturnDelay", SetManualAimReturnDelay);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetTargetSwitchLevel", SetTargetSwitchLevel);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetSearchConeDegrees", SetSearchConeDegrees);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetNoFireTimeoutSeconds", SetNoFireTimeoutSeconds);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetNoFireTimeoutAffectsIndependentLock", SetNoFireTimeoutAffectsIndependentLock);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetAllowOutOfCombatHostileTargets", SetAllowOutOfCombatHostileTargets);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetMeleeLockDistance", SetMeleeLockDistance);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetRangedLockDistance", SetRangedLockDistance);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetAimAnchor", SetAimAnchor);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "CycleAimAnchor", CycleAimAnchor);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetToggleAimMode", SetToggleAimMode);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetGamepadLongPressAction", SetGamepadLongPressAction);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "ToggleTargetLock", TogglePersistentTargetLock);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetKeyboardMouseMode", SetKeyboardMouseMode);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetGamepadMode", SetGamepadMode);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "GetInputMode", GetInputMode);
		a_vm->BindNativeMethod("SimpleAimAssistNative", "SetInputMode", SetInputMode);
		Log("Registered Papyrus natives");
		return true;
	}
}

SAA_EXPORT F4SE::PluginVersionData F4SEPlugin_Version = []() noexcept {
	F4SE::PluginVersionData v{};
	v.PluginVersion({ 1, 2, 5, 0 });
	v.PluginName("SimpleAimAssist");
	v.AuthorName("Sylva");
	v.UsesAddressLibrary(true);
	v.UsesAddressLibraryNG(true);
	v.UsesSigScanning(false);
	v.IsLayoutDependent(true);
	v.IsLayoutDependentNG(true);
	v.HasNoStructUse(false);
	v.CompatibleVersions({
		F4SE::RUNTIME_1_10_162, F4SE::RUNTIME_1_10_163,
		F4SE::RUNTIME_1_10_980, F4SE::RUNTIME_1_10_984,
		F4SE::RUNTIME_1_11_137, F4SE::RUNTIME_1_11_159,
		F4SE::RUNTIME_1_11_169, F4SE::RUNTIME_1_11_191,
		F4SE::RUNTIME_1_11_221, F4SE::RUNTIME_1_11_240
	});
	return v;
}();

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_intfc)
{
	F4SE::Init(a_intfc, { .log = false, .hook = false });
	LoadConfig();
	Log("Plugin load");
	if (auto* messaging = F4SE::GetMessagingInterface()) {
		messaging->RegisterListener(F4SEMessageHandler);
		Log("Registered F4SE message listener");
	}
	RegisterInputSink();
	RegisterTargetMarkerMenuSink();
	EnsureTargetMarkerHUDMenu();
	if (auto* papyrus = F4SE::GetPapyrusInterface()) {
		papyrus->Register(RegisterPapyrus);
	}
	return true;
}

SAA_EXPORT bool F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = "SimpleAimAssist";
	a_info->version = 1;
	if (a_f4se->IsEditor()) {
		return false;
	}
	return a_f4se->RuntimeVersion() >= F4SE::RUNTIME_1_10_162;
}

ScriptName SimpleAimAssist Hidden

Float Function _TickSeconds() Global
    Return 0.016
EndFunction

Function OnMcmAimAssistHotkey() Global
    SimpleAimAssistNative.ToggleEnabled()
EndFunction

Function OnMcmSetEnabled(Bool abEnabled) Global
    SimpleAimAssistNative.SetEnabled(abEnabled)
EndFunction

Function OnMcmSetToggleAimMode(Bool abEnabled) Global
    SimpleAimAssistNative.SetToggleAimMode(abEnabled)
EndFunction

Function OnMcmSetGamepadLongPressAction(Int aiAction) Global
    SimpleAimAssistNative.SetGamepadLongPressAction(aiAction)
EndFunction

Function OnMcmTargetLockHotkey() Global
    SimpleAimAssistNative.ToggleTargetLock()
EndFunction

Function OnMcmSetTargetMarkerEnabled(Bool abEnabled) Global
    SimpleAimAssistNative.SetTargetMarkerEnabled(abEnabled)
EndFunction

Function OnMcmSetTargetMarkerStyle(Int aiStyle) Global
    ; MCM dropdowns use zero-based indices; native converts them to style IDs.
    SimpleAimAssistNative.SetTargetMarkerStyle(aiStyle)
EndFunction

Function OnMcmSetAllowHighZoom(Bool abEnabled) Global
    SimpleAimAssistNative.SetAllowHighZoom(abEnabled)
EndFunction

Function OnMcmSetInputMode(Int aiMode) Global
    SimpleAimAssistNative.SetInputMode(aiMode)
EndFunction

Function OnMcmSetAssistStrengthScale(Float afValue) Global
    SimpleAimAssistNative.SetAssistStrengthScale(afValue)
EndFunction

Function OnMcmSetManualAimReturnDelay(Int aiOption) Global
    SimpleAimAssistNative.SetManualAimReturnDelay(aiOption)
EndFunction

Function OnMcmSetTargetSwitchLevel(Int aiLevel) Global
    SimpleAimAssistNative.SetTargetSwitchLevel(aiLevel)
EndFunction

Function OnMcmSetSearchConeDegrees(Float afValue) Global
    SimpleAimAssistNative.SetSearchConeDegrees(afValue)
EndFunction

Function OnMcmSetNoFireTimeoutSeconds(Float afValue) Global
    SimpleAimAssistNative.SetNoFireTimeoutSeconds(afValue)
EndFunction

Function OnMcmSetNoFireTimeoutAffectsIndependentLock(Bool abEnabled) Global
    SimpleAimAssistNative.SetNoFireTimeoutAffectsIndependentLock(abEnabled)
EndFunction

Function OnMcmSetAllowOutOfCombatHostileTargets(Bool abEnabled) Global
    SimpleAimAssistNative.SetAllowOutOfCombatHostileTargets(abEnabled)
EndFunction

Function OnMcmSetMeleeLockDistance(Float afValue) Global
    SimpleAimAssistNative.SetMeleeLockDistance(afValue)
EndFunction

Function OnMcmSetRangedLockDistance(Float afValue) Global
    SimpleAimAssistNative.SetRangedLockDistance(afValue)
EndFunction

Function OnMcmSetAimAnchor(Int aiAnchor) Global
    SimpleAimAssistNative.SetAimAnchor(aiAnchor)
EndFunction

Function OnMcmSetTargetFrictionStrength(Float afValue) Global
    SimpleAimAssistNative.SetTargetFrictionStrength(afValue)
EndFunction

Function OnMcmFocusHotkey() Global
    SimpleAimAssistNative.ToggleFocusMode()
EndFunction

Function OnMcmKeyboardMouseMode() Global
    SimpleAimAssistNative.SetKeyboardMouseMode()
EndFunction

Function OnMcmGamepadMode() Global
    SimpleAimAssistNative.SetGamepadMode()
EndFunction

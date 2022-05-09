#include "macro_set_command.h"
#include "ledmap.h"
#include "macros.h"
#include "timer.h"
#include "keymap.h"
#include "key_matrix.h"
#include "usb_report_updater.h"
#include "led_display.h"
#include "postponer.h"
#include "macro_recorder.h"
#include "macro_shortcut_parser.h"
#include "str_utils.h"
#include "utils.h"
#include "layer_switcher.h"
#include "mouse_controller.h"
#include "debug.h"
#include "caret_config.h"
#include "config_parser/parse_macro.h"
#include "slave_drivers/is31fl3xxx_driver.h"

static set_mode_t currentMode = SetMode_Set;

static const char* proceedByDot(const char* cmd, const char *cmdEnd)
{
    while(*cmd > 32 && *cmd != '.' && cmd < cmdEnd)    {
        cmd++;
    }
    if (*cmd != '.') {
        Macros_ReportError("'.' expected", NULL, NULL);
    }
    return cmd+1;
}

typedef struct {
    int32_t min;
    int32_t max;
} int_range_t;

typedef struct {
    float min;
    float max;
} float_range_t;

const int_range_t Range_Timeout16 = {
    .min = 0,
    .max = 65534
};

const int_range_t Range_Brightness8 = {
    .min = 0,
    .max = 255
};

const float_range_t Range_Float0To10 = {
    .min = 0.f,
    .max = 10.0f
};

static int32_t ProcessInt(
    int32_t currentValue,
    const char* arg,
    const char* textEnd,
    int_range_t range
)
{
    switch (currentMode) {
        case SetMode_Set:
            return Macros_ParseInt(arg, textEnd, NULL);
            return currentValue;
        case SetMode_Adjust:
            {
                int32_t step = Macros_ParseInt(arg, textEnd, NULL);

                if (step > 0) {
                    return MIN(currentValue + step, range.max);
                } else {
                    return MAX(currentValue + step, range.min);
                }

                return currentValue;
            }
        default:
        case SetMode_Toggle:
            Macros_ReportError("Toggle not supported by this command", arg, textEnd);
            return currentValue;
    }
}

static float ProcessFloat(
    float currentValue,
    const char* arg,
    const char* textEnd,
    float_range_t range
)
{
    switch (currentMode) {
        case SetMode_Set:
            return ParseFloat(arg, textEnd);
        case SetMode_Adjust:
            {
                float step = ParseFloat(arg, textEnd);
                if (step > 0) {
                    return MIN(currentValue + step, range.max);
                } else {
                    return MAX(currentValue + step, range.min);
                }

            }
        default:
        case SetMode_Toggle:
            Macros_ReportError("Toggle not supported by this command", arg, textEnd);
            return currentValue;
    }
}

static bool ProcessBoolean(
    bool currentValue,
    const char* arg,
    const char* textEnd
)
{
    switch (currentMode) {
        case SetMode_Set:
            return Macros_ParseBoolean(arg, textEnd);
        case SetMode_Toggle:
            {
                return !currentValue;
            }
        default:
        case SetMode_Adjust:
            Macros_ReportError("Adjust not supported by this command", arg, textEnd);
            return currentValue;
    }
}

static void moduleNavigationMode(const char* arg1, const char *textEnd, module_configuration_t* module)
{
    layer_id_t layerId = Macros_ParseLayerId(arg1, textEnd);
    navigation_mode_t modeId = ParseNavigationModeId(NextTok(arg1, textEnd), textEnd);

    if (Macros_ParserError) {
        return;
    }

    module->navigationModes[layerId] = modeId;
}

static void moduleSpeed(const char* arg1, const char *textEnd, module_configuration_t* module, uint8_t moduleId)
{
    const char* arg2 = NextTok(arg1, textEnd);

    if (TokenMatches(arg1, textEnd, "baseSpeed")) {
        module->baseSpeed = ParseFloat(arg2, textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "speed")) {
        module->speed = ParseFloat(arg2, textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "xceleration")) {
        module->xceleration = ParseFloat(arg2, textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "caretSpeedDivisor")) {
        module->caretSpeedDivisor = ParseFloat(arg2, textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "scrollSpeedDivisor")) {
        module->scrollSpeedDivisor = ParseFloat(arg2, textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "pinchZoomDivisor") && moduleId == ModuleId_TouchpadRight) {
        module->pinchZoomSpeedDivisor = ParseFloat(arg2, textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "axisLockSkew")) {
        module->axisLockSkew = ParseFloat(arg2, textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "axisLockFirstTickSkew")) {
        module->axisLockFirstTickSkew = ParseFloat(arg2, textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "cursorAxisLockEnabled")) {
        module->cursorAxisLock = Macros_ParseBoolean(arg2, textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "scrollAxisLockEnabled")) {
        module->scrollAxisLock = Macros_ParseBoolean(arg2, textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "swapAxes")) {
        module->swapAxes = Macros_ParseBoolean(arg2, textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "invertScrollDirection")) {
        module->invertScrollDirection = Macros_ParseBoolean(arg2, textEnd);
    }
    else {
        Macros_ReportError("parameter not recognized:", arg1, textEnd);
    }
}

static void module(const char* arg1, const char *textEnd)
{
    layer_id_t moduleId = ParseModuleId(arg1, textEnd);
    module_configuration_t* module = GetModuleConfiguration(moduleId);

    const char* arg2 = proceedByDot(arg1, textEnd);

    if (Macros_ParserError) {
        return;
    }

    if (TokenMatches(arg2, textEnd, "navigationMode")) {
        const char* arg3 = proceedByDot(arg2, textEnd);
        moduleNavigationMode(arg3, textEnd, module);
    }
    else {
        moduleSpeed(arg2, textEnd, module, moduleId);
    }
}

static void secondaryRoles(const char* arg1, const char *textEnd)
{
    Macros_ReportError("command not recognized:", arg1, textEnd);
}

static void mouseKeys(const char* arg1, const char *textEnd)
{
    mouse_kinetic_state_t* state = &MouseMoveState;

    if (TokenMatches(arg1, textEnd, "move")) {
        state = &MouseMoveState;
    } else if (TokenMatches(arg1, textEnd, "scroll")) {
        state = &MouseScrollState;
    } else {
        Macros_ReportError("scroll or move expected", NULL, NULL);
    }

    const char* arg2 = proceedByDot(arg1, textEnd);
    const char* arg3 = NextTok(arg2, textEnd);

    if (TokenMatches(arg2, textEnd, "initialSpeed")) {
        state->initialSpeed = Macros_ParseInt(arg3, textEnd, NULL) / state->intMultiplier;
    }
    else if (TokenMatches(arg2, textEnd, "baseSpeed")) {
        state->baseSpeed = Macros_ParseInt(arg3, textEnd, NULL) / state->intMultiplier;
    }
    else if (TokenMatches(arg2, textEnd, "initialAcceleration")) {
        state->acceleration = Macros_ParseInt(arg3, textEnd, NULL) / state->intMultiplier;
    }
    else if (TokenMatches(arg2, textEnd, "deceleratedSpeed")) {
        state->deceleratedSpeed = Macros_ParseInt(arg3, textEnd, NULL) / state->intMultiplier;
    }
    else if (TokenMatches(arg2, textEnd, "acceleratedSpeed")) {
        state->acceleratedSpeed = Macros_ParseInt(arg3, textEnd, NULL) / state->intMultiplier;
    }
    else if (TokenMatches(arg2, textEnd, "axisSkew")) {
        state->axisSkew = ParseFloat(arg3, textEnd);
    }
    else {
        Macros_ReportError("parameter not recognized:", arg1, textEnd);
    }
}

static void stickyModifiers(const char* arg1, const char *textEnd)
{
    if (TokenMatches(arg1, textEnd, "never")) {
        StickyModifierStrategy = Stick_Never;
    }
    else if (TokenMatches(arg1, textEnd, "smart")) {
        StickyModifierStrategy = Stick_Smart;
    }
    else if (TokenMatches(arg1, textEnd, "always")) {
        StickyModifierStrategy = Stick_Always;
    }
    else {
        Macros_ReportError("parameter not recognized:", arg1, textEnd);
    }
}

static void macroEngineScheduler(const char* arg1, const char *textEnd)
{
    if (TokenMatches(arg1, textEnd, "preemptive")) {
        Macros_Scheduler = Scheduler_Preemptive;
    }
    else if (TokenMatches(arg1, textEnd, "blocking")) {
        Macros_Scheduler = Scheduler_Blocking;
    }
    else {
        Macros_ReportError("parameter not recognized:", arg1, textEnd);
    }
}

static void macroEngine(const char* arg1, const char *textEnd)
{
    if (TokenMatches(arg1, textEnd, "scheduler")) {
        macroEngineScheduler(NextTok(arg1,  textEnd), textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "batchSize")) {
        Macros_MaxBatchSize = Macros_ParseInt(NextTok(arg1,  textEnd), textEnd, NULL);
    }
    else {
        Macros_ReportError("parameter not recognized:", arg1, textEnd);
    }
}

static void backlightStrategy(const char* arg1, const char *textEnd)
{
    if (TokenMatches(arg1, textEnd, "functional")) {
        LedMap_BacklightStrategy = BacklightStrategy_Functional;
    }
    else if (TokenMatches(arg1, textEnd, "constantRgb")) {
        LedMap_BacklightStrategy = BacklightStrategy_ConstantRGB;
    }
    else {
        Macros_ReportError("parameter not recognized:", arg1, textEnd);
    }
}

static void constantRgb(const char* arg1, const char *textEnd)
{
    if (TokenMatches(arg1, textEnd, "rgb")) {
        const char* r = NextTok(arg1,  textEnd);
        const char* g = NextTok(r, textEnd);
        const char* b = NextTok(g, textEnd);
        LedMap_ConstantRGB.red = Macros_ParseInt(r, textEnd, NULL);
        LedMap_ConstantRGB.green = Macros_ParseInt(g, textEnd, NULL);
        LedMap_ConstantRGB.blue = Macros_ParseInt(b, textEnd, NULL);
        LedMap_BacklightStrategy = BacklightStrategy_ConstantRGB;
    }
    else {
        Macros_ReportError("parameter not recognized:", arg1, textEnd);
    }
}

static void leds(const char* arg1, const char *textEnd)
{
    const char* value = NextTok(arg1, textEnd);
    if (TokenMatches(arg1, textEnd, "fadeTimeout")) {
        LedSleepTimeout = 1000*60*ProcessInt(LedSleepTimeout/1000/60, value, textEnd, Range_Timeout16);
    } else if (TokenMatches(arg1, textEnd, "brightness")) {
        LedBrightnessMultiplier = ProcessFloat(LedBrightnessMultiplier, value, textEnd, Range_Float0To10);
    } else if (TokenMatches(arg1, textEnd, "enabled")) {
        LedsEnabled = ProcessBoolean(LedsEnabled, value, textEnd);
    } else {
        Macros_ReportError("parameter not recognized:", arg1, textEnd);
    }

    LedSlaveDriver_UpdateLeds();
}

static void backlight(const char* arg1, const char *textEnd)
{
    if (TokenMatches(arg1, textEnd, "strategy")) {
        backlightStrategy(NextTok(arg1, textEnd), textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "constantRgb")) {
        constantRgb(proceedByDot(arg1, textEnd), textEnd);
    }
    else {
        Macros_ReportError("parameter not recognized:", arg1, textEnd);
    }
}

static void navigationModeAction(const char* arg1, const char *textEnd)
{
    navigation_mode_t navigationMode = NavigationMode_Caret;
    bool positive = true;
    caret_axis_t axis = CaretAxis_Horizontal;

    const char* arg2 = proceedByDot(arg1, textEnd);
    const char* arg3 = NextTok(arg2, textEnd);

    navigationMode = ParseNavigationModeId(arg1, textEnd);

    if(navigationMode < NavigationMode_RemappableFirst || navigationMode > NavigationMode_RemappableLast) {
        Macros_ReportError("Invalid or non-remapable navigation mode", arg1, textEnd);
    }

    if (Macros_ParserError) {
        return;
    }

    if (TokenMatches(arg2, textEnd, "left")) {
        axis = CaretAxis_Horizontal;
        positive = false;
    }
    else if (TokenMatches(arg2, textEnd, "up")) {
        axis = CaretAxis_Vertical;
        positive = false;
    }
    else if (TokenMatches(arg2, textEnd, "right")) {
        axis = CaretAxis_Horizontal;
        positive = true;
    }
    else if (TokenMatches(arg2, textEnd, "down")) {
        axis = CaretAxis_Vertical;
        positive = true;
    }
    else {
        Macros_ReportError("parameter not recognized:", arg1, textEnd);
    }

    uint8_t macroIndex;
    if (TokenMatches(arg3, textEnd, "none")) {
        macroIndex = 255;
    } else {
        macroIndex = FindMacroIndexByName(arg3, TokEnd(arg3, textEnd), true);
    }

    SetModuleCaretConfiguration(navigationMode, axis, positive, macroIndex);
}

static void keymapAction(const char* arg1, const char *textEnd)
{
    const char* arg2 = proceedByDot(arg1, textEnd);
    const char* arg3 = NextTok(arg2, textEnd);

    uint8_t layerId = Macros_ParseLayerId(arg1, textEnd);
    uint16_t keyId = Macros_ParseInt(arg2, textEnd, NULL);

    uint8_t macroIndex = FindMacroIndexByName(arg3, TokEnd(arg3, textEnd), true);

    uint8_t slotIdx = keyId/64;
    uint8_t inSlotIdx = keyId%64;

    if(slotIdx > SLOT_COUNT || inSlotIdx > MAX_KEY_COUNT_PER_MODULE) {
        Macros_ReportError("invalid key id:", arg2, textEnd);
    }

    if (Macros_ParserError) {
        return;
    }

    key_action_t* action = &CurrentKeymap[layerId][slotIdx][inSlotIdx];

    if(macroIndex == 255) {
        action->type = KeyActionType_None;
    } else {
        action->type = KeyActionType_PlayMacro;
        action->playMacro.macroId = macroIndex;
    }
}

static void modLayerTriggers(const char* arg1, const char *textEnd)
{
    const char* specifier = NextTok(arg1, textEnd);
    uint8_t layerId = Macros_ParseLayerId(arg1, textEnd);
    uint8_t left = 0;
    uint8_t right = 0;

    switch (layerId) {
    case LayerId_Shift:
        left = HID_KEYBOARD_MODIFIER_LEFTSHIFT ;
        right = HID_KEYBOARD_MODIFIER_RIGHTSHIFT ;
        break;
    case LayerId_Control:
        left = HID_KEYBOARD_MODIFIER_LEFTCTRL ;
        right = HID_KEYBOARD_MODIFIER_RIGHTCTRL ;
        break;
    case LayerId_Alt:
        left = HID_KEYBOARD_MODIFIER_LEFTALT ;
        right = HID_KEYBOARD_MODIFIER_RIGHTALT ;
        break;
    case LayerId_Super:
        left = HID_KEYBOARD_MODIFIER_LEFTGUI ;
        right = HID_KEYBOARD_MODIFIER_RIGHTGUI ;
        break;
    default:
        Macros_ReportError("This layer does not allow modifier triggers.", arg1, specifier);
        return;
    }

    if (Macros_ParserError) {
        return;
    }

    if (TokenMatches(specifier, textEnd, "left")) {
        LayerConfig[layerId].modifierLayerMask = left;
    }
    else if (TokenMatches(specifier, textEnd, "right")) {
        LayerConfig[layerId].modifierLayerMask = right;
    }
    else if (TokenMatches(specifier, textEnd, "both")) {
        LayerConfig[layerId].modifierLayerMask = left | right;
    }
    else {
        Macros_ReportError("Specifier not recognized", specifier, textEnd);
    }
}

macro_result_t MacroSetCommand(const char* arg1, const char *textEnd, set_mode_t mode)
{
    currentMode = mode;
    const char* arg2 = NextTok(arg1, textEnd);

    if (mode != SetMode_Set) {
        if (TokenMatches(arg1, textEnd, "leds")) {
            leds(proceedByDot(arg1, textEnd), textEnd);
        } else {
            Macros_ReportError("adjust/toggle is not allowed for this section", arg1, textEnd);
        }
        return MacroResult_Finished;
    }

    if (TokenMatches(arg1, textEnd, "module")) {
        module(proceedByDot(arg1, textEnd), textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "secondaryRoles")) {
        secondaryRoles(proceedByDot(arg1, textEnd), textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "mouseKeys")) {
        mouseKeys(proceedByDot(arg1, textEnd), textEnd);
    }
#ifdef EXTENDED_MACROS
    else if (TokenMatches(arg1, textEnd, "keymapAction")) {
        keymapAction(proceedByDot(arg1, textEnd), textEnd);
    }
#endif
    else if (TokenMatches(arg1, textEnd, "navigationModeAction")) {
        navigationModeAction(proceedByDot(arg1, textEnd), textEnd);
    }
#ifdef EXTENDED_MACROS
    else if (TokenMatches(arg1, textEnd, "macroEngine")) {
        macroEngine(proceedByDot(arg1, textEnd), textEnd);
    }
#endif
    else if (TokenMatches(arg1, textEnd, "backlight")) {
        backlight(proceedByDot(arg1, textEnd), textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "leds")) {
        leds(proceedByDot(arg1, textEnd), textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "modifierLayerTriggers")) {
        modLayerTriggers(proceedByDot(arg1, textEnd), textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "diagonalSpeedCompensation")) {
        DiagonalSpeedCompensation = Macros_ParseBoolean(arg2, textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "stickyModifiers")) {
        stickyModifiers(arg2, textEnd);
    }
    else if (TokenMatches(arg1, textEnd, "debounceDelay")) {
        uint16_t time = Macros_ParseInt(arg2, textEnd, NULL);
        DebounceTimePress = time;
        DebounceTimeRelease = time;
    }
    else if (TokenMatches(arg1, textEnd, "keystrokeDelay")) {
        KeystrokeDelay = Macros_ParseInt(arg2, textEnd, NULL);
    }
    else if (
            TokenMatches(arg1, textEnd, "doubletapTimeout")  // new name
#ifdef EXTENDED_MACROS
            || TokenMatches(arg1, textEnd, "doubletapDelay") // deprecated alias - old name
#endif
            ) {
        uint16_t delay = Macros_ParseInt(arg2, textEnd, NULL);
        DoubleTapSwitchLayerTimeout = delay;
        DoubletapConditionTimeout = delay;
    }
    else if (TokenMatches(arg1, textEnd, "chordingDelay")) {
        ChordingDelay = Macros_ParseInt(arg2, textEnd, NULL);
    }
#ifdef EXTENDED_MACROS
    else if (TokenMatches(arg1, textEnd, "emergencyKey")) {
        uint16_t key = Macros_ParseInt(arg2, textEnd, NULL);
        EmergencyKey = Utils_KeyIdToKeyState(key);
    }
#endif
    else {
        Macros_ReportError("parameter not recognized:", arg1, textEnd);
    }
    return MacroResult_Finished;
}

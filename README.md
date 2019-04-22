# Ultimate Hacking Keyboard firmware with extended macro engine

This is a fork of [UHK firmware](https://github.com/UltimateHackingKeyboard/firmware). This custom firmware provides extended macro engine. Namely, we allow a set of simple commands to be used in text macro actions. These commands can be used to reach functionality otherwise unavailable via agent.

## Compatibility

This firmware is 100% compatible with the original unmodified agent. All you need is to flash the modified firmware to your UHK. Configurations won't get lost in case you decide to switch back to official firmware, or if you then again flash the modified version too, since config formats were not altered in any way. (Extended macros will just not work when used within the official firmware, obviously.)

## Features

The firmware implements:
- macro commands for (almost?) all basic features of the keyboard otherwise unreachable via agent. 
- macro commands for conditionals, jumps and sync mechanisms 
- some extended configuration options (composite-keystroke delay, sticky modifiers)
- runtime macro recorder implemented on scancode level, for vim-like macro functionality
- ability to run multiple macros at the same time

Some of the usecases which can be achieved via these commands are: 
- ability to mimic secondary roles 
- ability to bind actions to doubletaps 
- ability to bind modifier-chorded actions (i.e., shortucts which consists of arbitrary number of modifiers and at most one other key)
- ability to bind shift and non-shift scancodes independently
- ability to configure custom layer switching logic, including nested layer toggling 
- flow control via goto command
- runtime macros

## Examples
**Note that every command (i.e., every line in the examples) has to be inputted as a separate action!** Also note that macros are being run "asynchronously" (i.e., interleaved with other event handling) at a pace of at most one action per update cycle. A macro action may take one or more update cycles to complete (esp. delay commands and all commands which interfere with usb reports).

For instance, if the following text is pasted as a macro text action, playing the macro will result in switching to QWR keymap.
    
    $switchKeymap QWR
    
Runtime macro recorder example. In this setup, shift+key will start recording (indicated by the "adaptive mode" led), another shit+key will stop recording. Hiting the key alone will then replay the macro (e.g., a simple repetitive text edit).

    $ifShift recordMacro A
    $ifNotShift playMacro A

Implementation of standard double-tap-locking hold modifier in recursive version could look like:

    $holdLayer fn
    $ifDoubletap toggleLayer fn

Once the layer is toggled, the target layer needs to contain another macro to allow return to the base layer. For instance:

    $holdLayer previous
    $ifDoubletap unToggleLayer
    
Alternative way to implement the above example would be the following. However, using `holdLayer` for "hold" mechanisms is strongly encouraged due to more elaborate release logic:

    $toggleLayer fn
    $delayUntilRelease
    $untoggleLayer 
    $ifDoubletap toggleLayer fn

Creating double-shift-to-caps may look like:
   
    <press Shift>
    $delayUntilRelease
    <releaseShift>
    $ifNotDoubletap break
    <tap CapsLock>

Smart toggle (if tapped, locks layer; if used with a key, acts as a secondary role):

    $holdLayer mouse
    $ifNotInterrupted toggleLayer mouse
    

Regular secondary role:

    $holdLayer mouse
    $ifInterrupted break
    <regular action>

Mapping shift/nonshift scancodes independently:

    $ifShift suppressMods write 4
    $ifNotShift write %
    
The following applies global settings. Namely turns off sticky modifiers (i.e., modifiers of composite keystrokes will apply but will no longer stick) so that composite actions don't affect external mouse. Furthermore, it enables separation of composite keystrokes, and increases update delay - this may be useful if macro playback needs to be slown down (e.g., because some software does not register short taps) or if artificial delays need to be introduced for some reason. (Recommended values are 0,1,0.)

    $setStickyModsEnabled 0 
    $setSplitCompositeKeystroke 1
    $setKeystrokeDelay 10

Postponed secondary role switch. This modification prevents secondary role hiccups on alphabetic keys. The `resolveSecondary` will listen for some time and once it decides whether the current situation fits primary or secondary action, it will issue goTo to the "second" line (line 1 since we index from 0) or the last line (line 3). Actions are indexed from 0. Any keys pressed during resolution are postponed until the first command after the jump is performed.

    $resolveSecondary 350 1 3
    $write f
    $break
    $holdLayer mod

Mapping custom shortcuts may be done using `ifShortcut` command. The macro needs to be placed on the first key of the shortcut, and refers to other keys by their hardware ids obtained by `resolveNextKeyId` command and pressing the key (while having focused text editor). The command will postpone other actions until sufficient number of keys is pressed. If the pressed keys correspond to the arguments, the keys are consumed and the rest of the command performed. Otherwise, postponed keypresses are either used up by the rest of the macro or replayed back. The `final` modifier breaks the command after the "modified" `tapKey` command finishes. 

    $ifShortcut 90 final tapKey v
    $ifShortcut 88 final tapKey x
    $ifShortcut 70 71 final tapKey CG-a
    $tapKey c

`resolveNextKeyEq` is a bit more flexible and less-user friendly version of the `ifShortcut` command. It does not consume keys, allows querying for keys which are further in the queue and timeout is not limited to release of the key. The example shows a "rocker guesture" where sequence c->v is mapped to V. This is macro for the c key.  Alternatively, `resolveSecondary` could be used to implement similar functionality when combined with another layer. In that case, `resolveSecondary` will require prolonged press of c in order to activate the shortcut and won't interfere with writing. This version will eat all `cv`s encountered while writing, but does not depend on proper release of the keys. Rocker guestures work fine for key combinations which are not encountered in normal text, i.e., for small number of special cases. `ResolveSecondary` should be used for mapping key clusters.

    $resolveNextKeyEq 0 90 untilRelease 1 4
    $consumePending 1
    $tapKey V
    $break
    $tapKey c

You can simplify writing macros by using `#` and `@` characters. The first resolves a number as an index of a register. The second interprets the number as a relative action index. For instance the following macro will write out five "a"s with 50 ms delays
    
    $setReg 0 50
    $setReg 1 5
    $tapKey a
    $delayUntil #0
    $repeatFor 1 @-2
    
    
## Reference manual

The following grammar is supported:

    BODY = $COMMAND 
    BODY = $COMMAND [//<comment, excluding commands taking custom text arguments>]
    BODY = //<comment>
    BODY = #<comment>
    COMMAND = [CONDITION|MODIFIER]* COMMAND
    COMMAND = delayUntilRelease
    COMMAND = delayUntil <timeout (NUMBER)>
    COMMAND = delayUntilReleaseMax <timeout (NUMBER)>
    COMMAND = switchKeymap KEYMAPID
    COMMAND = toggleLayer LAYERID
    COMMAND = toggleKeymapLayer KEYMAPID LAYERID
    COMMAND = unToggleLayer
    COMMAND = holdLayer LAYERID
    COMMAND = holdLayerMax LAYERID <time in ms (NUMBER)>
    COMMAND = holdKeymapLayer KEYMAPID LAYERID
    COMMAND = holdKeymapLayerMax KEYMAPID LAYERID <time in ms (NUMBER)>
    COMMAND = resolveSecondary <time in ms (NUMBER)> [<time in ms (NUMBER)>] <primary action macro action index (NUMBER)> <secondary action macro action index (NUMBER)>
    COMMAND = resolveNextKeyId 
    COMMAND = resolveNextKeyEq <queue position (NUMBER)> KEYID {<time in ms>|untilRelease} <action adr (NUMBER)> <action adr (NUMBER)>
    COMMAND = consumePending
    COMMAND = postponeNext <number of commands (NUMER)>
    COMMAND = break
    COMMAND = noOp
    COMMAND = statsRuntime
    COMMAND = statsLayerStack
    COMMAND = statsPostponerStack
    COMMAND = statsActiveKeys
    COMMAND = diagnose
    COMMAND = printStatus
    COMMAND = setStatus <custom text>
    COMMAND = clearStatus 
    COMMAND = setLedTxt <timeout (NUMBER)> <custom text>
    COMMAND = write <custom text>
    COMMAND = goTo <index (NUMBER)>
    COMMAND = repeatFor <register index (NUMBER)> <action adr (NUMBER)>
    COMMAND = recordMacroDelay
    COMMAND = stopRecording
    COMMAND = {recordMacro|playMacro} [<slot identifier (MACROID)>]
    COMMAND = {startMouse|stopMouse} {move DIRECTION|scroll DIRECTION|accelerate|decelerate}
    COMMAND = setStickyModsEnabled {0|1}
    COMMAND = setActivateOnRelease {0|1}
    COMMAND = setSplitCompositeKeystroke {0|1}
    COMMAND = setKeystrokeDelay <time in ms, at most 250 (NUMBER)>
    COMMAND = setDebounceDelay <time in ms, at most 250 (NUMBER)>
    COMMAND = setReg <register index (NUMBER)> <value (NUMBER)> 
    COMMAND = {addReg|subReg|mulReg} <register index (NUMBER)> <value (NUMBER)>
    COMMAND = pressKey/holdKey/tapKey/releaseKey SHORTCUT
    CONDITION = {ifDoubletap|ifNotDoubletap}
    CONDITION = {ifPending|ifNotPending} <n (NUMBER)>
    CONDITION = {ifPendingId|ifNotPendingId} <idx in buffer (NUMBER)> KEYID
    CONDITION = {ifInterrupted|ifNotInterrupted}
    CONDITION = {ifReleased|ifNotReleased}
    CONDITION = {ifPlaytime|ifNotPlaytime} <timeout in ms (NUMBER)>
    CONDITION = {ifShift | ifAlt | ifCtrl | ifGui | ifAnyMod | ifNotShift | ifNotAlt | ifNotCtrl | ifNotGui | ifNotAnyMod}
    CONDITION = {ifRegEq|ifNotRegEq} <register index (NUMBER)> <value (NUMBER)>
    CONDITION = {ifRecording|ifNotRecording}
    CONDITION = {ifRecordingId|ifNotRecordingId} MACROID
    CONDITION = {ifShortcut|ifNotShortcut} [KEYID]*
    CONDITION = {ifGesture|ifNotGesture} [KEYID]*
    MODIFIER = suppressMods
    MODIFIER = suppressKeys
    MODIFIER = postponeKeys
    MODIFIER = final
    DIRECTION = {left|right|up|down}
    LAYERID = {fn|mouse|mod|base}|last|previous
    KEYMAPID = <abbrev>|last
    MACROID = last|CHAR|NUMBER
    NUMBER = [0-9]+ | -[0-9]+ | #<register idx (NUMBER)> | #key | @<relative macro action index(NUMBER)> 
    CHAR = <any ascii char>
    KEYID = <id of hardware key obtained by resolveNextKeyId (NUMBER)>
    SHORTCUT = MODMASK-KEY | KEY
    MODMASK = [MODMASK]+ | [L|R]{S|C|A|G}
    KEY = CHAR|KEYABBREV
    KEYABBREV = enter | escape | backspace | tab | space | minusAndUnderscore | equalAndPlus | openingBracketAndOpeningBrace | closingBracketAndClosingBrace | backslashAndPipe | nonUsHashmarkAndTilde | semicolonAndColon | apostropheAndQuote | graveAccentAndTilde | commaAndLessThanSign | dotAndGreaterThanSign | slashAndQuestionMark | capsLock | f1 | f2 | f3 | f4 | f5 | f6 | f7 | f8 | f9 | f10 | f11 | f12 | printScreen | scrollLock | pause | insert | home | pageUp | delete | end | pageDown | rightArrow | leftArrow | downArrow | upArrow | numLock | keypadSlash | keypadAsterisk | keypadMinus | keypadPlus | keypadEnter | keypad1AndEnd | keypad2AndDownArrow | keypad3AndPageDown | keypad4AndLeftArrow | keypad5 | keypad6AndRightArrow | keypad7AndHome | keypad8AndUpArrow | keypad9AndPageUp | keypad0AndInsert | keypadDotAndDelete | nonUsBackslashAndPipe | application | power | keypadEqualSign | f13 | f14 | f15 | f16 | f17 | f18 | f19 | f20 | f21 | f22 | f23 | f24 | execute | help | menu | select | stop | again | undo | cut | copy | paste | find | mute | volumeUp | volumeDown | lockingCapsLock | lockingNumLock | lockingScrollLock | keypadComma | keypadEqualSignAs400 | international1 | international2 | international3 | international4 | international5 | international6 | international7 | international8 | international9 | lang1 | lang2 | lang3 | lang4 | lang5 | lang6 | lang7 | lang8 | lang9 | alternateErase | sysreq | cancel | clear | prior | return | separator | out | oper | clearAndAgain | crselAndProps | exsel | keypad00 | keypad000 | thousandsSeparator | decimalSeparator | currencyUnit | currencySubUnit | keypadOpeningParenthesis | keypadClosingParenthesis | keypadOpeningBrace | keypadClosingBrace | keypadTab | keypadBackspace | keypadA | keypadB | keypadC | keypadD | keypadE | keypadF | keypadXor | keypadCaret | keypadPercentage | keypadLessThanSign | keypadGreaterThanSign | keypadAmp | keypadAmpAmp | keypadPipe | keypadPipePipe | keypadColon | keypadHashmark | keypadSpace | keypadAt | keypadExclamationSign | keypadMemoryStore | keypadMemoryRecall | keypadMemoryClear | keypadMemoryAdd | keypadMemorySubtract | keypadMemoryMultiply | keypadMemoryDivide | keypadPlusAndMinus | keypadClear | keypadClearEntry | keypadBinary | keypadOctal | keypadDecimal | keypadHexadecimal | leftControl | leftShift | leftAlt | leftGui | rightControl | rightShift | rightAlt | rightGui 
    KEYABBREV = mediaVolumeMute | mediaVolumeUp | mediaVolumeDown | mediaRecord | mediaFastForward | mediaRewind | mediaNext | mediaPrevious | mediaStop | mediaPlayPause | mediaPause 
    KEYABBREV = systemPowerDown | systemSleep | systemWakeUp 
    KEYABBREV = mouseBtnLeft | mouseBtnRight | mouseBtnMiddle | mouseBtn4 | mouseBtn5 | mouseBtn6
    ############
    #DEPRECATED#
    ############
    COMMAND = switchLayer LAYERID
    COMMAND = switchKeymapLayer KEYMAPID LAYERID

- Uncategorized commands:
  - `goTo <int>` will go to action index int. Actions are indexed from zero.
  - `repeatFor <register index> <adr>` - abbreviation to simplify cycles. Will decrement the supplemented register and perform `goTo` to `adr` if the value is still greater than zero. Intended usecase - place after command which is to be repeated with the register containing number of repeats.
  - `break` will end playback of the current macro
  - `write <custom text>` will type rest of the string. Same as the plain text command. This is just easier to use with conditionals...
  - `startMouse/stopMouse` start/stop corresponding mouse action. E.g., `startMouse move left`
  - `pressKey/holdKey/tapKey/releaseKey` Presses/holds/taps/releases the provided scancode. E.g., `pressKey mouseBtnLeft`, `tapKey LC-v` (Left Control + (lowercase) v), `tapKey CS-f5` (Ctrl + Shift + F5).
  - `noOp` does nothing - i.e., stops macro for exactly one update cycle and then continues.
  - `setLedTxt <time> <custom text>` will set led display to supplemented text for the given time. (Blocks for the given time.)
- Status buffer
  - `printStatus` will "type" content of error status buffer (256 or 1024 chars, depends on my mood) on the keyboard. Mainly for debug purposes.
  - `setStatus <custom text>` will append <custom text> to the error report buffer, if there is enough space for that.
  - `clearStatus` will clear the buffer.
  - `statsRuntime` will append information about runtime of current macro at the end of status buffer. The time is measured before the printing mechanism is initiated.
  - `statsLayerStack` will append information about layer stack at the end of status buffer. 
  - `statsPostponerStack` will print out information about postponer queue.
  - `statsActiveKeys` will print all active keys and their states.
  - `Diagnose` will deactivate all keys and macros and print diagnostic information into the status buffer.
- Delays:
  - `delayUntil <timeout>` sleeps the macro until timeout (in ms) is reached.
  - `delayUntilRelease` sleeps the macro until its activation key is released. Can be used to set action on key release. 
  - `delayUntilReleaseMax <timeout>` same as `delayUntilRelease`, but is also broken when timeout (in ms) is reached.
- Layer/Keymap switching:
  Layer/Keymap switching mechanism allows toggling/switching of keymaps of layers. We keep layer records in a stack of limited size, which can be used for nested toggling and/or holds.
  - special ids:
    - `previous` refers to the second stack record (i.e., `stackTop-1`)
    - `last` always refers to the previously used layer/keymap (i.e., `stackTop-1` or `stackTop+1`)
  - terminology:
    - switch means loading the target keymap and reseting layer-switching context
    - toggle refers to switching a layer and remaining there
    - hold refers to switching to a layer when the command is activated and switching back when the activation key is released
  - `switchKeymap` will load the keymap by its abbreviation and reset the stack.
  - `switchLayer/switchKeymapLayer` are deprecated. They simply push the layer onto stack (or pop in case of `previous`) without any further handling. Should be replaced by toggle/untoggle/hold layer commands.
  - `toggleLayer` toggles the layer. 
  - `unToggleLayer` pops topmost non-held layer from the stack. (I.e., untoggles layer which was toggled via "toggle" or "switch" feature.)
  - `toggleKeymapLayer` toggles layer from different keymap. 
  - `holdLayer LAYERID` mostly corresponds to the sequence `toggleLayer <layer>; delayUntilRelease; unToggleLayer`, except for more elaborate conflict resolution (releasing holds in incorrect order).
  - `holdKeymapLayer KEYMAPID LAYERID` just as holdLayer, but allows referring to layer of different keymap. This reloads the entire keymap, so it may be very inefficient.
  - `holdLayerMax/holdKeymapLayerMax` will timeout after <timeout> ms if no action is performed in that time.
  - `resolveSecondary <timeout in ms> [<safety margin delay in ms>] <primary action macro action index> <secondary action macro action index>` is a special action used to resolve secondary roles on alphabetic keys. The following commands are supposed to determine behaviour of primary action and the secondary role. The command takes liberty to wait for the time specified by the first argument. If the key is held for more than the time, or if the algorithm decides that secondary role should be activated, goTo to secondary action is issued. Otherwise goTo to primary action is issued. Actions are indexed from 0. Any keys pressed during resolution are postponed until the first command after the jump is performed. See examples. 
    
    In more detail, the resolution waits for the first key release - if the switch key is released first or within the safety margin delay after release of the postponed key, it is taken for a primary action and goes to the section of the "primary action", then the postponed key is activated; if the postponed key is released first, then the switcher branches the secondary role (e.g., activates layer hold) and then the postponed key is activated; if the time given by first argument passes, the "secondary" branch is activated as in the previous case.

    - `arg1` - total timeout of the resolution. If the timeout is exceeded and the switcher key (the key which activated the macro) is still being held, goto to secondary action is issued. Recommended value is 350ms.
    - `arg2` - safety margin delay. If the postponed key is released first, we still wait for this delay (or for timeout of the arg1 delay - whichever happens first). If the switcher key is released within this delay (starting counting at release of the key), the switcher key is still taken to have been released first. Recommended value is between 0 and `arg1`. If only three arguments are passed, this argument defaults to `arg1`.   
    - `arg3`/`arg4` - primary/secondary action macro action index. When the resolution is finished, the macro jumps to one of the two indices (I.e., this command is a conditional goTo.).
- Postponing mechanisms. We allow postponing key activations in order to allow deciding between some scenarios depending on the next pressed key and then activating the keys pressed in "past" in the newly determined context. The postponing mechanism happens in key state preprocessing phase - i.e., works prior to activation of the key's action, including all macros. Postponing mechanism registers and postpones both key presses and releases, but does not preserve delays between them. Postponing affects even macro keystate queries, unless the macro in question is the one which initiates the postponing state (otherwise `$postponeKeys delayUntilRelease` would indefinitely postpone its own release). Replay of postponed keys happens every `CYCLES_PER_ACTIVATION` update cycles, currently 2.The following commands either use this feature or allow control of the queue.
  - `postponeKeys` modifier prefixed before another command keeps the firmware in postponing mode. Once no instance of postponeKeys modifer is active, the postponer will start replaying the keys. Replaying happens with normal event loop, which means that postponed keys will be replayed even during macro execution (most likely after next macro action). Some commands (thos from this section) apply this modifier implicitly. See MODIFIER section. 
  - `postponeNext <n>` command will apply `postponeKeys` modifier on the current command and following next n commands (macro actions).
  - `ifPending/ifNotPending <n>` is true if there is at least `n` postponed keys in the queue.
  - `ifPendingId/ifNotPendingId <idx> <keyId>` looks into postponing queue at `idx`th waiting key nad compares it to the `keyId`. 
  - `consumePending <n>` will remove n records from the queue.
  - `resolveSecondary` allows resolution of secondary roles depending on the next key - this allows us to accurately distinguish random press from intentional press of shortcut via secondary role. See `resolveSecondary` entry under Layer switching. Implicitly applies `postponeKeys` modifier.
  - `ifShortcut/ifNotShortcut [KEYID]*` will wait for next keypresses until the key is released. If the next keypresses correspond to the provided arguments (hardware ids), the keypresses are consumed and the condition is performed. This is shorter and simpler version of `resolveNextKeyEq`. E.g., `ifShortcut 090 089 final tapKey C-V; holdKey v`.
  - `ifGesture/ifNotGesture [KEYID]*` just as `ifShortcut`, but breaks after 500ms instead of when the key is released.
  - `resolveNextKeyEq <queue idx> <key id> <timeout> <adr1> <adr2>` will wait for next (n) key press(es). When the key press happens, it will compare its id with the `<key id>` argument. If the id equals, it issues goto to adr1. Otherwise, to adr2. See examples. Implicitly applies `postponeKeys` modifier.
    - `arg1 - queue idx` idx of key to compare, indexed from 0. Typically 0, if we want to resolve the key after next key then 1, etc.
    - `arg2 - key id` key id obtained by `resolveNextKeyId`. This is static identifier of the hardware key.
    - `arg3 - timeout` timeout. If not enough keys is pressed within the time, goto to `arg5` is issued. Either number in ms, or `untilRelease`.
    - `arg4 - adr1` index of macro action to go to if the `arg1`th next key's hardware identifier equals `arg2`.
    - `arg5 - adr2` index of macro action to go to otherwise.
  - `resolveNextKeyId` will wait for next key press. When the next key is pressed, it will type a unique identifier identifying the pressed hardware key. 
- Conditions are checked before processing the rest of the command. If the condition does not hold, the rest of the command is skipped entirelly. If the command is evaluated multiple times (i.e., if it internally consists of multiple steps, such as the delay, which is evaluated repeatedly until the desired time has passed), the condition is evaluated only in the first iteration.  
  - `ifDoubletap/ifNotDoubletap` is true if previous played macro had the same index and finished at most 250ms ago
  - `ifInterrupted/ifNotInterrupted` is true if a keystroke action or mouse action was triggered during macro runtime. Allows fake implementation of secondary roles. Also allows interruption of cycles.
  - `ifReleased/ifNotReleased` is true if the key which activated current macro has been released. If the key has been physically released but the release has been postponed by another key, the macro returns false. If the key has been physically and postponing mode was initiated by this macro (e.g., `postponeKeys ifReleased goTo @2`), it returns non-postponed release state (i.e., if there's matching release event in the queue).
  - `ifPending/ifNotPending <n>` is true if there is at least `n` postponed keys in the postponing queue. In context of postponing mechanism, this condition acts similar in place of ifInterrupted. 
  - `ifPendingId/ifNotPendingId <idx> <keyId>` looks into postponing queue at `idx`th waiting key nad compares it to the `keyId`. See `resolveNextKeyId`.
  - `ifPlaytime/ifNotPlaytime <timeout in ms>` is true if at least `timeout` milliseconds passed since macro was started.
  - `ifShift/ifAlt/ifCtrl/ifGui/ifAnyMod/ifNotShift/ifNotAlt/ifNotCtrl/ifNotGui/ifNotAnyMod` is true if either right or left modifier was held in the previous update cycle.
  - `{ifRegEq|ifNotRegEq} <register inex> <value>` pull test if the value in the register identified by first argument equals second argument.
  - `ifRecording/ifNotRecording` and `ifRecordingId/ifNotRecordingId <macro id>` test if the runtime macro recorder is in recording state. 
  - `ifShortcut/ifNotShortcut [KEYID]*` will wait for next keypresses and compare them to the argument. See postponer mechanism section.
  - `ifGesture/ifNotGesture [KEYID]*` just as `ifShortcut`, but breaks after 500ms instead of when the key is released.
- `MODIFIER`s modify behaviour of the rest of the keyboard while the rest of the command is active (e.g., a delay) is active.
  - `suppressMods` will supress any modifiers except those applied via macro engine. Can be used to remap shift and nonShift characters independently.
  - `suppressKeys` will suppress all new key activations triggered while this modifier is active. 
  - `postponeKeys` will postpone all new key activations for as long as any instance of this modifier is active. See postponing mechanisms section.
  - `final` will end macro playback after the "modified" action is properly finished. Simplifies control flow. "Implicit break."
- Runtime macros:
  - `recordMacro|playMacro <slot identifier>` targets vim-like macro functionality. Slot identifier is a single character. Usage (e.g.): call `recordMacro a`, do some work, end recording by another `recordMacro a`. Now you can play the actions (i.e., sequence of keyboard reports) back by calling `playMacro a`. Only BasicKeyboard scancodes are available at the moment. These macros are recorded into RAM only. Number of macros is limited by memory (current limit is set to approximately 500 keystrokes (4kb) (maximum is ~1000 if we used all available memory)). If less than 1/4 of dedicated memory is free, oldest macro slot is freed. If currently recorded macro is longer than 1/4 of dedicated memory, recording is stopped and the macro is freed (prevents unwanted deletion of macros).
  - `recordMacroDelay` will measure time until key release (i.e., works like `delayUntilRelease`) and insert delay of that length into the currently recorded macro. This can be used to wait for window manager's reaction etc. 
  - `stopRecording` will stop recording the current macro
- Registers - for the purpose of toggling functionality on and off, and for global constants management, we provide 32 numeric registers (namely of type int32_t). 
  - `setReg <register index> <value>` will set register identified by index to value.
  - `ifRegEq|ifNotRegEq` see CONDITION section
  - `{addReg|subReg|mulReg} <register index> <value>` adds value to the register 
  - Register values can also be used in place of all numeric arguments by prefixing register index by '#'. E.g., waiting until release or for amount of time defined by reg 1 can be achieved by `$delayUntilReleaseMax #1`
- Global configuration options:
  - `setStickyModsEnabled` globally turns on or off sticky modifiers
  - `setSplitCompositeKeystroke {0|1}` If enabled, composite keystrokes (e.g., Ctrl+c sent by a single key) are separated into distinct usb reports. This makes order of keypresses clearly determined. Enabled by default.
  - `setKeystrokeDelay <time in ms, at most 250>` will stop event processing for the specified time after every usb report change. May be used to slow down macroes, to insert delay between composite keystrokes. Beware, this does not queue keypresses - if the delay is too long, some keypresses may be skipped entirelly!
  - `setDebounceDelay <time in ms, at most 250>` prevents key state from changing for some time after every state change. This is needed because contacts of mechanical switches can bounce after contact and therefore change state multiple times in span of a few milliseconds. Official firmware debounce time is 50 ms for both press and release. Recommended value is 10-50, default is 50.
  - `setActivateOnRelease {0|1}` **experimental** if turned on, key actions are activated just once upon key release. This is a highly experimental feature, not guaranteed to work properly with all features of the keyboard! Intended usecase - if you wish to see whether or not you release keys in proper order. 
- Argument parsing rules:
  - `NUMBER` is parsed as a 32 bit signed integer and then assigned into the target variable. However, the target variable is often only 8 or 16 bit unsigned. If a number is prefixed with '#', it is interpretted as a register address (index). If a number is prefixed with '@', current macro index is added to the final value. `#key` returns activation key's hardware id.
  - `Keymap abbrev` is assumed to be 3 characters long abbreviation of the keymap.
  - `macro slot identifier` is either a number or a single ascii character (interpretted as a one-byte value). `#key` can be used so that the same macro refers to different slots when assigned to different keys.
  - `register index` is an integer in the appropriate range, used as an index to the register array.
  - `custom text` is an arbitrary text starting on next non-space character and ending at the end of the text action.
  - `KEYID` is a numeric id obtained by `resolveNextKeyId` macro.
  - `SHORTCUT` is an abbreviation of a key possibly accompanied by modifiers. Describes at most one scancode action. Can be prefixed by `C/S/A/G` denoting `Control/Shift/Alt/Gui`. Mods can further be prefixed by `L/R`, denoting left or right modifier. If a single ascii character is entered, it is translated into corresponding key combination (shift mask + scancode) according to standard EN-US layout. E.g., `pressKey mouseBtnLeft`, `tapKey LC-v` (Left Control + (lowercase) V (scancode)), `tapKey CS-f5` (Ctrl + Shift + F5), `tapKey v` (V), `tapKey V` (Shift + V).

## Error handling

This version of firmware includes basic error handling. If an error is encountered, led display will change to `ERR` and error message written into the status buffer. Error log can be retrieved via the `$printStatus` command. (E.g., focus some text area (for instance, open notepad), and press key with corresponding macro).)

## Known issues/limitations

- Layers can be untoggled only via macro or "toggle" feature. The combined hold/doubletap will *not* release layer toggle (this is bug of the official firmware, waiting for reply from devs).  
- Generally, interaction of legacy layer switching mechanisms with the newly implemented ones is not very well tested. Therefore, we advise you to use only one of the mechanisms in each of your layer systems. Of course, you are welcome to test and report problems.
- Only one-liners are allowed, due to our need to respect firmware's indexation of actions.
- Global settings and recorded macros are remembered until power cycling only. 

## Performance impact and other statistics

Some measurements:

- Typical update cycle without running macros takes approximately 2 ms.
- With four macros running, the update cycle takes around 4 ms. (The parsing mechanism is stateless, therefore eating up performance on every update cycle.)
- According to my measurements, typical key tap takes between 90 and 230 ms, with quite large variation (i.e., full range is encountered when writing regularly). Currently, debouncing delay is set to 50 ms, which means that after any change of state, the state is prevented from changing for the next 50 ms. This means that one key tap cannot last less than 50 ms with UHK (except for macro-induced taps and secondary roles). It also means that a key cannot be repeated faster than once per 100ms (in ideal conditions).
- According to my experience, 250ms is a good double-tap delay trashold. 
- According to my experience, 350ms is a good trashold for secondary role activation. I.e., at this time, it can be safely assumed that the key held was prolonged at purpose. 

## Contributing

If you wish some functionality, feel free to fire tickets with feature requests. If you wish something already present on the tracker (e.g., in 'idea' tickets), say so in comments. (Feel totally free to harass me over desired functionality :-).) If you feel brave, fork the repo, implement the desired functionality and post a PR.

## Adding new features

Practically all high-level functionality of the firmware is implemented in the following files:

- `usb_report_updater.c` - logic of key activation, layer switching, debouncing, etc.. Almost all important stuff is here.
- `layer.c` - some suport for "hold" layer switching (beware, there are two independent layer switching mechanisms and this one is the less important one).
- `keymap.c` - keymap switching functions.
- `macros.c` - the macro engine. 

We furthermore add the following:
- `macro_recorder.c` - includes the actual recorder of runtime macros.
- `postponer.c` - contains simple circular buffer which keeps track of postponed keys. This is vital for proper function of postponed secondary roles. 
- `str_utils.c` - contains functions used for handling strings
- `macro_shortcut_parser.c` - contains lookup tables and algorithms used to transform strings to scancodes.

Our command actions are rooted in `processCommandAction(...)` in `macros.c`.

If you have any questions regarding the code, simply ask (via tickets or email).

## Building the firmware

If you want to try the firmware out, just download the tar in releases and flash it via Agent. 

If you wish to make changes into the source code, please follow the official repository guide. Basically, you will need:

- Clone the repo with `--recursive` flag.
- Build agent in lib/agent (that is, unmodified official agent), via `npm install && npm run build` in repository root. While doing so, you may run into some problems:
  - You may need to install some packages globally (I am afraid I no longer remember which ones).
  - You may need to downgrade npm: `sudo npm install -g n && sudo n 8.12.0`
  - You will need to commit changes made by npm in this repo, otherwise, make-release.js will be faililng later.
- Then you can setup mcuxpressoide according to the official firmware README guide.
- Now you can build and flash firmware either:
  - Via mcuxpressoide (debugging probes are not needed, see official firmware README).
  - Or via running scripts/make-release.js (run by `node make-release.js`) and flashing the resulting tar.bz2 through agent.
  
If you have any problems with the build procedure, please create issue in the official agent repository. I made no changes into the proccedure and I will most likely not be able to help.



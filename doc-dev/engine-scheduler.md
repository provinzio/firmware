# History of the engine:

 - Single-state engine: At first, macro engine executed at most one macro at a time, not even composing with other actions. Executing macro would mean going back and forth between main update loop, and the macro engine. Macro engine would always progress the macro by one atomic step - maybe one action, maybe less than a single action, keeping its state in static variables. After each call, action would return a boolean indicating whether the action finished or not (yet).

   (Macro result: bool indicating either InProgress or ActionFinished)

 - Preemptive scheduler: Single state was quite limiting, so I introduced a state pool, and moved all statefull information of the engine into these states. As a result, each macro was allowed to execute once during each update cycle. This allowed multiple macros running at a time, and made commands like `holdLayer` or `holdKey` useful. However, free action interleaving caused macros to be prone to race conditions in user logic.

   (Macro result: still bool indicating either InProgress or ActionFinished)

 - Blocking scheduler: So the macro engine was redesigned so that only one macro state is active at a time, allowing sequences of commands to execute atomicaly. Delay actions/commands and backward jumps would automatically yield, allowing other macros to take turns.

   (Macro result: current flag system described below)

# Flags:
 - InProgress, ActionFinished, Done (exactly one of these should be set):

   - InProgress means that an action/command is not finished and should be called again.

   - ActionFinished means that action/command has finished. This flag indicates that the outernmost engine call should take care of action/command epilogue, that is, to load next action/command and do any other necessary processing.

   - Done is a special flag indicating that action/command epilogue was already done. This is done by some special commands such as `goTo` which modify the state of the engine.

 - Yield flag indicates that current macro does no longer wish to block others from running. It is returned by delay behaviours (`holdKey, delayUntil, holdLayer,...`) and by backward jumps (because active wait loops).

- Blocking indicates that USB report output needs to be processed.

Generally, macro engine returns when either all macros yield, or when a blocking flag is returned, or when execution quota is reached.

# Macro sleeping:

- Since macro parsing is stateless, every command has to be reparsed on each invocation. As a consequence, commands like holdLayer can slow down keyboard execution significantly, especially hindering cursor movement. For this reason, macro sleeping was introduced.

- The idea is that macro can be put to sleep until either some key state changes, or until specified times. Similar to the concept of a *discrete* event calendar.

- The macro is then not executed until the condition is met. Spurious wakeups are allowed, so macro command implementation should always check whether the condition is actually met, and put the macro to sleep again otherwise. (For instance macro may want to sleep until its own key is released. In practice it is also woken up when another key's state changes.)

- Macro sleeping is also used with the `call` command to wait until the child macro has finished executing.

# Cyclic scheduler:

When new macros are spawned (e.g., via `call` or `fork` commands), their execution order has to be determined. At the moment, each macro state contains a reference to the next macro that is going to be executed, forming a cyclic single-linked list. Further details are stored in an instance of scheduler_state_t.

Thanks to this system, newly spawned macros can be queued right after execution of current macro. (Or more precisely - at the and of a queue beginning at the currently executed macro.)

In theory, this mechanism yields nice determinism in the actual execution. In practice, it is a nightmare to maintain/debug, and maybe is not worth the benefits.


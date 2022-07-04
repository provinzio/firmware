# UHK Firmware with Extended macro engine

This fork of UHK firmware extends the keyboard with a "simple" macro language that allows users to customize behaviour of the UHK from user space.

These include things like:
- macro commands to switch keymaps (`switchKeymap`) or layers (`holdLayer`, `toggleLayer`), activate keys (`holdKey`, `tapKey`),
- some basic conditionals (e.g., `ifCtrl`, `ifDoubletap`), allowing to mimic secondary roles and various timeout scenarios
- super-simple control flow (basically `goTo` instruction), and commands to activate other macros (`call`, `fork`, `exec`)
- many configuration options which are not directly exposed by Agent (`set`)
- runtime macro recorder implemented on scancode level, for vim-like macro functionality

If you want to give it a try, you should continue at.

- [Getting started](doc-dev/user-guide.md)
- [Reference manual](doc-dev/reference-manual.md)

# Other notes

## Contributing

If you wish some functionality, feel free to fire tickets with feature requests. If you wish something already present on the tracker (e.g., in 'idea' tickets), say so in comments. (Feel totally free to harass me over desired functionality :-).) If you feel brave, fork the repo, implement the desired functionality and post a PR.

## Adding new features

The key file is `usb_report_updater.c` and its `UpdateUsbReports` function. All keyboard logic is driven from here.

Our command actions are rooted in `processCommandAction(...)` in `macros.c`.

If you have any questions regarding the code, simply ask (via tickets or email).

## Building the firmware

If you want to try the firmware out, just download the tar in releases and flash it via Agent.

If you wish to make changes into the source code, then you indeed need to build your own firmware:

- Clone the repo with `--recursive` flag.
- Build agent in lib/agent (that is, unmodified official agent), via `npm install && npm run build` in repository root. While doing so, you may run into some problems:
  - You may need to remove `node_modules` directory for number of unintuitive reasons. E.g., if things just stopped working out of nothing.
  - You may need to run `npm install` and maybe `npm run build` in various directories - in such cases, it is usually noted in their README.md
  - You may need to install some packages globally.
  - You may need to downgrade or upgrade npm: `sudo npm install -g n && sudo n 8.12.0`
  - You may need to commit changes made by npm in this repo, otherwise, make-release.js will be faililng later.
  - You may need to offer some sacrifice the node.js gods.
- Then you can setup mcuxpressoide according to the official firmware README guide. (Optionaly - any C-capable editor of choice will work just fine.)
- Now you can build and flash firmware either: (No special equipment is needed.)
  - `make` / `make flash` in `right/uhk60v1`.
  - Or via mcuxpressoide (debugging probes are not needed, see official firmware README).
  - Or via running scripts/make-release.js and flashing the resulting tar through agent.

If you have any problems with the build procedure (especially witn npm), please create issue in the official agent repository. I made no changes into the proccedure and I will most likely not be able to help.

## Philosophy

- Functionality of the firmware should consist of small but generic building blocks which can be combined to support new usecases.

- The engine is designed to be super simple and have very small memory footprint.

- The aim of the command interface is mostly to provide direct interface between the user and experimental new features in the firmware. (That is, to bypass the fuss of having to implement new user interface, extending two parses, three different config formats, and ensuring compatibility of it all.) My goal never was to provide a strong scripting language. In this light, I hope that lack of scoping, reasonable control flow, named variables or shunting yard expressions is somewhat understandable.





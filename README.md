# Grimoire for Unreal Engine

Connect your Unreal Engine project to Grimoire. Import game data into native Unreal assets, link it to Actors, and use localized text, game variables, and dialogs through Blueprints or C++.

Imported data is stored locally, allowing your game to use it offline.

## Supported versions

Prebuilt Windows packages are available for:

- **Unreal Engine 5.8.3**
- **Unreal Engine 5.7.4**

Download the package matching your engine version. Building from source requires a compatible C++ compiler and Windows SDK.

## Installation

1. Close Unreal Editor.
2. Download and extract the package for your engine version.
3. Copy the `Grimoire` folder into your project's `Plugins` folder. Create that folder if needed.
4. Verify the plugin descriptor is located at:
   `<YourProject>/Plugins/Grimoire/Grimoire.uplugin`
5. Open your project and enable **Grimoire** under **Edit → Plugins** if necessary.
6. Open **Window → Grimoire Connect**.

## Getting started

1. Sign in with your Grimoire account.
2. Select your company and game.
3. Click **Import / update game**.

The plugin creates a database asset containing your imported data. Reference this asset from an Actor, Widget, or another cooked asset to include it in your packaged game.

## Features

- **Game data:** Import objects and read typed fields and references from Blueprints or C++.
- **Actor linking:** Associate level Actors with Grimoire objects.
- **Localization:** Use imported strings and translations in widgets and world text.
- **Dialog playback:** Run branching dialogs with choices, conditions, variables, and setters.
- **Custom dialog UI:** Create Widget Blueprints for dialog layouts and answer buttons.
- **Game variables:** Share runtime values between gameplay systems and dialogs.
- **Speaker portraits:** Display portraits that react to character mood.
- **Editor synchronization:** Submit supported field and Actor transform changes to Grimoire's review queue.
- **Offline runtime:** Use imported data without contacting the Grimoire API during gameplay.

## Blueprints and C++

The plugin provides Blueprint-accessible components, widgets, and a Game Instance subsystem.

For C++ integration, add `GrimoireRuntime` to your game module's dependencies.

## Build validation

The UE 5.8.3 Windows package passed:

- Editor Development compilation
- Game Development compilation
- Game Shipping compilation
- All 15 Grimoire automation tests

Live account integration and project-specific gameplay should also be tested in your own project.

## Notes

- Prebuilt binaries are specific to the engine version named on each package.
- Building or packaging a game requires a supported C++ toolchain.
- Runtime changes affect local game state; they do not automatically update data in Grimoire.
- Re-import your game to refresh its local data snapshot.
- Credentials remain in memory. Sign in again after restarting the editor.

See the README included with the plugin for detailed setup and usage instructions.

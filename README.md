# Truck Pass Through

A Satisfactory 1.2 mod that stops truck jams. Self-driving trucks pass through each other and no longer wait at junctions. Near truck stations they keep vanilla behavior, so only one truck docks at a time and the others queue behind it.

Status: **0.1.0, untested**. It has not been compiled or run yet. See "Known unknowns" below.

## What it does

In 1.2, self-driving trucks wait for each other in two ways:

1. **Vehicle avoidance.** A truck slows down for, or stops behind, the vehicle ahead.
2. **Junction reservations.** Like train block signals, a truck books the junction blocks ahead and waits at the line until they are free.

Away from truck stations, this mod turns both off. A truck counts as "near a station" when it is docked, or within `TPT.StationRadius` (default 30 m) of a station's docking point. Near a station, both rules work as in vanilla.

The mod also turns off physical collision between wheeled vehicles (trucks, tractors, explorers), so they pass through each other cleanly even when you are standing next to them. This includes vehicles you drive yourself. Collision with terrain, buildings and players is unchanged.

## Console commands

Open the console with `~`.

| Command | Default | Effect |
|---|---|---|
| `TPT.Enabled 0/1` | 1 | 0 restores vanilla waiting at junctions and behind other trucks |
| `TPT.Ghost 0/1` | 1 | 0 restores vanilla physical collision between vehicles |
| `TPT.StationRadius <cm>` | 3000 | How close to a station's docking point a truck keeps vanilla queueing |
| `TPT.Status` | | Prints station count, how many autopilot trucks are ghosting vs queueing, vehicle counts by class, and how often each hook has removed a stop. Then lists every self-driving truck that is standing still: whether it is a ghost or vanilla (and why), and what it is braking for. The same block is written to the log every 30 s |
| `TPT.Autopilot on/off [name filter]` | | Turns self-driving on or off for every vehicle at once, or only for those whose name contains the filter. Host or single player only |

In chat, `/trucks on`, `/trucks off` and `/trucks off <name filter>` do the same as `TPT.Autopilot`. Chat commands run on the server, so any player on a dedicated server can use them.

Settings reset to the defaults when the game restarts.

## How it works

All hooks use SML native hooking (`Source/TruckPassThrough/Private/TPTHooks.cpp`):

| Game function (`UFGVehicleAutopilotComponent`) | What the mod does for a truck away from stations |
|---|---|
| `CalculateVehicleAvoidanceTarget` | Skipped, so there is no stop target behind the vehicle ahead |
| `CalculatePathReservationStopTarget` | Skipped, so there is no stop at junction lines |
| `ReserveVehiclePathBlocks_Parallel` | Skipped, so the truck books nothing and never blocks a truck queueing at a station |
| `UpdateVehicleDeadlockStatus_Parallel` | Its wait timer is held at zero, so it is never flagged "Deadlocked" |

A post-hook on `AFGVehicleSubsystem::TickVehicleAutopilot` runs on the game thread after the parallel autopilot tick. It does two things:

- Refreshes the list of station docking points every 5 s.
- Once a second, sets every vehicle collision component (and each truck's distant-view proxy) to ignore the Vehicle channel, plus whatever channel the vehicle bodies use.

`Config/AccessTransformers.ini` gives the hook class access to the protected game members. Because of this, building the mod also rebuilds FactoryGame, which is slow.

Hooks are not installed in the Unreal editor, because the editor's FactoryGame has empty function bodies. The mod only does anything in the packaged game.

The mod only needs to be installed where the game simulation runs: your own game in single player, or the host or dedicated server in multiplayer. In multiplayer, clients would also need it for the physical pass-through part.

## Building

Windows only. Unreal and the Satisfactory toolchain do not support macOS. The full guide is at https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/index.html. This is the short version, current for SML 3.12, Unreal Engine 5.6.1-CSS and Satisfactory 1.2.

### One-time setup (2 to 4 hours, mostly downloads)

1. Install Satisfactory and launch it once.
2. Install **Visual Studio 2022, version 17.14**. Do not use VS 2026.
   - In the Visual Studio Installer, choose "Import configuration".
   - Use [`SML.vsconfig`](https://docs.ficsit.app/satisfactory-modding/latest/_attachments/BeginnersGuide/dependencies/SML.vsconfig). It includes the MSVC 14.38 toolset, which is listed as out of support but is required.
3. Get access to the custom Unreal Engine:
   - Link your GitHub account to Epic at https://www.unrealengine.com/en-US/ue-on-github, and accept the GitHub org invite.
   - Link at https://linker.ficsit.app/link.
4. From https://github.com/satisfactorymodding/UnrealEngine/releases/latest, download `UnrealEngine-CSS-Editor-Win64.exe` and its `-1.bin`, `-2.bin` and `-3.bin` files into one folder. Run the `.exe`.
5. Install **Wwise 2025.1.6.9117** from the Audiokinetic launcher. Select:
   - Authoring, and SDK (C++).
   - Platforms: Windows (VS2019, VS2022, Game Core) and Linux.
   - No plugins.
6. Clone the starter project to a short path. Avoid OneDrive and non-English characters in the path.
   ```
   git clone -b master https://github.com/satisfactorymodding/SatisfactoryModLoader.git C:\SML
   ```
7. Integrate Wwise:
   - In the Wwise launcher, open the Unreal Engine tab and choose "Open other". Select `C:\SML\FactoryGame.uproject`.
   - Click "Integrate Wwise in Project". Set version 2025.1.6.9117, Integration Version "All", and "New project".
   - Open the generated `.wproj` and run "Generate Soundbank(s) for all platforms".
   - Never click any Wwise "update" buttons.
8. Clone this mod into the starter project's Mods folder:
   ```
   git clone https://github.com/nikgavalas/TruckPassThrough.git C:\SML\Mods\TruckPassThrough
   ```
9. Build the editor:
   - Right-click `FactoryGame.uproject` and choose "Generate Visual Studio project files".
   - Open `FactoryGame.sln` and select **Development Editor** / **Win64**.
   - In Solution Explorer, right-click **Games > FactoryGame** and choose **Build**. Do not use "Build Solution". The first build takes a long time.
10. Open `FactoryGame.uproject`. Shaders compile on first launch, which is also slow.
    - In Project Settings > Wwise, set the GeneratedSoundBanks path.
11. Set up Alpakit:
    - Open it with the alpaca toolbar button, or File > Alpakit Dev.
    - Under Windows, check Enabled and "Copy to Game Path", and set your Satisfactory install folder.
    - Click **Alpakit!** next to SML once to install the mod loader into the game. Installing SML with Satisfactory Mod Manager also works.

### Building and installing the mod

1. In Alpakit, click **Alpakit!** next to TruckPassThrough. This builds the Shipping game target and copies the result to `<Satisfactory>\FactoryGame\Mods\TruckPassThrough`.
2. Launch the game and load a save.
3. Open `%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log`. Search for `LogTruckPassThrough`. You should see `hooks installed`, then one line per vehicle collision component.

To uninstall, delete the `Mods\TruckPassThrough` folder from the game.

## Testing checklist

- Two truck routes cross at a junction. The trucks drive through it at the same time and through each other.
- Stand right next to that junction. Overlapping trucks do not shove, jitter or flip. Repeat with `TPT.Ghost 0` to compare.
- A fast truck catches a slow one on an open road and passes through it.
- Two trucks share a station. The second one waits behind the docked one and docks after it leaves.
- After 3 or more minutes, no truck shows "Deadlocked".
- `TPT.Enabled 0` brings back vanilla behavior. `TPT.Status` prints sensible numbers.
- Everything still works after saving and reloading.

## Known unknowns

The game's function bodies are not public. These points come from the headers and need confirming in-game:

- **Inlining.** Checked by disassembling the shipped game DLL: every hooked function is called as a real function, none is inlined. `TPT.Status` also shows a counter per hook. A counter that stays at 0 while trucks drive means that hook never runs.
- **Other mods.** BetterJunctions hooks `ReserveVehiclePathBlocks_Parallel` too, and changes how trucks near stations queue. Disable it when testing this mod.
- **Other waiting.** Skipping the reservation stop target may not be the only thing that makes a truck wait at a junction.
- **Collision channel.** The vehicle collision channel is assumed to be `ECC_Vehicle`. The log lines from step 3 above show the real one.
- **Game version.** `GameVersion` is set to `>=513885`, the build the current SML headers come from. An older game build will refuse to load the mod.

## Credits

Hook points were found by reading the FactoryGame headers shipped with the [SML starter project](https://github.com/satisfactorymodding/SatisfactoryModLoader). [BetterJunctions](https://github.com/frostmx/BetterJunctions) by frostmx was a useful reference for how the 1.2 truck autopilot can be hooked. No code was copied from it.

## License

MIT, see [LICENSE](LICENSE).

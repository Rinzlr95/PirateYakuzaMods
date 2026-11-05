# Pirate Yakuza Mods

Source for **Cursed Encounters - Pirate Affixes** and the flotsam mods.

## Pirate Affixes

Random traits for on-foot enemies, minibosses, bounties and bosses. Two or more traits add the **Elite** title.

- **Strong:** more HP and harder hits.
- **Speedy:** faster animations.
- **Regen:** heals over time.
- **Berserk:** harder hits below its health threshold.
- **Enraged:** faster at low HP, but takes more damage.
- **Heavyweight:** tougher and slower, with armor against hit reactions.
- **Volatile:** a delayed fiery explosion after defeat.

### Install

1. Install **ShinRyuModManager with Parless**.
2. With the game closed, extract the mod folder into `runtime\media\mods`. Both files should be here:

   ```text
   runtime\media\mods\PirateAffixes\PirateAffixes.asi
   runtime\media\mods\PirateAffixes\PirateAffixes.ini
   ```

3. Enable **PirateAffixes** in ShinRyuModManager, regenerate the load order, and launch the game.

If updating from a manual install, remove the old `PirateAffixes.asi` beside the game EXE so it cannot load twice. Disable older affix prototypes.

Requires the supported Windows Steam 1.14 build; English game text.

**F8** opens settings. **F7** toggles HP numbers. Use borderless/windowed mode. The settings panel does not pause the game.

Settings save to the INI. Chance and HP changes affect new enemies; other settings update live. Reward values and drop chance are under General. Defaults are $500 per trait and 100% drop chance, combined into one cash pickup. Works with the tested x10 money-drop table. Original drops stay unchanged.

To remove, close the game, disable PirateAffixes in ShinRyuModManager and regenerate the load order. You can then delete its folder from `mods`.

## Flotsam mods

**FlotsamLoot2x** doubles items collected from floating crates. Spawn rates, loot chances and selling prices stay unchanged. Use ShinRyuModManager/Parless, place the included mod folder in `runtime\media\mods`, enable it, regenerate the load order and restart. Conflicts with other `db.spr.en\reward_table.bin` overrides.

The earlier **Flotsam2x Experimental** changes spawn density instead. Its overrides are `pirate_sea_ingame_parameter.bin` and `pirate_sea_driftage.bin`.

Build either ZIP with Python 3 and your own `db.spr.en.par`:

```sh
python Flotsam/build_flotsam_loot.py /path/to/db.spr.en.par FlotsamLoot2x.zip
python Flotsam/build_flotsam.py /path/to/db.spr.en.par Flotsam2x.zip
```

The builders check the supported archive hash before patching. Game files are not included.

<details>
<summary>Build instructions and source details for Nexus review</summary>

### Build 0.2.3

Download **Zig 0.14.1** from [ziglang.org/download](https://ziglang.org/download/). MinHook is included; no game files or extra libraries are needed to compile.

Clone this repository, then build on Windows:

```powershell
git clone https://github.com/Rinzlr95/PirateYakuzaMods.git
cd PirateYakuzaMods\PirateAffixes
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Zig 'C:\Tools\zig-0.14.1\zig.exe'
```

Replace the Zig path with your extracted `zig.exe`. `Bypass` applies only to this PowerShell process. Output: `PirateAffixes/build/PirateAffixes.asi`. The script prints its SHA-256.

On Linux, from `PirateAffixes/`:

```sh
ZIG=/absolute/path/to/zig sh build.sh
sha256sum build/PirateAffixes.asi
```

The Linux cross-build matches the original 0.2.3 ASI byte for byte:

```text
8aa0783a3a7f8506a8af2d4c657ac5095575fe471613aa15168a9d7914438174
```

The Windows script uses the same flags but has not been run here. `build.sh` and `build.ps1` contain the full compiler commands. To run the calculation tests on Windows:

```powershell
C:\Tools\zig-0.14.1\zig.exe cc -std=c11 affix_logic_tests.c -o build\affix_logic_tests.exe
.\build\affix_logic_tests.exe
```

### For reviewers

`PirateAffixes.c` contains the runtime hooks; `affix_logic.h` contains trait calculations; `settings_panel.h` contains the UI; `target_signatures.h` checks game functions. MinHook source and its licence are in `PirateAffixes/minhook/`.

The ASI hooks and reads/writes its own game process, calls native game effects/reward functions, checks the game EXE's MD5, and writes its INI/log beside the ASI. It has no network or updater code. It does not modify game archives. The compiled C sources match the original 0.2.3 release.


</details>

Thanks to ShinRyuModManager, Parless, Tsuda Kageyu for MinHook, the community tool developers, and everyone who helped test the mods. DragonEngine.NET was consulted as a reference.

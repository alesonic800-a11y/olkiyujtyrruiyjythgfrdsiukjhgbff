# Sushi the Cat — Nintendo 3DS native project

Native `libctru` homebrew platform-game reconstruction. It does not embed or redistribute the original GBA ROM.

## Build

Build with devkitPro/devkitARM and libctru installed:

```sh
make clean
make 3dsx
```

The GitHub Actions workflow uses `devkitpro/devkitarm:latest` and uploads `sushi3ds.3dsx` and `sushi3ds.elf` as an artifact.

## Controls

- Title: LEFT/RIGHT selects a world; A starts; B exits.
- Game: LEFT/RIGHT move; A jumps; UP meows (also damages a nearby boss); DOWN ducks; START pauses.
- Pause: A resumes; B or SELECT returns to world select.
- Boss defeated: A advances to the next world; B returns to world select.
- The 3D slider adjusts stereo separation.

## Gameplay systems

- Six themed worlds, scrolling backgrounds and multiple one-way platforms at different heights, with gaps that must be jumped.
- Falling into a pit costs a life and returns the player to the previous section.
- Sushi has idle, run, jump, duck and meow animation frames from the generated character sheet.
- Eight common enemy sprites and six distinct boss sprites are rendered from the generated atlas crops.
- Stars, score, lives, boss health, campaign progression, procedural music and sound effects.

## Assets

`assets/` contains the six world background PNGs, original sprite atlas, categorized sprite sheets and `sushi_assets.h`, a generated C pixel-data header used at runtime. `world_backgrounds.h` contains RGB565 background data generated from the six world images.

## Verification note

The source was checked with a host C syntax pass using a minimal libctru API stub. That catches C syntax and missing generated asset symbols, but it is **not** a substitute for compiling with devkitARM or testing on a physical 3DS/emulator. The GitHub Actions workflow performs the actual devkitARM build when run.

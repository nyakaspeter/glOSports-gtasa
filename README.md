# glOSports-gtasa — GTA San Andreas, native, on the GA36-MB

Grand Theft Auto: San Andreas through Rockstar's Android build (War Drum
Studios, 2013): a loader (`loader/`) maps the game's own `libGTASA.so`
(armeabi-v7a), resolves its imports to goodluckOS's glibc, ARM's Mali GLES 2.0
driver (from the folder's `glos/`, see `goodluckOS-mali-driver`), OpenAL Soft and
SDL's pad, answers its Java calls through a fake JNI, and runs it, the way
[gtasa_vita](https://github.com/TheOfficialFloW/gtasa_vita) (MIT) does on the
Vita. The loader core is carried over from `glOSports-maxpayne`. The console
stays stock goodluckOS.

Status: runs on the GA36-MB. Menu about 55 fps, new game through
the intro cutscenes (with their sound) into Grove Street at about 25–28 fps,
Xbox 360 HUD. Stops cleanly from the launcher, also with a kill (the loader's
first thread stays alive for that, see `loader/main.c`). Unresolved imports,
JNI functions and Java methods are logged by name.

## Game files

From your own install of the Android game (package `com.rockstargames.gtasa`;
the German `com.rockstargames.gtasager` is a different build and won't load),
into `/home/player/ports/gtasa/game/`:

| From | Version | What goes into `game/` |
|---|---|---|
| the `.apk` | **2.00** exactly (the hooks use its code offsets) | `lib/armeabi-v7a/libGTASA.so`, and the contents of `assets/` |
| `main.8.com.rockstargames.gtasa.obb` | 2.00 | its contents (it is a zip) |
| `patch.8.com.rockstargames.gtasa.obb` | 2.00 | its contents, over the main OBB's |

The OBBs are in `/sdcard/Android/obb/com.rockstargames.gtasa/` on the phone.
Check that `audio/SFX/SPC_GA.osw` (about 355 MB, 31234 speech lines by its
`.osw.idx`) came along: copies passed around for other consoles sometimes
leave it out, and the game then plays the scripted dialogue outside the
cutscenes silently (the cutscene tracks are in `audio/STREAMS/CUTSCENE.osw`).
For the Xbox 360 HUD (radar bottom left), copy `data/360Default1280x720.cfg`
to `game/Adjustable.cfg`; without it the game uses the stock mobile HUD.
The console uses gtasa_vita's `timecyc.dat` (`upstream/gtasa_vita/gamefiles/data/`)
in `game/data/` over the OBB's: the same weather, with the colour filter
alphas on the PS2 scale (about half, which our stock filter mostly normalises
away), a fixed rainy-countryside 8 PM row (two values missing in the OBB's,
shifting the rest) and one more value per row for the Vita's PS2 lighting.
On the first start the loader patches two script files in place
(`data/script/mainV1.scm`, `scriptv1.img`: the ITB and gym treadmill fix from
gtasa_vita); it checks the original bytes first, so a second start leaves
them alone.

## The game folder on the console (`/home/player/ports/gtasa/`)

| | |
|---|---|
| `gtasa` | the loader (DT_RPATH `$ORIGIN/lib`) |
| `lib/` | OpenAL Soft 1.19 and its dependencies (sndio, bsd, md) |
| `alsoft.conf` | OpenAL reads it from the game folder by itself (ALSA) |
| `game/` | the game files above; saves land here too |
| `glos/` | the ARM driver set from `goodluckOS-mali-driver` |
| `start.sh` | pad mapping and settings, then `exec ./glos/glos-gpu run mali ./gtasa` |

Launcher entry: `gtasa.puppy`, its icon a crop of a SteamGridDB banner. Log: `/dev/shm/gtasa.log` (a crash prints the
PC and LR as `libGTASA.so+offset`).

## Settings (`start.sh`)

| Variable | Default | |
|---|---|---|
| `GAME_STICK_SCALE`, `GAME_STICK_DEADZONE` | 1.5, 0.12 | stick shaping (radial dead zone after scaling) |
| `GAME_FPS_COUNTER` | 0 (1 in `start.sh`) | frames per second, top right |
| `GAME_LOG_FPS` | 0 | fps, swap time, swaps and flips into the log |
| `GAME_VSYNC` | 1 | swap interval and the wait for the previous flip (no tearing) |
| `GAME_AUDIO_RATE` | 44100 | OpenAL mixing rate (the game asks for 22050) |
| `GAME_LANGUAGE` | 0 | 0 English, 1 French, 2 German, 3 Italian, 4 Spanish, 5 Japanese |

## Fixes of our own

- **Target switching:** while locked on, LT / RT (the Xbox 360 layout's
  PREV/NEXT_TARGET, which the game only read as a touch swipe) and a
  right-stick flick past half switch targets
  (`CPad::ShiftTargetLeft/RightJustDown`).
- **Frame Limiter:** outside the menu `DoGameState` waits for 1000/maxFPS ms
  a frame, maxFPS 20 with the limiter on and 30 with it off. Now 30 on and
  60 off (above the panel's ~55 Hz, so the flips set the pace). The main
  menu keeps the game's 30.
- **Detail textures off** (the game's `gNoDetailTextures`).

## From gtasa_vita, what carries over and what doesn't

- **Carried over** (its defaults, always on): the flying-vehicle camera fix
  (the right stick stays the camera in helicopters and planes, the Harrier
  nozzle on the right stick, the rudder on the shoulder buttons), no mobile
  widgets (no touch buttons for side missions and cutscene skip, no
  steering and rating popups), cut radio tracks allowed, no explored-zone map highlight, free aim
  and target switching fixes, no automatic landing gear, telemetry off, jump /
  crouch / enter car / camera distance on press, the Xbox 360 button layout,
  Resume loads the newest save, Exit saves and quits, the cheat hash table,
  the two script fixes.
- **Left out:** the Vita's rendering work (Cg shader rewrites, the PS2 colour
  filter and its `timecyc.dat`, vitaGL tuning, the matrix and bone
  optimisations): the game's own GLSL ES shaders run on Mali-400, as they did
  on Android phones with the same GPU. The custom button mapping file and the
  keyboard for cheats (no keyboard on the console). Touch.
- **Different:** the pad is reported as an Xbox 360 pad (the Vita's as a PS3
  one, with the Xbox 360 layout swapped in), so prompts show Xbox buttons.
  Threads are real pthreads (the Vita runs the game's main
  thread on the loader's); mutexes, conditions and semaphores are glibc's
  behind bionic-sized handles; EGL is ARM's fbdev EGL with the tear-free flip
  wait from `glOSports-maxpayne`.

## The loader (`loader/`)

| File | |
|---|---|
| `so_util.c` | maps the .so, relocates, finds symbols through its hash table, patches code (`hook_addr`: Thumb or ARM by the address's low bit) |
| `imports.c` | bionic → glibc (softfp wrappers for every float import, bionic-sized pthread / sem / stat / FILE / jmp_buf); every `gl*` / `egl*` resolves to ARM's driver, through wrappers where floats pass by value |
| `jni.c`, `jni_names.inc` | the fake Java VM and JNIEnv: all 233 slots, unknown ones log their name |
| `android.c` | War Drum's threads and screen, and the Java methods the game calls (window, storage, device, pad) |
| `egl.c` | GLES 2.0 on EGL on `/dev/fb0`, tear-free flips, FPS counter |
| `openal.c` | every `al*` / `alc*` entry point of the game's own OpenAL Soft jumps to the folder's (found by walking the .so's symbols) |
| `pad.c` | SDL2 game controller, stick shaping |
| `game.c` | what is GTA SA's: hooks, offsets, fixes |

## Videos (from the PS2 release)

The Android build plays no video (nothing calls `OS_MoviePlay`). The PS2
release has two, `MOVIES/VCNTSC.PSS` (the Rockstar logos, 15 s) and
`MOVIES/OPEN1.PSS` (the opening, 89 s): 640x480 MPEG-2, the screen's shape,
with 48 kHz stereo PCM in Sony's SShd/SSbd wrapper in MPEG private stream 1,
which FFmpeg doesn't read. `tools/pss_audio.py` takes the sound out,
`tools/movies.sh <MOVIES folder or disc image> [out]` converts both to
640x480 H.264 baseline + AAC (`logos.mp4` 0.3 MB, `intro.mp4` 6 MB, into
`gamedata/movies/` by default) for `game/movies/`. Tested with the USA v3.00
disc (SLUS-20946).

The loader plays them once at the game's first swap, as a cutscene on the
thread that draws (`startup_videos` in `loader/egl.c`, `loader/movie.c` from
`glOSports-gtactw`), then the game goes on: 459 and 2,659 frames, none
dropped. A or Start skips; without the files, or with `GAME_INTRO=0`,
nothing happens.

## Build

```
./build.sh      # loader/ -> build/gtasa (Docker, Debian bookworm armhf)
./package.sh    # -> out/ports/gtasa/ and out/.local/share/applications/gtasa.puppy
```

`build.sh` runs `tools/build-ffmpeg.sh` first when `build/ffmpeg/` is
missing: a static FFmpeg 6.1.2 (MP4 demuxer, H.264 and AAC decoders, NEON),
from ffmpeg.org's `ffmpeg-6.1.2.tar.xz` (sha256 `3b624649…`) unpacked in
`upstream/ffmpeg-6.1.2`.

Test tools are shared: `../goodluckOS-tools/testing-sync.sh gtasa` puts them
on the console (`glos/testing/`), `testing-clean.sh gtasa` takes them off,
`shot.sh gtasa file.png` takes a screenshot (see `goodluckOS-tools`).

## Reference loader (`upstream/`, not committed)

| Folder | Source | Commit |
|---|---|---|
| `upstream/gtasa_vita` | [TheOfficialFloW/gtasa_vita](https://github.com/TheOfficialFloW/gtasa_vita) (MIT) | `9694171` |

## Third-party components

Code carried over from the Vita loaders it names keeps their MIT notices;
those, FFmpeg (LGPL-2.1, linked statically for the videos), OpenAL Soft and
the other bundled libraries, and ARM's Mali driver are listed with their
licences in `licenses/`, which `package.sh` copies into the game folder. The
game itself is not included.

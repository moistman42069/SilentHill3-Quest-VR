# SH3 Quest VR: session 20 handoff (2026-11-26)

Started from the owner's 0.1.4 headset report (D:/Downloads/report_1795691087348.txt; run_2026-11-26_06-04-38.txt is the
same text). The session stopped at the owner's "wrap up and do a handoff". **Nothing is built or committed.** The next
release is **0.1.5** (new; v0.1.0-v0.1.4 stay as published).

## The owner's asks (two messages, verbatim in full in the session transcript)

1. "FIX all the issues you can see and figure need fixing ... make sure this next release is as perfect as possible."
2. "keep appsw off by default going forward, but fix it if it seems wrong since my game kept stuttering/freezing/
   cutting to black."
3. "cutscenes arent synced with audio/seem delayed."
4. "theres also a depth of field effect that seems to still be on during cutscenes. see if possible to disable."
5. Dynamic cutscenes: "im not facing the exact look direction of heather when it switches to 1st person", and "her head
   is still viewable in 1st person during cutscenes, clipping in. openrac/jak and dax seem to properly position it/hide
   head in those situations."
6. "even with appsw off, the game has this weird stutter/ghosting when turning my head. be sure that performance is as
   maximized as it can be. look up and research whatever you need to COMPLETELY MAXIMIZE PERFRMANCE."
7. "make it so clicking left stick and right stick at the same time recenters, like other ports."

## What the report shows

The owner's settings: Quest 3, **refresh 120 Hz**, **render resolution 1.5x** (eyes 2520x2640, the Quest 3's start
value from device_defaults), dynamic resolution on, foveation 2, boost on, sharpen on, **Smoother frames on**, smooth
edges on, Cutscenes "Dynamic".

- **The last run (0.1.4, AppSW on):** AppSW engaged (period 16.67 ms); the game made only 28-57 frames a second for its
  60 slots ("[vr-perf] 39.2 frames/s ... 442 displays missed, smoother frames on 100%"). Hence the stutter.
- **The run before (0.1.3, AppSW asked but never given):** 120 Hz with the game at 50-57 a second, "worked" 16-17 ms.
- **Dynamic resolution** said several times "the smaller pictures did not make the frames quicker ... the game's own work
  is the limit": CPU-bound (or bound by something resolution does not change).
- **~2 s freezes in play**: "most 2021.47", "2018.54", "2019.75", "2020.15" ms (one frame each, in successive 10 s
  spells, frames 7022-8633), and "most 2017.66" in the 0.1.3 run without AppSW. Not AppSW's. Very likely the
  "freezing/cutting to black" (the Quest shows black or the last frame when no frame comes for ~2 s). **Cause not found.**
  Ruled out: the game's own waits (tools/find_api_calls.py: WaitForSingleObject/WaitForMultipleObjects timeouts are -1,
  0, 100 or passed in; a scan of the exe for 0x7D0 found one constant, not a wait); the port's bridge code has no 2 s
  timeout; the probes' GPU read is non-blocking (glClientWaitSync 0). Still suspected: the GL driver or the OpenXR
  swapchain wait (acquire_chain waits XR_INFINITE_DURATION and is inside "worked"), the audio path, file streaming.
- **logcat**: three earlier crashes (11-25, 0.1.3 runs): `FORTIFY: pthread_mutex_lock called on a destroyed mutex
  (0x79fc690b50)`, likely at exit (a thread locking a mutex already destroyed). sh3-crash.log said "(none)" for the last
  run. To find and fix (owner rule: every crash/close explains itself in the game's own log).
- At the end of a run, after the session stopped: several "framebuffer incomplete: 0000" lines (harmless noise; quiet
  them once the session has stopped).

## Cause of the cutscene desync (found, fix coded)

The game sets its frame-time tuple each frame in play: 0x5F1940 (the main loop) at 0x5F1A74 calls **0x60DA50(step)**,
step = [0xBCC630] picked by 0x5F15E0 from the limiter's tick counts (1-3 sixtieths). 0x60DA50 writes 0x070E67A8 (the
step in sixtieths), 0x070E67B4 (the same), 0x070E67AC (its seconds), 0x070E67C4 (frames a second), and the whole-step
ints 0x070E67C0 / 0x070E67A4. 0x60D8E0 (the seconds' getter) has 1692 callers. While the headset paces the game, the
limiter's virtual clock (kernel32.c rt_limiter_virtual_step) counts one tick a frame, so step = 1 always, and sh3.c's
write of 60/hz at Present was overwritten by 0x60DA50 before every update. **So the game always ran 1/60 s per frame**:
twice its speed at 120 frames a second, half speed at 30 (AppSW's misses): scenes slow, voices ahead.

Coded (not built):
- src/vr/sh3vr.cpp begin_next_frame: `game_seconds` = this frame's display time minus the last one's (clamped to
  half a period .. 3/60 s, the game's own longest step), else 1/display_hz. `float sh3vr_frame_seconds(void)` returns it
  (0 when not paced); declared in src/vr/sh3vr.h.
- src/game/sh3.c: **`sub_0060DA50` wrapper**: when paced, ARG(0) = 1, the generated code runs (its 0x5FE970 checks and
  the int counts), then `vr_time_step()` writes the tuple from the real seconds (scale = s / the game's one-step seconds,
  fps / scale), and logs once at the start and every 600 frames "the game's step is the real time a frame is shown: N
  sixtieths on average ... N game frames a second". sh3_vr_frame's write at Present now uses the same real seconds.
- tools/build_quest.py: "sub_0060DA50_gen" added to the wrapped list.
- **A new wrapper means `tools/regen.sh --recomp-only` before building** (PY=C:/SH3Quest/venv/Scripts/python.exe).
- To check: the rig paces at 60 Hz (xr_sim.cpp sleeps 16.667 ms), so the step should read ~1.00; with SH3 slowed (e.g.
  a test sleep) it should rise. Then on the headset: a scene's lips with its voice.
- The game's code was written for steps of 1-3 (0x60DB40 clamps a float step to 1..3); at 72-120 Hz the step is below 1
  (0.83 / 0.5). The PC VR mod ran 0.667 (90 Hz) and called movement and animation right; integer per-frame counters in
  the game would still run at the frame rate (unchanged from before).

## AppSW off by default (coded in the game, not yet in the launcher)

- src/vr/sh3vr_internal.h: `space_warp = false`, **`refresh_hz = 72.0f`** (was 120, LIST 51's "120 Hz with AppSW"; with
  AppSW off 120 Hz needs 8.3 ms frames, which the game cannot make on a Quest 3: 13-17 ms in play).
- src/vr/sh3vr.cpp: kDefaultsVersion = 5; load_options: `if (defaults < 5)` turns Smoother frames off once and moves a
  refresh of exactly 120 to the new default (72), saves, logs "sh3vr.ini from before 0.1.5 ...".
- **Still to do in the launcher** (android/app/src/main/java/dev/sh3quest/vr/): Options.java `SPACE_WARP_ON = false`,
  `DEFAULTS_VERSION = 5`, the refresh row's default 72; Config.newDefaults: for version < 5, space_warp to its default
  and refresh_hz 120 -> 72 (as the game does). Check the launcher's Recommended performance options text.
- "fix it if it seems wrong": AppSW itself drew motion right on the rig (0.1.4, vr261/vr262). The owner's stutter was the
  game missing AppSW's 60 slots at 120 Hz. If AppSW stays offered, consider pairing it with 72 or 90 Hz (36/45 game
  frames a second), and the 2 s freezes are separate.

## Performance (started)

Research (Meta, [CPU and GPU levels](https://developers.meta.com/vr/essentials/boost-cpu-gpu-levels)):
- Quest 3 only: manifest `<meta-data android:name="com.oculus.trade_cpu_for_gpu_amount" android:value="-1"/>` gives
  +1 CPU level for -1 GPU level. Fits a CPU-bound game; dynamic resolution covers the GPU. To add to the manifest.
- Boost levels: the port already asks XR_PERF_SETTINGS_LEVEL_BOOST_EXT for CPU and GPU ("CPU taken, GPU taken").
- A CPU boost hint (v77+) exists for loads (45 s at most); Quest boosts 45 s at launch anyway.
- Frame budgets: 72 Hz 13.9 ms, 90 Hz 11.1 ms, 120 Hz 8.3 ms.

A sampling profiler for the PC rig: **tools/prof_sample.c** (new; `clang -O2 tools/prof_sample.c -o
build/prof_sample.exe -ldbghelp -lwinmm`; `build/prof_sample.exe <pid> 18 45`). It suspends each thread every ~1 ms,
reads RIP and names it with the PDB. Script **tools/sim/perf.txt** (walking, turning, looking about). Run **vr264**
(build 212, AppSW off): C:/SH3Quest/vr264.log, profile C:/SH3Quest/prof264.txt (6697 samples, 18 s). The main thread
(7664):

| Share | Where | Note |
|---|---|---|
| 14.2% | ntdll!NtQueryInformationThread | a syscall on the main thread: find the caller (compat_win.c / host.c / SDL?) |
| 6.3% | ucrtbase!getenv | dinput.c scripted_key/GetDeviceState call getenv("SH3_KEYS") per poll (rig only), **d3d8_shader.c:1048 calls getenv("SH3_VSC_DUMP") on every fixed-function draw (headset too)**, d3d8.c:451-480 four per Present, dsound.c:461 per buffer |
| 6.4% + 5.8% + 1.2% + 0.8% + 0.8% | sub_00648639, sub_006483A7, sub_00648491, sub_0064856F, sub_00648432 | the game's own (0x648xxx: find what it is, likely its matrix/vector or memory code; a native version could pay) |
| 4.2%, 3.6%, 2.0%, 2.0% | sub_0048C290, sub_0048E140, sub_0048C4E0, sub_0048F040 | the game's own (0x48Cxxx-0x48Fxxx: drawing lists?) |
| 2.7% | SDL2!SDL_wcsstr | a guess by export name (SDL2.dll has no PDB): likely SDL_GetKeyboardState / scancode names from scripted_key |
| 1.5% | ucrtbase vsscanf | scripted_key's sscanf (rig only) |
| 1.5% + 0.6% + 0.7% | shadow_mesh, shadow_part, cross | VR: her shadow volumes from her mesh, built on the CPU each frame |
| 1.5% | sub_00662C40 | the CRT's ftol |

Caveats: the PC is several times faster than the Quest's cores and the rig paces at 60 Hz, so the main thread waits much
of the time (its idle shows as the sleep); the shares above are of all its samples. The scripted-keys costs are the rig's
own; the per-draw getenv is real on the headset.

Planned (none done):
1. Cache every per-call getenv (d3d8_shader.c:1048, dinput.c scripted_key: parse SH3_KEYS once, d3d8.c Present's four,
   dsound.c:461, sh3.c's per-frame ones at 1561/1943/2004/2035): static values read once.
2. Find NtQueryInformationThread's caller on the main thread (extend prof_sample.c to walk a few frames with
   StackWalk64, or set a breakpoint in a debugger); on Android its equivalent may be a gettid/syscall in a hot path.
3. The game's hot functions 0x648xxx / 0x48Cxxx: disassemble (tools/disx.py) and see whether a native replacement
   (memcpy/matrix) is worth it.
4. GPU time per frame: GL_EXT_disjoint_timer_query around the frame, in [vr-perf] ("GPU x ms"), so CPU- vs GPU-bound is
   known from a player's log; dynamic resolution could use it.
5. The 2 s freezes: a per-frame breakdown when a frame takes over 250 ms (the guest update to Present, sh3vr_present,
   each swapchain wait, xrEndFrame, the bridges' waits on the main thread with the caller and handle, file reads, Sleep),
   logged with the frame number.
6. Build flags: the Quest build is RelWithDebInfo (-O2) with -fno-omit-frame-pointer; try -O3 / thin LTO on the VR and
   runtime files (not the 30 MB of generated code at first), measure.
7. Manifest trade_cpu_for_gpu_amount = -1 (above); the default refresh 72 (done in the game); consider starting the
   Quest 3 at a lower render resolution than 1.5x if the GPU time says so.
8. Head-turn "stutter/ghosting" with AppSW off: at 120 Hz and 50-57 frames a second each picture shows 2-3 times, which
   judders; 72 Hz and the time-step fix should help most. Also check: the game's own afterimage/blur render targets
   (the 512x512 / 64x64 / 62x62 targets from 0x40281C, the 256x256 ones from 0x427CCF in scenes) blending the last
   frame while the head turns; Smooth edges is FXAA (spatial, not the cause).

## Not started

- **Depth of field in cutscenes** (owner ask 4): find the pass (likely the 256x256 / 512x512 render targets made during
  scenes, "from 00427ccf" in the log; disassemble around 0x427CCF and the draws into them) and leave it out in VR or add
  an option (plain name, e.g. "Depth of field in cutscenes", off).
- **Dynamic cutscenes** (owner ask 5): on the switch to first person, the log's "recentred (first person): Heather's
  facing 0.00" shows her facing was not taken from her head; take the yaw (and pitch) of her head bone. Hide her head in
  first person during scenes (the head's bones or draw group near the eyes), as the OpenRAC/Jak ports do.
- **Both sticks clicked together recentre** (owner ask 7): in sh3vr.cpp's input (pad.h has kPadL3/R3 style bits);
  recentre as the menu's recentre does; make sure no other action takes L3+R3 together (owner rule: every button
  reachable, no conflicts).
- The FORTIFY mutex crash at exit; the "framebuffer incomplete" noise after the session stops.
- Docs for 0.1.5: RELEASE-NOTES-0.1.5.md, launcher whats_new.txt / features.txt, README, HEADSET_CHECKLIST, AUDIT rows,
  GAME_FACTS (Timing: the step setter above), this handoff's follow-up; versionCode 6 "0.1.5"; the APK; apkcheck;
  verify every feature on the release build before publishing (owner rule).

## Files changed this session (uncommitted)

- src/game/sh3.c (sub_0060DA50, vr_time_step, the Present write)
- src/vr/sh3vr.cpp (game_seconds, sh3vr_frame_seconds, kDefaultsVersion 5, the defaults < 5 fold)
- src/vr/sh3vr.h (sh3vr_frame_seconds)
- src/vr/sh3vr_internal.h (space_warp false, refresh_hz 72)
- tools/build_quest.py (wrapped list)
- tools/prof_sample.c, tools/sim/perf.txt (new)
- C:/SH3Quest/gamedir/sh3vr.ini: put back as it was (a copy at C:/SH3Quest/sh3vr.ini.gamedir-before-s20); it is a
  defaults-4 file with space_warp = 1, right for checking the 0.1.5 fold.

## Next, in order

1. The launcher's half of the defaults (above).
2. The performance items 1-2 and 4-5 (cheap, and they explain the freezes), then 3, 6, 7.
3. Depth of field, the dynamic-cutscene facing and head, both sticks to recentre.
4. regen --recomp-only, one PC build (build 213), short rig checks (next run vr265): the fold's log line, "the game's
   step" line, a scene in Dynamic mode (facing, head hidden), L3+R3, pictures checked whole.
5. Release 0.1.5 (ask before publishing).

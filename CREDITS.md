# Credits

SH3 Quest VR is built directly on other people's work. Every one of them made this possible.

| Who | What | Used for |
|-----|------|----------|
| **cutarev** | [sh3-nx](https://github.com/cutarev/sh3-nx) (and [sh2-nx](https://github.com/cutarev/sh2-nx)) | The whole port underneath: the recompiled game, its runtime (Direct3D 8 on OpenGL, DirectSound, DirectInput, kernel32), the launcher, the widescreen work. Built on directly. |
| **NotGodlikeUwU** | [Silent Hill 3 VR mod](https://github.com/NotGodlikeUwU/Silent-Hill-3-VR-Mod) (PC) | Its findings, taken and refined: Heather's weapons by mesh signature and their bone palette, the calibration angles and sh3vr_weapons.ini format, melee reaches and swing thresholds, the native hit volumes through clBattleAddQue (and lengthening them by the weapon's radius), the attack pulses, the aim direction from the controller, camera-relative movement, the 165 cm player height and world scale. |
| **heathercherymason-beep** | [SH3AndroidEvolved](https://github.com/heathercherymason-beep/SH3AndroidEvolved) | Showed sh3-nx's runtime running on Android GLES. |
| **sp00nznet** | xboxrecomp (MIT) | The static recompiler sh3-nx uses to lift sh3.exe to C. |
| **ThirteenAG** | [WidescreenFixesPack](https://github.com/ThirteenAG/WidescreenFixesPack) (MIT) | sh3-nx's widescreen fixes. |
| **zealottormunds** | [sh3cammod](https://github.com/zealottormunds/sh3cammod) | The first-person camera idea the PC VR mod used. |
| **Steam006** | Silent Hill 3 PC Fix | The PC fixes the PC VR mod is set up with. |
| **The Khronos Group** | OpenXR loader and headers (Apache-2.0), OpenGL ES headers | VR on the Quest. |
| **SDL** (Sam Lantinga and contributors) | SDL2 (zlib) | Window, input, sound and the Android activity. |
| **Dominic Szablewski** | [pl_mpeg](https://github.com/phoboslab/pl_mpeg) (MIT) | The movies: the MPEG-1 video, the Layer II sound and the program stream, decoded once the port has unscrambled the game's .000 files. |
| Our Ratchet & Clank and Jak and Daxter Quest VR ports | The headset layer (OpenXR, the pointer, the rig, haptics) and the launcher | Reused and adapted here. |

Silent Hill 3 is © Konami. This is an unofficial fan project; it includes no game files: it runs your own copy of
the PC game.

## About the projects without a license

sh3-nx, the PC VR mod and the Android port carry no license file. The owner of this project decided, on 2026-11-24,
to build directly on their code and credit every author here, in NOTICE and in the release notes.

# Third-party components used by CyGameCaptureVideoProbe

| Component | Version | Licence | Location |
|-----------|---------|---------|----------|
| FFmpeg (public headers only: libavutil, libavcodec, libavformat, libswscale) | 6.0, tag `n6.0`, commit `ea3d24bbe3c58b171e55fe2151fc7ffaca3ab3d2` (2023-02-26) | **LGPL-2.1-or-later** | `ffmpeg/include/` |

The licence texts are kept next to the headers (`ffmpeg/COPYING.LGPLv2.1`, `ffmpeg/LICENSE.md`).

## What is vendored, and what is not

Only the **public** headers are here: the exact list each library declares as `HEADERS` in its makefile,
123 files, with none of FFmpeg's internal headers and none of its source. `libavutil/avconfig.h` is the
one file that is not copied verbatim, because FFmpeg generates it from its configure script; the copy
here carries the values of a normal x86-64 build (little endian, fast unaligned access) and defines
nothing else.

**No FFmpeg binary is vendored or downloaded.** The probe links the `avcodec-60`, `avformat-60` and
`avutil-58` DLLs that **OBS Studio already installs**, through import libraries generated on the machine
that builds it by `Tools\GenerateFFmpegImportLibs.cmd`. Those `.lib` files contain no FFmpeg code, only
export names. The DLLs are delay loaded and the probe points the loader at the OBS installation itself
at start-up, so nothing has to be copied around or put on the PATH.

## Licensing note

FFmpeg is LGPL-2.1-or-later in the configuration OBS ships (a build configured with `--enable-gpl`
would be GPL instead). The probe uses it through dynamic linking against binaries the user already has,
which is what the LGPL is designed for, and it is a development and verification tool rather than
something shipped to end users. It is also entirely optional: without the import libraries CMake skips
it with a message and the rest of the project builds unchanged.

This concerns `CyGameCaptureVideoProbe` alone. CyGameCaptureRS, CyGameCaptureUE and CyGameCaptureOBS
link nothing from FFmpeg.

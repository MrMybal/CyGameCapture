# Third-party components used by CyGameCaptureOBS

| Component | Version | Licence | Location |
|-----------|---------|---------|----------|
| OBS Studio / libobs (public headers only) | 29.1.3 | **GPL-2.0-or-later** | `obs-studio/libobs/` |
| Spout2 SDK (DirectX subset: SenderNames, SharedMemory, FrameCount, Utils) | see `../../CyGameCaptureRS/ThirdParty/THIRD_PARTY_NOTICES.md` | Simplified BSD (2-clause) | compiled from `CyGameCaptureRS/ThirdParty/Spout2/` |

The full OBS licence text is kept in `obs-studio/COPYING`. No libobs source file is modified: only the
public headers of the release the plugin is built against are vendored, so the plugin can be compiled
without a full OBS build tree.

`obs-studio/libobs/obsconfig.h` is the one file that is not copied verbatim: OBS generates it from
`libobs/obsconfig.h.in` at build time. The copy here fills in the values of the 29.1.3 release. It only
carries version strings and platform feature flags and does not affect the ABI of any function the
plugin calls.

`obs-studio/lib/obs.lib` is **not** vendored. It is an import library generated on the machine that
builds the plugin, from the `obs.dll` of the installed OBS Studio, by `Tools/GenerateObsImportLib.cmd`.
It contains no OBS code, only export names.

## Licensing note

libobs is distributed under the **GNU General Public License v2 or later**. A plugin that links against
it and is distributed to others has to be released under a GPL-compatible licence. CyGameCaptureOBS is
released under the **GNU Affero General Public License v3 or later**: the "or later" clause of libobs
makes GPL v3 available, and section 13 of GPL v3 explicitly allows a work under it to be combined with
code under AGPL v3. This concerns CyGameCaptureOBS only: CyGameCaptureRS (ReShade add-on) and
CyGameCaptureUE (Unreal plugin) link nothing from OBS.

`obs-studio/COMMITMENT` is the GPL Cooperation Commitment of the OBS project (GPL v3's cure and
reinstatement terms, extended to its GPL v2 code); it is kept with the licence text.

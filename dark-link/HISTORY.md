# Dark Link 0.1.38 — matte contour shading

Replace the body/equipment's flat-zero palette with separate dark cool ambient
C0 and charcoal diffuse K0 inputs; keep the other colour registers at a subdued
charcoal base. The native environment path assigns ambient C0 to slot 4 and
diffuse K0 to slot 7 (d_kankyo_6 setLight_actor / setLightTevColorType). Native
normals, toon ramps, light directions, material/ambient lighting channels and
fog remain unchanged. No specular lighting, new lights or shader replacements.

The body albedo view now returns neutral RGB rather than zero, so it no longer
annihilates the nonzero palette. A separate cache/binding role keeps eyes red
and the aura black, including late native texture rebinding. Texture alpha,
UVs and image data stay native. All palette values/owned flags and texture
bindings retain the existing restoration behavior. Aura fade from 0.1.37 stays.

Regression coverage checks distinct nonzero body inputs, all six eye variants,
equipment, untouched lighting/alpha, nested restoration, missing parameters,
and three distinct views of one texture with late aura rebinding. These tests
validate submitted inputs and ownership, not the actual game shaders; device
checks must confirm body/face/equipment contrast in bright and dark scenes,
blinking and repeated transformation/cancellation.

# Dark Link 0.1.37 — softer black aura

The Ymgcs00 shell samples the midpoint of its native BRK fade during Link draw,
instead of the fully visible end. The other aura animation remains at its old
frame, and native BTK motion, black blending, body, eyes and equipment are
unchanged. This is 50% of the fade timeline, not a claim of exactly 50% opacity;
the game-owned curve controls the result.

Frame resource, controller and renderer-channel values are restored after each
draw, including during real Magic Armor use. No protection flags, timers or save
values are changed. Regression coverage checks the shell-only midpoint, repeated
draws, restoration and the disabled path. Actual GPU appearance needs an in-game
check in bright/dark areas and while moving and toggling the transformation.

# Dark Link (prototype 0.1.36 — retain charge on manual cancellation)

Standalone code mod for ZeldaWWHDRecomp: sword hits charge a transformation with
black body/equipment, red eyes and boosted sword damage. The user confirmed the
complete body, mouth, eyes, sword and shield appearance in **0.1.11**. That
texture-binding repair at **viewCalc return** is retained.

## Changed in 0.1.36

Manually cancelling Dark Link retains the remaining gauge percentage as charge.
For example, cancelling at 50% returns 50/100 charge; another ten damaging
sword hits fill it with default settings. Inactive charge does not drain, and
activation still requires a full gauge. This works for both 20-second short
and 40-second long gauges and custom capacity/duration settings.

Conversion uses remaining/total from the active transformation, not the current
duration setting. Only manual cancellation changes: natural expiry, death,
stage/owner reset and disabling the mod still clear the state. Appearance,
frame composition and the shared editor remain unchanged.

State tests cover 0/25/50/75/100% cancellation, custom capacity, settings changes,
blocked hits, pause, refill, reactivation, invalid totals and natural expiry.
Translated guest tests cancel both gauge sizes at half duration, wait while
inactive, verify nine hits are insufficient and the tenth permits activation.

Install `dark-link-0.1.36.wwhdmod`, complete the mod build and restart.

## Changed in 0.1.35

The native gauge appearance is confirmed working on device in 0.1.34.
The magic-meter upgrade now also doubles Dark Link transformation duration:
the short gauge uses the configured base duration, the long gauge uses twice
that value (20/40 seconds by default). Maximum magic selects the same variant
as the HUD: 0/16 uses the short gauge, values above 16 use the long gauge.
Current magic, HUD visibility and screen scaling do not affect duration.

Charge capacity and charge per damaging sword hit are unchanged: both gauges
still require 20 hits with default settings. An upgrade during a transformation
scales both remaining and total duration, preserving the remaining percentage
rather than refilling the timer. Downgrading likewise preserves that percentage.
Repeated updates do not compound the multiplier. Duration-setting changes
continue to apply at the next transformation; pauses, cancellation and unlimited
Free transformation keep their existing behaviour.

Tests cover both duration multipliers, the unchanged hit count, custom base
durations, upgrades/downgrades while active, paused updates, cancellation and
Free mode. Translated guest tests check 20/40-second expiry with the HUD hidden
and native magic empty, and verify that changing current magic does not change
the remaining gauge fraction. Native gauge artwork/placement is unchanged.

Install `dark-link-0.1.35.wwhdmod`, complete the mod build and restart.
No app or Enhanced Controls rebuild is required.

## Changed in 0.1.34

The 0.1.33 device report identifies the remaining rejection precisely:
`W_MagicBase_00`, 210x33, four frames, flags 3, successfully loads a 94x16
texture and is then discarded as `wnd-fill-occluded`. Authored and resolved
slice dimensions agree: 94/94/16/16. This is not a missing texture or size failure.

Remove the inner-alpha veto for the selected native frame. Frame artwork can
extend into the inner gauge; it does not need to be transparent there when
composited underneath the background and fill. Submit the outer frame first,
then the original-size inner background, then the independently cropped red
fill. Retain the actual frame pixels and colours rather than cutting out or
replacing the artwork. Native window content is still never drawn; unrelated
window selection, resource bounds and texture validation remain unchanged.

The regression reproduces the reported 94x16 slice dimensions and flags with
synthetic opaque artwork inside the gauge. It checks successful frame loading,
edge pixels and submission order at empty, half and full charge, both short
and long gauges, absent native magic, unequal scaling and cached redraws.
Untextured frames with interior coverage also remain visible underneath the
base. Tests explicitly reset their charge state before subsequent gameplay checks.

Install `dark-link-0.1.34.wwhdmod`, complete the mod build and restart.
The on-device rejection is addressed; exact visual matching with the game's
textures still needs in-game confirmation. No app rebuild is required.

## Changed in 0.1.33

The device's 0.1.32 report now reaches `wnd-image-failed`: window selection
succeeds, but the old report does not distinguish geometry, material, texture,
coverage or output failures. It does not identify the precise failing branch.

A renderer mismatch found during review is corrected: textured frame extents
come from the decoded corner textures (top-left and bottom-right), with the
wnd1 FrameElement dimensions used for untextured materials. Previously the
fallback fields were always used, rejecting zero/oversized values before the
textures could supply valid geometry. Reference: the frame-size selection in
[Switch-Toolbox BxlytToGL.DrawWindowPane](https://github.com/KillzXGaming/Switch-Toolbox/blob/3a3279140ca0fa54ce804a8d56d4653a491de613/File_Format_Library/FileFormats/Layout/BxlytToGL.cs).
Bounds are checked before converting scaled extents to integer coordinates.

Appearance Diagnostics now records the exact failed stage, frame index,
material/map count, texture name/dimensions, window flags, authored dimensions
and used dimensions. Examples include `wnd-texture-missing`,
`wnd-material-maps`, `wnd-slice-size`, `wnd-fill-occluded`, `wnd-file-write`
and `wnd-hud-texture`. The report is cached at load and displayed only when
diagnostics are enabled; it does not export resources or touch game state.

Tests cover texture-derived frames with zero and oversized fallback fields,
both capacities, nonuniform HUD scaling, pixel colours/coverage, untextured
fallback, missing textures, unsupported map counts and malformed frame tables.
Core gauge and touch controls remain valid when a border fails. The previous
0.1.32 module fails the new zero-field rendering regression.

This fixes the demonstrated renderer mismatch; the screenshot alone cannot
prove it is the remaining failure on the device. Actual game artwork is not
available locally. If the border is still absent, the expanded on-screen
report identifies the next failure without a log-file transfer.

Install `dark-link-0.1.33.wwhdmod`, complete the mod build and restart.
No app or Enhanced Controls rebuild is required.

## Changed in 0.1.32

The 0.1.31 device report identifies the actual rejection: the matching
`W_MagicBase_00` is 210x33 with four frames and alpha 255, but is discarded
as `wnd-opening/bounds` before any frame material is decoded. The old check
mistook nominal slice sizes for opaque border thickness.

For the exact capacity-matched MagicBase window, validate the rendered alpha
over the fill instead. Transparent padding in a frame texture is allowed;
artwork that actually obscures the red fill is still rejected. Outer bounds,
resource spans, image dimensions and unrelated-window checks stay bounded.
The inner green background remains at its original size.

Four-frame windows now use four perimeter strips with corner-anchored UVs.
This matches the four-frame geometry in the BFLYT renderer, rather than
stretching eight independent corner/edge quads. Transparent overlap is alpha
composited. Frame materials and colours still come from resident game assets;
window content is never drawn and no native HUD constructors are called.

Regression coverage uses a synthetic 210/400x33 four-frame window with padded
16x16 corner textures. Its nominal opening is too small, but its actual alpha
leaves the full red gauge clear. Tests check pixels on all four edges and
throughout the fill, both capacities, no native magic, nonuniform scaling,
and reject an otherwise identical opaque frame. No game artwork is bundled.

Reference for four-strip geometry:
[Switch-Toolbox BxlytToGL.DrawWindowPane, four-frame case](https://github.com/KillzXGaming/Switch-Toolbox/blob/3a3279140ca0fa54ce804a8d56d4653a491de613/File_Format_Library/FileFormats/Layout/BxlytToGL.cs).
The source images and slice sizes on the user's device are not available here;
the synthetic regression verifies the fix but does not replace an in-game check.

Install `dark-link-0.1.32.wwhdmod`, complete the mod build and restart.
No app or Enhanced Controls rebuild is required.

## Changed in 0.1.31

The 0.1.30 screenshot confirms that enlarging the green background did not
restore the missing native frame. That background enlargement is removed:
`P_MagicBase` is drawn at its own inner bounds again. The outer rectangle is
reserved for the separate `W_MagicBase` frame image and row/D-pad spacing.

The frame loader now supports untextured native materials, using their authored
white colour in linear space. Previously it required exactly one texture map,
so a valid solid-colour frame failed to load. Textured frame slices still use
the existing decoder/sampler. A wholly untextured frame does not require TXL.

The exact capacity-matched `W_MagicBase_00/01` frame remains independently
visible even if it belongs to the native Flash animation group. Membership
alone must not classify the normal frame as disposable warning artwork.
Only its border slices are copied, never window content. Other hidden/flash
panes and frames whose inner opening would cover the fill remain excluded.

Tests now distinguish the inner background draw from the outer border draw,
check solid frame pixels and transparent centres, and exercise zero-map
materials without a texture list or readable window content. Textured frames,
capacity selection, scaling, spacing and unrelated flash rejection remain
covered. The screenshots do not expose the frame material itself, so the
on-device result is still needed to confirm which rejection affected it.

Install `dark-link-0.1.31.wwhdmod`, complete the mod build and restart.
No app or Enhanced Controls rebuild is required.

## Changed in 0.1.30 (superseded background sizing)

The user confirmed the 0.1.29 gauge is now close, but its frame is too thin.
The background was still drawn at the inner `P_MagicBase` size. Its outer
bounds now come from the matching `W_MagicBase_00/01` rectangle, even when
that window's warning artwork is hidden. Only enclosing, unrotated, bounded
geometry is accepted; unavailable or invalid bounds retain the previous size.
This uses the existing native background art and does not enable flash content.
The red inner fill retains its dimensions and independent charge/time crop.

Placement includes both outer margins so the larger frame does not overlap
the normal magic bar. Details and borders use the same placement calculation;
the existing D-pad reservation measures the resulting outer row height.
Synthetic tests cover short/long capacity, unequal X/Y scale, unchanged fill
size, an eight-unit gap between outer frames, and repeated cached draws.
Final frame thickness still needs checking on the device with its native assets.

Install `dark-link-0.1.30.wwhdmod`, complete the mod build and restart.
No app or Enhanced Controls rebuild is required.

## Changed in 0.1.29

The 0.1.28 screenshot identifies the selection error: `P_MagicBase_00` was
selected as `BASE+FILL`, while the actual `P_Magic_00` (190x12, two texture
maps) was discarded as `outline-bounds`. The 386x18 / 380x12 long variant
uses the corresponding `_01` names. These exact native fill names now take
precedence over the old green-colour/first-stage-progress heuristic, including
when the native pane is hidden. Only Dark Link's charge/time crops the result.

The separate MagicBase artwork retains its authored colours; removing green
from that background also removed the native frame shading. Neutralisation
is now limited to layouts where the same picture really is both base and fill.
Named `W_MagicBase_00/01` frames can ignore native visibility for the matching
capacity, but flash-group membership and geometric opening checks still apply.
A frame must leave the actual inner fill unobstructed, rather than enclose the
larger background as well. The report panel now fits its row count.

Tests reproduce the reported names, dimensions, texture-map counts and hidden
panes at 0/16/32 magic capacity; they assert fill geometry, background pixels,
transparent frame centres, flash rejection and no source-resource mutation.
This fixes selection and frame handling, not a complete material interpreter:
the picture baker still samples the first texture map. The screenshot does not
expose the second map's combiner, so no guessed two-texture blend is introduced.
**Final appearance still needs on-device checking.**

Install `dark-link-0.1.29.wwhdmod`, complete the code-mod build and restart.
No app or Enhanced Controls rebuild is required. The 0.1.26 startup guard and
0.1.28 diagnostic text-buffer ownership fix remain in place.

## Changed in 0.1.28

Fixes the entire Dark Link overlay disappearing when **Appearance diagnostics**
is enabled. The new gauge report in 0.1.27 passed a guest-stack string to
`wwhd_hud_emit`. The runtime's `svc_hud_record` accepts buffers only inside the
calling mod's assigned memory region; it therefore rejected the string and
invalidated the whole list, including the gauge and touch controls.

The report now uses a static buffer in the mod's BSS. The service copies the
string synchronously, allowing the same buffer to be reused for each line.
The translated callback tests now check element and text pointer ownership
before the existing geometry/UTF-8/limit validation. This catches the published
0.1.27 module before the fix; the previous tests copied arbitrary guest memory
and therefore missed the service-level rejection.

The user's 0.1.28 screenshot confirmed that the report works and supplied the
native pane names used by the 0.1.29 selection fix above.

## Gauge diagnostics added in 0.1.27

The user confirmed that 0.1.26 removed the startup crash. The red gauge still
looks flat. This version adds read-only observations to **Appearance diagnostics**
(default off); it does **not** claim to fix the design or change gauge selection.
The second native HUD loader from 0.1.25 remains removed, and the startup-recovery
package checks remain active.

Use the current package described above.
Enable **Appearance diagnostics** in the mod settings and capture one screenshot
with the gauge visible. Below the appearance panel, the report shows the active
capacity, picture/window counts, rejected UVs, animation mask, selected fill
material colours, and each parsed pane's selection/rejection reason. For picture
rows the numeric triplet is texture count / progress flag / effective alpha;
window rows use frame count in place of texture count. At most 48 rows are shown;
truncation is explicitly counted. No diagnostic file or game artwork is exported.

`BASE`, `FILL`, `BASE+FILL`, `DETAIL` and `BORDER` identify submitted artwork.
`hidden/flash`, `outline-bounds`, `opaque-centre`, `wnd-opening/bounds`,
`wnd-image-failed`, `pixels-failed` and `pane-query` distinguish the existing
filters and loading failures. The animation mask bits are InAll, Flash,
ColorChange, ScaleChange and progress, in that order. These observations identify
which existing assumptions fail on the actual game resources, avoiding another
unverified change to the loader or rendering rules.

Gameplay, body/eyes/equipment, aura, D-pad spacing and the shared editor retain
the 0.1.26 behavior. Diagnostics do not write game memory or invoke a new native
function. They record metadata while the existing image cache loads; disabling
them emits no report commands. Existing generated gauge images remain local to
the mod's Data folder. No app rebuild or Enhanced Controls update is needed.

## Startup recovery retained from 0.1.26

The reported 0.1.25 crash enters the game's layout loader (0287F640) through our
second MagicBar init call. It removed the embedded parts descriptor and entered
a standalone loading path whose resources had not been established. The null
dereference happens inside the native call, before post-load checks. Mocked init
calls did not exercise the real resource loader. Version 0.1.26 restored the
0.1.24 gauge and removed native copy construction/drawing/teardown; an actual
on-device restart has now been confirmed. Do not use 0.1.25 for play or load its
full-state snapshots. The resource-only gauge path is retained; full multi-texture material support
is still pending.

## Added in 0.1.17

- Original Dark Link medallion icon in the Enhanced Controls style.
- The same touch editor as Enhanced Controls, with a standalone saved profile.
- One shared editor for all five mod buttons when Enhanced Controls 0.4.0 is
  installed; update both packages to enable this.

See the 0.1.17 section below for editor ownership, compatibility and validation.

## Fixed in 0.1.15

- Bake native texture coordinates and animated scale/translation into the
  copied gauge pixels. Version 0.1.14 sent these coordinates directly to the
  HUD: out-of-range or reversed UVs cause the renderer to discard the entire
  mod list, including the DARK button. The emitted UVs now stay within [0,1].
- Resample with bilinear filtering and edge clamping; preserve coverage alpha
  and crop the baked full-capacity red fill by the Dark Link timer.
- Reject unsupported rotated textures and invalid gauge geometry locally so
  they cannot discard the button or diagnostic overlay.
- Replay actual translated-hook HUD output through the runtime's real Store
  validator, including reversed/out-of-range scales and malformed resources.
- Retain 0.1.14's black aura blend equation/full-capacity animation handling
  and the user-confirmed 0.1.11 body/face/eye/equipment binding path.

USA/EU compilation, translated-hook regressions and synthetic pixel/PNG tests
pass. **The new appearance still needs checking in-game**: no game executable
or actual game artwork is available in this development environment. These
checks do not establish GPU output or support for every authored material.
The package contains original code only, with no game artwork/models/binaries.
No app rebuild is needed.

## Install and play

1. Download `dark-link.wwhdmod` from the **Dark Link mod** workflow artifact
   `dark-link-wwhdmod`, or use the supplied versioned package.
2. Replace the installed Dark Link package in Mods, finish its code-mod build,
   and fully restart the game. The same package supports USA/EU and Android.
3. Press **L3 + R3** together or tap **DARK** to transform; press/tap again to
   cancel. Defaults: 100 charge, 5 per damaging sword hit, 20 seconds (40 with the magic-meter upgrade), 200% damage.

**Free transformation** permits immediate transformation with unlimited duration.
Capacity, charge gain, duration, damage, automatic activation, gauge visibility,
and DARK button visibility/position remain configurable. The touch button keeps
its existing behaviour. Menus and cutscenes pause the transformation timer and
hide mod HUD. Death, stage changes, player deletion and disabling clear the
transformation. No normal-save values or global cheat options are written.

## Implementation

### Native aura

`setAuraEffect` (0240B7F4) updates the native shell's matrices and UV animation.
When no real Magic Armor/Tingle effect is active, temporarily supply its BRK
animation frames during that call, then restore the normal native fade value.
During Link's draw, expose the active frames and restore the frame controller,
resource and renderer channel afterwards. No shield flag, Tingle timer, magic,
rupees or invulnerability is changed. An actual armor effect retains its own
native update path. Animation length comes from the resource vtable, not the
uninitialized playback end field.

Only the current player's shell at Link+5700 joins the existing black palette
and `_a0` texture-view path. Its FMAT render state uses zero and inverse
source-alpha colour blending while active. If alpha originally shared the RGB
equation, preserve that original equation in separate-alpha fields. Restoration checks actor, model,
material array and resource identities and restores owned bits while preserving
unrelated changes. Other actors and held non-sword items are excluded.

### Magic gauge artwork

Read the resident **MagicBar_00** SARC/BFLYT with bounded offsets. Find the native
P_MagicBase_00/01 and the authored green fill. Query their original geometry and
UVs. Decode the referenced BFLIM (RGBA8, BC1–5; linear, micro and macro tiling),
apply the material's black/white colours in linear light, bake all four vertex
colours, then swap the final red/green channels. Preserve
texture/material alpha and contrast. When the base itself is green, remove its
green chroma in the empty copy. Prefer a distinct progress-animated fill; only
without one use the base itself as the full-size fill.
Bake the selected UV rectangle and texture scale/translation into the copied
pixels with bilinear sampling and the native clamp/repeat/mirror sampler.
Repeated textures use at least pane resolution to retain their detail. Partial fill then crops image
width and normalized UVs; native out-of-range/reversed UVs never reach the HUD.

Cache generated PNGs as `magic-{0,1}-{frame,fill,border}.png` and
`magic-{0,1}-detail-{00..07}.png` in this mod's
Data folder and load them through the existing HUD image service. The source
archive is read-only; these local derivatives are never included in the package
or uploaded. Conversion only runs on load/retry, not on every drawing frame.
Each decoded image is bounded to 131072 pixels and each cache write is below
1 MiB. Unsupported or missing textures are skipped and retried, with no fake
rectangle fallback. Picture vertex gradients are baked into the images. Window
borders use their original frame textures/materials and displayed alpha. Their
inner opening must enclose the selected gauge. Hidden/warning window decorations are excluded; the core base/fill use mod-owned
visibility and never inherit native pane/ancestor fade alpha.
Up to eight additional picture layers per capacity preserve native outlines
and progress artwork. Static outlines require a transparent inner area; progress
layers must fit inside the gauge. Their native geometry and texture detail are
retained, with green artwork recoloured red. Arbitrary multi-texture shader
effects are not reproduced.

A bounded BFLAN reader applies InAll at its end and Flash/ColorChange/ScaleChange
at zero for the displayed ordinary-Link style, then
selects a 16/32-frame animation explicitly bound to
`G_Bar_00`, validates all tracks before applying any, and evaluates its full
frame. It supports FLPA picture fields, FLMC material colours, FLVC corner
colours and the first FLTS texture transform. Animated ancestor-pane transforms,
texture-pattern swaps and rotated UVs are not supported. If no matching valid
animation exists, the static authored state is retained. Only mod-owned copies
change; the native HUD, animation controllers and magic amount stay untouched.

The native HD magic controller (02675414) supplies live base position, size and
origin. Place the copy eight layout units below it; before magic is unlocked,
use Main_00's authored placement. Normal and doubled capacities are supported.
The gauge shows charge, remaining duration, or full capacity in Free mode,
independently of normal magic. Full-state load renews image handles and position.

### Existing appearance and gameplay

Body, hands, hair, sword grip/blade, scabbard and shield keep 0.1.11's immutable
FTEX/GX2 views. RGB selectors are black, or red for the six named eye materials;
alpha, images, UVs and toon ramps remain native. Repair after native viewCalc
handles late mouth/eye texture animation. Restore the latest native binding on
cancel/disable, with identity checks for stale models. Cache capacity is bounded.
Scoped C0..C2/K0..K3 palette overrides preserve original RGB and override flags.

Damage scaling applies to freshly computed native sword damage, capped at 127
(or 63 for a double-hit spin). Only damaging hits through the native sword
collision path charge the gauge. Boss-specific handlers may override damage.

## Diagnostics and validation

Enable **Appearance diagnostics** only when needed. The first line reads
`DL 0.1.36 ON/OFF`; `Magic` counts loaded frame/fill images (up to four),
`Pos` is 1 for native or 2 for authored placement, and `Aura` reports the shell
model being present. Body material/albedo/eye counts, binding retention and late
repair counters retain their existing meanings. `Fill` reports the actual
Dark Link percentage (100% in Free mode); `Full` reports whether matching full
animations were applied for normal/doubled capacity. Diagnostics do not prove GPU
output. No Share log or game-file upload is required.

Automated checks cover gameplay/timing, body/eye/equipment texture restoration,
nested native refresh, animated face replacements, malformed layout bounds,
material-derived red pixels and alpha, PNG checksums/zlib decoding, full-width
coloured bases, UV cropping, native placement, handle reuse/state-load invalidation,
zero-length playback controllers with valid resource lengths, visual-only aura
frames, genuine armor coexistence, RGB suppression and preserved alpha blending.
Regression fixtures cover half-width authored fills becoming full-width at both
16/32 frames, texture SRT, malformed animation rejection, full Free-mode coverage
and neutral vertex tint. These are synthetic resource fixtures, not a game run.

In-game acceptance: activate Dark Link, check the black aura in motion and the
red bar below normal magic, drain/charge the bar, then toggle off and verify
normal Link/armor appearance. Check a save before and after acquiring magic,
doubled magic, menus, a stage change and full-state reload.

## Build and catalogue validation

Use a clean SDK checkout at the commit in the repository's `sdk.json`.
The catalogue builds the single entrypoint `src/mod.c`; source files, licence
and original artwork are included by the existing repository packager.

```sh
export WWHD_SDK=/absolute/path/to/pinned/ZeldaWWHDRecomp
python3 dark-link/tests/build_and_test.py
```

The local regression runner requires Python 3, PowerPC-enabled Clang/LLD and a
native Clang compiler. Generated files stay under the repository's ignored
`build/` directory. It does not require game files. For catalogue publication,
commit the sources first and run the repository's documented `tools/guard.py`,
`tools/package.py` and `tools/validate.py` against the pinned SDK. The local
runner's package is a test artifact, not the published catalogue download.

The maintained catalogue generates the download URL, size and checksum.
Contributors edit `catalogue.json`; do not hand-edit `index.json` or commit ELF/ZIP
files. Publication is separate from opening or merging a source PR.

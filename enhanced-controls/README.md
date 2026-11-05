# Enhanced Controls

Experimental version **1.0.0** adds four optional touch buttons: **Jump**, **Slash**,
**Spin** and **Dodge**. Draw your sword for attacks. Spin starts a native quick
spin once per tap; Dodge moves sideways with left/right input, otherwise back.
Physical controller input, native animations and damage remain intact.

## Install and settings

Enable **Show experimental mods** in the mod manager.
Install from **Mods → Community**, enable and complete code-mod setup when
requested. Requires mod manager **1.6.0**; supports USA/EU.
Touch controls are **off by default**. Turn on **Enable touch controls** in the
mod settings to show the buttons. While off, they claim no screen areas and do
not filter controller input. Settings also control the **Edit** button's visibility
and horizontal/vertical position.

## Touch editor

Tap **Edit** to select and drag a button. Each button has size, opacity, visibility,
shape, background and icon/text options. Hidden buttons remain selectable in the
top tabs. **Grid** snaps placement; **Reset all** resets the draft. **Save / Done**
keeps changes and **Cancel** discards them. Layouts persist across restarts.
A failed save leaves the editor open so you can retry or cancel.

Dark Link uses its separate **DL Edit** button.

## Limits

The game keeps running while editing: choose a safe location. The editor blocks
controller input until it closes and controls return to neutral. Native Android
controls can intercept touches; avoid overlaps or temporarily hide them.
Macros are cancelled during menus, events, stage changes and unsupported movement.
Spin requires a valid normal movement state with a drawn sword; rejected taps
are discarded. There is no pinch zoom, gesture pad or double-tap recenter.

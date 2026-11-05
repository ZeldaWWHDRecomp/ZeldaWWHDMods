# Enemy HP Bars

A standalone `enemy-hp-bars-1.0.0.wwhdmod` for ZeldaWWHDRecomp. Displays floating
health bars and rising, fading damage numbers on the main game picture. It reads
enemy health; it does not change combat, difficulty, drops or save data. No
textures, images, models or game files are included.

## Install

After catalogue publication, install **Enemy HP Bars** from **Mods → Community**.
For local testing, install the generated package through **Settings → Mods**.
Enable it and complete code-mod setup if requested.
Requires mod manager 1.6.0 or later. The same guest package targets USA and EU.

## Settings

All options are live and also appear on the in-game Mods settings page.

| Setting | Default |
| --- | --- |
| Enable Enemy HP Bars | On |
| Show HP bars | On |
| Show damage numbers | On |
| HP bar visibility | Always; alternatively four seconds after damage |
| Show current / maximum HP | Off |
| Display size | 100%; adjustable from 50–200% |
| Maximum distance | 2,000 game units; adjustable from 300–6,000 |

Bars turn green, yellow and red as HP decrease. Damage numbers last one second.
Numbers are **actual native HP lost**, capped at remaining HP on a lethal hit,
not hearts or theoretical weapon damage. Hits within one enemy update can be
combined. Healing, blocked hits and a stun that restores HP before the update
finishes do not create damage numbers. Environmental HP loss is also shown.

## Coverage and limits

Initial coverage: ChuChu, Bokoblin, Moblin, Keese, Miniblin, Peahat, Rat,
Darknut, Boko Baba, Bubble, ordinary Octorok, Magtail, Morth, Mothula, ReDead,
Poe, Armos, Armos Knight, Kargaroc, Wizzrobe and Floormaster. Shared-class
Octorok rocks, Baba Buds and Wizzrobe spells are excluded.

Bosses and Stalfos's separate body/parts health are not handled. Only retail
enemy profiles are observed; custom enemies with their own HP need an adapter.

Bars use the enemy's eye position with a per-type height fallback. This is a
screen overlay, not depth-tested world geometry; it can appear through walls.
Behind-camera points are rejected, and drawing is clipped by the viewport.
Only the current room is tracked. Cutscenes, transitions and native menu flags
hide/clear the display. Room changes, Quest Log loads and full-state epochs
reset it without producing phantom damage. Maximum HP uses the native maximum
when available, otherwise the highest health observed for that actor.

Enable **Show experimental mods** in the mod manager to find this mod.

# Heart ticker

An authored guest SDK example. Hooks Link's logic step using the public SDK's
named binding. Every configured number of steps, health decreases by a quarter
heart, then refills at half a heart. It never intentionally lowers health below
half a heart. Disable and restart to restore normal behavior.

Built once as a PowerPC ELF, then translated locally for USA or European
installations by the port. Native-code trust is required. Android guest mods
are not supported. This is a demonstration, not a recommended gameplay mod.

Source: `mod.c`. Licence: MPL-2.0 (see LICENSE). SDK declarations are supplied
by the pinned public port SDK, whose generated game declarations derive from
the pinned public CC0 decomp. No game files or initialized data are included.

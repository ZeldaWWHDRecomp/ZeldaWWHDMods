# Proposed SDK audio stream service

Call of the Sky currently omits its custom synthesized melody. This proposal
is deferred; it adds no service or implementation to the current port.

A generic service could accept bounded, signed 16-bit interleaved PCM from
guest memory, at explicitly enumerated sample rates and one or two channels.
Opening returns a per-mod opaque handle. Submission copies samples into a
fixed-capacity host queue and returns the accepted frame count without
blocking the game thread. A query reports queue capacity; close releases the
stream. No guest pointers survive a call. Total handles and queued bytes are
limited per mod and across all mods. Invalid dimensions, overflow and stale
handles return explicit errors. Mods generate their own audio or read legally
redistributable content through existing per-mod file services.

The host mixer applies user volume and mute settings. Headless/no-audio mode
accepts calls with documented silent behavior and never waits for a device.
Disabling a mod or restarting closes its streams. Loading a full state clears
host queues and advances a stream epoch so a restored guest handle cannot
refer to a later queue; mods recreate streams after an epoch change. Audio
already played is not rewound. Device changes follow the same invalidation
rule. Queues and backend objects are not serialized into guest save states.

Before implementation, agree on queue limits, supported sample rates,
underrun behavior and the silent-mode clock. Tests must cover overflow,
cross-mod handle ownership, device loss, mute, disable/restart, full-state
restore and Metal/Vulkan-independent mixing. The service must not expose
network access, host filesystem paths or executable callbacks.

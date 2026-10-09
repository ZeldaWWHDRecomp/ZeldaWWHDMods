# Call of the Sky

SDK v2 PowerPC guest port of the optional Dragon Roost quest and Valoo ride.
The original built-in prototype remains in `../legacy/dragon/`.

Install through Mods → Browse, complete the guest build, enable code mods and
restart. The pinned SDK in `../sdk.json` is required; the release number alone
does not establish compatibility. USA and European address translation is
performed by the port. Panels use the shared Metal/Vulkan HUD service.

The quest offers a letter after obtaining the Deku Leaf and defeating the
Dragon Roost boss. Read it with D-pad Left, meet the additional Medli on the
island, and follow the chime and song instructions. B cancels dialogue or a
ride request. Progress is stored separately for each of the three save slots
in this mod's data folder. New-game initialization resets the corresponding
progress file. Full save states retain guest quest and ride state; HUD epoch
changes do not reload newer progress files over restored state.

The synthesized song melody is omitted. The game sound calls used for chimes
do not provide an established interface for this custom melody. See
[the proposed generic audio service](../docs/audio-stream-service.md).

This is a testing implementation, not a release approval. Guest compilation,
regional translation and synthetic quest/recovery tests have passed; normal
quest completion, actor cleanup, ride state restoration and the complete
renderer/timing matrix still require gameplay verification. The opt-in
`tests/run_runtime_e2e.py` harness records private evidence and distinguishes
normal routes from instrumented failure fixtures. No game assets or saves
are included. The prototype's custom flight and quest remain optional
gameplay modifications rather than ordinary story progression.

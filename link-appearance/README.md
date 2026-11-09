# Link's look

A dyer in Windfall's town square changes the colours of Link's hair, skin, eyes, tunic and hat,
undershirt, trousers, boots and belt buckle. Talk to her and the port's colour page opens: Link stands
in a close camera on the left, the page is on the right in the game's menu style, and every colour you
pick is shown on Link at once.

Needs a port with Mod SDK manager 1.11.0 (the `appearance` story action and the colour page) and the
Story Notebook package (the port's story system).

What the player gets:

1. **The dyer:** a townswoman (the game's own character, read from your game) waits in Windfall's town
   square, day and night, whatever the story.
2. **Her talk:** three lines, then the colour page opens.
3. **The colour page:** one row per area with its colour; Left / Right step through the colours (the
   first, "Original", is the game's own), "Original look" puts all of them back. The right stick or
   L / R turn the camera around Link to see him from the front and the back. **Done** (A, or +) keeps
   the look, **Back** (B) puts back the look you came with.
4. **The save:** the look belongs to the save file and is kept when you save the game, like your
   progress. A new game starts with Link's own colours.

Only colours change: the port recolours Link's own textures from your game in memory. This package
ships no art and no game material; everything is data in `manifest.json` (the placed character and her
talk). The code only notes in the log when a save file is played. The texts are original.

In online co-op, the other players in your party see your look on your Link (players outside your
party see your co-op tunic colour).

See [Placing actors](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp/blob/devel/docs/mod-sdk-v3.md#placing-actors)
and [Link's appearance](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp/blob/devel/docs/mod-sdk-v2.md#links-appearance)
in the port's SDK documentation.

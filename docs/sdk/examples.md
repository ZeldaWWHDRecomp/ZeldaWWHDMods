# Worked examples

Three story packages of this repository, from the smallest to the largest, and QoL extras for live
options, touches and the microphone. Each folder is a complete mod: read its `manifest.json`,
`src/mod.c` and README side by side with this page.

## The Old map

[`story-chart/`](../../story-chart/): a treasure chart on its own, no quest and no notebook entry,
and nothing in code. It needs port 0.3.0.

**The manifest** declares the chart and the letter that brings it:

```json
"dependencies": [{"id": "story-notebook", "minimum_version": "0.6.0"}],
"charts": [
  {"id": "old-map", "name": "Old map",
   "description": "An old map. An X marks a spot in the open sea south-west of Windfall.",
   "place": {"x": -3500, "z": -194000}, "item": "game:purple_rupee"}
],
"letters": [
  {"id": "map-letter", "sender": "A sailor",
   "text": "Dear Link,\nI found this old map in a bottle.\nMaybe you can find what it marks.",
   "encloses": ["chart:old-map"]}
]
```

**What the player gets:**

1. **The letter:** once Link has the Delivery Bag, the sailor's letter waits at any Rito postbox.
   The port sends it while the mod is on, once per save file.
2. **The MAP list:** after reading it, Link owns the Old map. The MAP screen's chart list shows it
   after the game's charts. The sea chart shows nothing for it yet, as for an owned treasure chart.
3. **Open the chart once:** the game's chart view shows the X at the spot, in the open sea
   south-west of Windfall (square D2).
4. **The light ring:** from then on, the game's ring of light marks the spot while Link sails there.
5. **The crane:** in the ring, the grappling hook from the boat brings up the treasure chest, as for
   the game's own treasures.
6. **The chest:** it holds a purple rupee (the chart's `item`).
7. **The red circle:** the chart is finished, and the game's red hand-drawn circle marks the spot on
   the sea chart and in the chart view.

The save file remembers the letter, the chart, whether it was opened and whether it is finished.

**The code** only logs the chart's state with `wwhd_chart_state("old-map")` (0 not yet, 1 owned, 2
finished), which is a good way to follow the chart while testing.

What to take from it: a treasure chart is data. `charts[].place` is in the sea's world coordinates,
the same as Link's position; put the spot in open water so the boat can reach the ring. Don't wrap a
chart in a quest whose only step is to find it: the port warns about that, and a letter does the job.

## The Story Notebook

[`story-notebook/`](../../story-notebook/): the story system's required package and a quest with no
code at all.

**The quest** "The storyteller's lantern" is data in the manifest:

- its `letter` encloses `"notebook"` (only this package may) and a sealed letter
  (`"letter:friend"`, described in its own words). Reading it brings the notebook and starts the
  quest at step 1;
- its one step names a resident by neutral id, `windfall-um1` (the man up on the windmill by the
  light; see [the NPC catalogue](story.md#the-npc-catalogue)). While Link carries the sealed letter,
  that resident says the quest's three lines, takes the letter, gives an orange rupee and completes
  the quest (`then`);
- the step's `sail_line` has the King of Red Lions remind Link once where the letter goes, while the
  quest is shown on the chart;
- `"npc_catalogue": 3` says which catalogue version the quest needs.

**The look** is the `notebook` block: the Delivery Bag icons (closed and open), the 3D model and the
page layout with its pictures, all in `assets/` and listed in `assets/ASSETS.md`. See
[Art, layouts and licences](art.md#the-story-notebooks-look).

**The code** only logs the quest's state with `wwhd_quest_state("lantern")`, which is a good way to
watch a manifest quest while testing.

## Call of the Sky

[`dragon/`](../../dragon/): a larger mod that mixes a manifest quest with its own gameplay code: an
optional song quest on Dragon Roost Island and a ride across the Great Sea. It is marked
experimental.

**The quest** "Valoo's Gratitude" is declared in the manifest (`quests`): title, start text, reward
and three steps, each with `where` and a `place` on the island, so the notebook shows it and can put
it on the chart.

**The letter comes from code,** because it depends on the game: only once Link has the Deku Leaf and
has helped the dragon does the mod call `wwhd_mail_send("medli", ...)`; when `wwhd_mail_state`
reports it read, the quest reaches step 1.

**Progress is reported from code:** the mod's own state machine (meeting at the high entrance,
striking three wind chimes, conducting the song) maps to the quest's steps with `wwhd_quest_step`,
the three chimes to quest flags 1 to 3 with `wwhd_quest_set_flag`, and the end to `wwhd_quest_done`.
Reading the record back (`wwhd_quest_state`, `wwhd_quest_flag`) after a state load keeps the mod in
step with the save, so progress follows the Quest Log, its copies and save states.

**Structure:** one translation unit `src/mod.c` that includes implementation headers (`*_impl.h`)
for the quest, ride, HUD and persistence; pure logic (state transitions, flight maths, HUD layout)
lives in plain headers that `tests/` compiles for the host and checks with
`python3 -m unittest discover -s dragon/tests -p 'test_*.py'`.

What to take from it: describe the quest as data, keep the code for what only it can know, and make
pure logic testable on the host.

## QoL extras

[`qol/`](../../qol/): many small extras in one package, each with its own switch that the player
changes while playing. It needs port 0.5.0 (`"minimum_manager_version": "1.6.0"`); the services are in
[Live options, input filters and touches](c-api.md#live-options-input-filters-and-touches).

**One of each per mod.** A mod has one HUD callback, one touch callback and one input filter, so
`src/mod.c` registers them once from Link's step and hands each feature its part. A touch goes to the
first feature that says it was its own.

**Live options** (`mod.c`): every option is `"live": true`. A small table pairs each option with the
feature's "changed" function; `wwhd_config_changed` is checked each step and on each controller read,
and a feature reads its value again when the revision moved.

**Touches that press buttons** (`src/kaboom.c`): while Sploosh Kaboom's board is played, the mod claims
the board's rectangle; a tap puts the game's cursor on that square, and the input filter then holds A
for a moment, so the game fires as if the player pressed it. When the board goes, the claim goes.

**Touch buttons on its own HUD** (`src/songs.c`): while Link holds up the Wind Waker, the song buttons
and note pads are drawn with the HUD API and claimed in two slots, removed again when he lowers it.

**Points on the picture and pinches** (`src/minigames.c`): the picture puzzles and the mail sorting
find what the finger is on with `wwhd_screen_point`; at the Nintendo Gallery a drag turns the figurine
and `WWHD_TOUCH_PINCH` zooms (the mouse wheel on a computer).

**The microphone** (`src/sing.c`): "Sing the songs" is a bool option with `"permission":
"microphone"`, off by default. The microphone is opened once when the player turns it on, so the
question comes then and never at the game's start; after the answer it is closed again and opened only
while Link conducts. Each controller read takes the newest pitch from `wwhd_mic_pitch` when it is loud
and clear enough, and turns the notes into a song. A declined question turns the switch off, done by
the port.

What to take from it: keep features off and hooks cheap until their option is on, claim the picture
only while you show something to touch, and open the microphone only while you listen.

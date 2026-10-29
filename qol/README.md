# QoL extras

Small gameplay extras in one package. Each has its own switch, all **off** by default, and each follows a
change at once while you play: in **Settings → Mods → QoL extras**, or on the item menu's **Settings** page
(press + and go left of the items), Mods tab.

| Extra | Section | What it does |
| --- | --- | --- |
| **Link's clothes** | Link | The Hero's clothes or the casual clothes (the lobster shirt) at any time. "As in the story" gives Link the clothes the story gave him. |
| **Replay conducted songs** | Songs | After you conduct a song, Link plays it back before its effect, as on the GameCube (the HD version skips the replay for songs you played before). |
| **Songs by touch** | Songs | While Link holds up the Wind Waker: a button for each song he knows (a tap plays it, as if conducted; a scene that waits for one song shows only that one) and four note pads to conduct by touch. |
| **One-hit kills** | Combat | Every enemy goes down with any hit. Bosses that count their hits are not changed. |
| **Minigames by touch** | Minigames | Sploosh Kaboom: tap a square to aim there and fire. The Cabana's picture puzzles: tap a piece in the gap's row or column, or drag it towards the gap, and it slides there (with the pieces between). Mail sorting at the Rito post: tap a slot to throw the letter there. Spectacle Island's cannon game: drag to turn the cannon (sideways: left and right; up and down: farther and nearer), tap to fire. The auction: tap anywhere to press A (each tap raises your gauge; the bid's number is entered with the port's **Talks, answers and shops by touch**). The Nintendo Gallery: while you look at a figurine, keep a finger on the picture and move it to turn the figurine, pinch to zoom (the mouse wheel on a computer). |
| **Picto subjects** | Picto Box | In the Picto Box album, a label over the picto says whether it is good for a Nintendo Gallery figurine, and if not, why: black and white, turned away or out of view, partly hidden, or no figurine subject. |

More extras will join this package (album pages, mailing pictos at postboxes, singing the songs); each one is a
switch of its own.

## Needs

- The port with the Mod SDK's live options, input filters and touches (the QoL track's SDK addition); the
  minigames by touch also need its world points on the picture and pinches (`wwhd_screen_point`, `WWHD_TOUCH_PINCH`).
- Code-mod support (Mods → More → Code-mod support → Use code mods; on Android also the setup app's
  **Code mods** switch): turning it on rebuilds the game code once. On Android that takes about as long as
  the first setup and runs in the background; close and reopen the app when it is done. The package is then
  built on the device when the game starts.

## Notes

- Touch features work with a finger on a touch screen and with the mouse on the TV picture on a computer.
  On Android they take the finger with the on-screen controls shown or hidden; on the GamePad picture
  (picture-in-picture, or the phone showing the GamePad) the game's own touch keeps it.
- Android: built and run the same way as on a computer, not yet tested on a phone.
- Nothing here changes your save file on its own: the clothes change Link's model while you play; the story
  keeps its own choice.
- Ideas from the GreenNaugahyde Android fork of the port (MPL-2.0), reimplemented as a guest mod.

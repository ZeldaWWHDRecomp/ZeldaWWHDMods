# QoL extras

Small gameplay extras in one package. Each has its own switch, all **off** by default, and each follows a
change at once while you play: in **Settings → Mods → QoL extras**, or on the item menu's **Settings** page
(press + and go left of the items), Mods tab.

| Extra | Section | What it does |
| --- | --- | --- |
| **Link's clothes** | Link | The Hero's clothes or the casual clothes (the lobster shirt) at any time. "As in the story" gives Link the clothes the story gave him. |
| **Replay conducted songs** | Songs | After you conduct a song, Link plays it back before its effect, as on the GameCube (the HD version skips the replay for songs you played before). |
| **Songs by touch** | Songs | While Link holds up the Wind Waker: a button for each song he knows (a tap plays it, as if conducted; a scene that waits for one song shows only that one) and four note pads to conduct by touch. |
| **Sing the songs** (experimental) | Songs | While Link holds up the Wind Waker, sing, hum or whistle a song's notes into the microphone and he plays it, as if conducted (any key, any octave, any tempo; only songs he knows). A line at the top shows what is heard. |
| **One-hit kills** | Combat | Every enemy goes down with any hit. Bosses that count their hits are not changed. |
| **Minigames by touch** | Minigames | Sploosh Kaboom: tap a square to aim there and fire. The Cabana's picture puzzles: tap a piece in the gap's row or column, or drag it towards the gap, and it slides there (with the pieces between). Mail sorting at the Rito post: tap a slot to throw the letter there. Spectacle Island's cannon game: drag to turn the cannon (sideways: left and right; up and down: farther and nearer), tap to fire. The auction: tap anywhere to press A (each tap raises your gauge; the bid's number is entered with the port's **Talks, answers and shops by touch**). The Nintendo Gallery: while you look at a figurine, keep a finger on the picture and move it to turn the figurine, pinch to zoom (the mouse wheel on a computer). |
| **Picto subjects** | Picto Box | In the Picto Box album, a label over the picto says whether it is good for a Nintendo Gallery figurine, and if not, why: black and white, turned away or out of view, partly hidden, or no figurine subject. A figurine subject is named ("Good for a figurine: Sturgeon"), in the language your game shows: the name comes from the game's own text for that figurine. |
| **Album pages** | Picto Box | More pages of 12 pictos in the Picto Box album. In the album, **L** turns to the next page and **ZL** back (or tap the arrows at the top of the album). Turning past the last page starts a new, empty page; an empty page at the end goes again when you leave it. |
| **Mail pictos at postboxes** | Picto Box | Stand in front of a postbox of the Island Postal Service and use the Picto Box (its item button): choose a picto, and the postbox takes the figurine order Carlov would take at the Nintendo Gallery (the same checks: a colour picto of a gallery subject not made or ordered yet, face and body seen; up to 12 orders). The picto leaves your album, as when you give it to Carlov. The figurine is ready the next day: Carlov hands it over at the gallery as usual. While this is on, the Picto Box can't take pictures while you face a postbox. |
| **More figurine orders** | Picto Box | Carlov takes more than 12 figurine orders at a time. Orders beyond his 12 wait in a list of this package and go to him when a day passes and he has room again; they are ready that day. Works at the Nintendo Gallery and, with **Mail pictos at postboxes**, at the postboxes. |


## Needs

- The port with the Mod SDK's live options, input filters and touches; the
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
- Sing the songs and the microphone: off by default. Nothing asks for the microphone at install or start. When you
  turn **Sing the songs** on, the system asks you then (Android's microphone permission, the macOS prompt); if you
  decline, the switch goes back off with a short note, and you can allow the microphone later in the system settings.
  After the question the microphone is closed again and opens only while Link holds up the Wind Waker; turning the
  switch off closes it at once. The sound is only analysed for its pitch by the game while you play: it is never
  stored, written, logged or sent anywhere, and this package never gets the sound itself, only the pitch.
- Album pages: page 1 is always the game's own album, the only page the game saves. The other pages are files of
  this package in its own data folder (Mods data, `qol`), one set per save file; they are written when you turn a
  page and when the game saves. When the game saves while another page is shown, page 1 is put back for the save, so
  your save file is the same as without this package. Turning the option off shows page 1 again at once; your other
  pages stay in their files for when you turn it on again. A save state brings back the page shown at that moment;
  the other pages keep their latest contents. If a page's file is lost or damaged, only the pictos in it are lost.
  If a save file's album has nothing in common with the pages made for it (a new or copied save file), it starts
  with new pages; the old ones stay in the folder.
- Mail pictos at postboxes: an order made at a postbox is exactly Carlov's own order (his checks, his order list,
  the same flags), so your save holds the same as after giving him the picto; nothing else is saved.
- More figurine orders: the game itself keeps at most 12 orders, and your save file never holds more. Carlov
  checks every picto as usual (a picto of a figurine that waits counts as on order) and takes it from your album as
  usual; with his 12 orders taken, the new one waits in this package's data folder (Mods data, `qol`, one list per
  Quest Log). When a day passes and he has room (after you picked up his figurines), waiting orders go to his list,
  oldest first, up to 12; they are ready that day. The list follows your saves: it is written when the game saves
  and read when you load that Quest Log, so it always matches your save file. A copied or new Quest Log starts
  without waiting orders. Turning the option off keeps the waiting orders in the list (and in the file): they go on
  when you turn it on again. While it is off, Carlov takes 12 orders as in the game, and a waiting figurine you
  order from him directly leaves the list.
- Nothing here changes your save file on its own: the clothes change Link's model while you play; the story
  keeps its own choice.
- Ideas from the GreenNaugahyde Android fork of the port (MPL-2.0), reimplemented as a guest mod. The figurine's
  name on the Picto subjects label and More figurine orders are ideas from @gogokfen (issue #276); thanks!

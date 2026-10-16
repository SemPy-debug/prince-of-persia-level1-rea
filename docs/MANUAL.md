# Player manual

This native Windows reconstruction contains the first level of the original 1989 Apple II Prince of Persia. It ends after you complete that level.

## Start and window controls

Extract the Windows game ZIP, open **PrinceOfPersia-Level1.exe**, and press **Enter**. Keep the manual and license files with the executable.

| Key or action | Effect |
| --- | --- |
| Enter or Space | Pause / resume |
| R | Start level one again as a new game |
| M | Mute / unmute |
| Escape | Quit |
| Drag the window border | Resize the window |

Losing window focus pauses the game. Click back into the game and press Enter to resume. Original music temporarily pauses gameplay and finishes automatically; it does not need a dismissal key.

## Movement

| Move | Controls |
| --- | --- |
| Run or turn | Hold Left or Right. Release to stop. |
| Careful step | Stop running first. Hold Shift and tap Left or Right. Release the arrow between steps. Shift is not a brake. |
| Jump straight up | Up. |
| Standing jump | From a stop, press Up and Left or Right together. |
| Running jump | Build up a run, keep that direction held, then press Up before the gap. |
| Crouch | Hold Down. Release to stand. |
| Crouched step | Keep Down held, then tap the direction the Prince is facing. Turn before crouching if you need to go back. |

Turns, landings and attacks take time to animate. Let one move settle before beginning another. A running jump travels farther than a standing jump.

## Ledges

| Move | Controls |
| --- | --- |
| Climb up | Face a reachable ledge and press Up. |
| Catch a ledge | Hold Shift while jumping or falling within reach. |
| Stay hanging | Keep Shift held. |
| Pull up | While hanging, keep Shift held and press Up. |
| Let go | Release Up, then release Shift while hanging. |
| Climb down | Carefully approach the edge, turn to face away from the drop, then press Down. Hold Shift to hang. |

Hold Shift during a doubtful jump so the Prince can catch a reachable far ledge. Releasing Shift while hanging makes him drop.

## Items, gates and the exit

**Collect the sword or drink a potion:** face the item and tap Shift nearby. If the Prince only kneels, hold Down and tap Shift again once he is crouched. Release Shift between presses.

**Open gates:** step on the appropriate floor pressure plate. Some plates open a linked gate; others close one. There is no separate use-gate key. Cross a timed gate before it closes.

**Finish the level:** activate the plate linked to the final exit, reach the opened doorway, and press Up. The game stops after the level-one exit sequence.

## Sword fighting

You begin without a sword. Find and collect it before fighting a guard.

| Move | Controls |
| --- | --- |
| Draw the sword | Usually automatic near a guard. Shift draws it again when an opponent is close enough. |
| Strike | Tap Shift. Release, then press again for another blow. |
| Block / parry | Tap Up as the guard's attack approaches. |
| Counterattack | After a successful block, tap Shift. |
| Advance | Tap the direction you are facing, toward the guard. |
| Retreat | Tap the opposite direction. |
| Put the sword away | Down. Running and jumping controls then apply again. |

Time your strikes and parries. Holding Shift continuously does not replace separate attack presses. If the Prince runs instead of fencing, check that you have collected the sword and that the guard is within engagement range.

## Troubleshooting

- **Nothing moves:** click inside the game window and press Enter. The game starts paused and also pauses when it loses focus. During an original music cue, wait for it to finish.
- **The Prince will not take an item:** face it, stop nearby, release Shift, and press Shift again. Try the crouched action described above.
- **A gate will not open:** find its linked pressure plate. A different plate may close it.
- **No sound:** check the game's M mute toggle and the Windows audio output and volume. The game can continue if no audio device is available.
- **Restarting:** R starts a new level-one game; progress is not saved between runs.
- **Launch fails:** use the Windows x64 game release, extract it first, and record the exact Windows message. No administrator privileges are required by the game.

The controls reflect this build's event handling and movement/combat implementation. Modern pause, restart, mute and quit shortcuts belong to this native host.


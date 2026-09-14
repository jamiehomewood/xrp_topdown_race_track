# XRP Top-Down Race (Igloo 360° room)

Experiment 01 from [PROJECT_BRIEF.md](PROJECT_BRIEF.md): a 4-player top-down arcade racer projected on the floor
of a 5 m × 6 m Igloo room, with a trees-and-sky backdrop on the walls.

## Getting it on a new PC

1. Install Unreal Engine **5.7** and Git with **Git LFS** (`git lfs install` once).
2. `git clone https://github.com/jamiehomewood/xrp_topdown_race_track.git`
   **Don't use GitHub's "Download ZIP"**: ZIP archives contain Git LFS pointer stubs instead of the real
   `.uasset` / `.umap` files, so every asset would be broken.
3. Open `xrp_TopDownRace/xrp_TopDownRace.uproject`. It starts in `/Game/RaceTrack/Maps/Lvl_RaceTrack`.

The compiled editor module (`Binaries/Win64`: dll, `.modules`, `.target`) is committed, so the project opens
on any UE 5.7.x without Visual Studio. If Unreal still asks to rebuild `xrp_TopDownRace`, install Visual Studio
2022 with the "Game development with C++" workload and let the editor rebuild it.

## Layout

| Path | What |
| --- | --- |
| `xrp_TopDownRace/Content/RaceTrack` | Generated track meshes, grey materials, the level |
| `xrp_TopDownRace/Content/Fab`, `LowPolyNatureLite` | Low-poly cars, fences, trees, nature props |
| `xrp_TopDownRace/Source/xrp_TopDownRace` | Car pawn, player controller (join + controls), game mode, input settings |
| `xrp_TopDownRace/Plugins/IglooUnrealToolkit` | Igloo Manager + Spout (prebuilt for 5.7) |
| `xrp_TopDownRace/Tools/TrackGen` | Python that generates the track, scenery and Igloo setup |

World scale is **1 physical cm = 10 UU**: the room floor is 5000 × 6000 UU centred on the origin.

## Playing

All four cars are always on track, with models picked at random from the Fab car pack each race (all scaled to
the same length so a van or monster truck isn't bigger than a sports car). Cars without a player are driven by
the computer (each with its own skill, wobble and occasional mistakes: braking late, running wide, oversteer,
hesitating); pressing any button on
controller N takes car N over straight away, whatever the race is doing. A player's car has a pulsing disc in the
player's colour underneath and a "P1".."P4" roof badge; CPU cars show a grey number. After 60 s without input
(`IdleReleaseSeconds`) a car goes back to the computer.

- Pad: RT or A accelerate, LT or X brake/reverse, left stick or D-pad steer, **B or RB handbrake**.
- Keyboard (player 1): W/S or Up/Down, A/D or Left/Right, **Space handbrake**; Enter takes the car.
- Handling is GTA 2 style: cars slide, bounce off walls and each other, and spin from off-centre hits.
  Throttle + handbrake + full lock spins the car into a doughnut, with tyre squeal.

### Race

- Cars line up on the grid, five red lights come on one per second, then all go out after a random pause: go.
  Lights show on a gantry over the start line (floor) and on boards on the front and back walls.
- `RaceLaps` laps (default 5). Laps only count after passing both sector checkpoints. The race ends the moment
  the first car completes the last lap: everyone else is placed where they are, all cars roll to a stop, and the
  winner is celebrated for 11 s — "PLAYER n WINS!" turning slowly above the middle of the track, confetti pouring
  from the roof line of every wall down onto the floor, a fanfare from every speaker — then everyone is back on the grid.
- **Slipstream**: right behind another car you gain up to +22 % top speed and +60 % acceleration, so you can
  pull out and pass on the straights (`bDraftingEnabled`).
- Displays: a floor panel in each room corner (player/CPU, position, lap, lap time, best, DRAFT) and a banner +
  leaderboard on the front and back walls.
- **Computer drivers** (Race Settings › Computer Drivers, or `DefaultGame.ini`): each race every CPU car gets a
  random pace between `CpuPaceMin` and `CpuPaceMax` (0.78–0.9 of full speed), makes a mistake every
  `CpuMistakeGapMin`–`CpuMistakeGapMax` seconds (3–7), and `CpuSpinChance` (35 %) of those are a full spin in a
  corner. With `bCpuEaseOffWhenAhead`, CPU cars ahead of the best-placed player slow to `CpuEaseOffPace` (0.85) so
  players can catch up. Raise the pace values if the CPUs get too easy.
- Project Settings › Game › Race Settings: laps, slipstream, computer drivers, input toggles, idle release, audio, performance.

### Tracks (a new one every race)

- With `bNewTrackEachRace` (on by default; also in the settings menu) every race gets a new random circuit, built
  in the game when the cars line up: road, kerb walls, start line and gantry, a fence round the outside, and low
  scenery scattered over the rest of the room floor. Turn it off to always race the original track.
- A layout is the outline of a random group of cells on a 2-column, 2-4 row grid with random cut positions (the
  original track is one of them), so the road never crosses itself and corners are rounded right angles. The
  direction and the start straight are random too. Plain ovals (kept 10% of the time) and L-shapes (40%) are made
  rarer in favour of twistier 8–12 corner layouts. Random tracks use a footprint 30 cm bigger on every side than the
  original (outer kerb ~36 cm, fence ~27 cm from the room walls). Each layout is checked before use: inside that footprint,
  straights long enough for the corners, grass between neighbouring stretches of road, a lap of at least 9 m,
  and a straight with room for the grid behind the start line.
- **Narrow sections** (`bNarrowTrackSections`, on by default; also in the menu): most random tracks get one or
  two stretches on straights where the road tapers down to 3–4.2 m (in UU, roughly two car widths) with rocks and
  bushes beside the kerb, so not every car fits through side by side. Never on corners or the start grid. CPU
  drivers keep inside the narrowed walls and lift for the squeeze (unless they're braking late).
- Laps, positions, slipstream and the computer drivers all follow whichever track is built.
- **Mountains**: two overlapping ranges of low-poly mountains and hills all the way round (about 12 km and 21 km
  out, peaks 5–14° above eye level) hide the flat horizon line on every wall. The horizon itself stays at the Igloo
  eye height (1.7 m): lowering the cameras would make the floor projection bigger than the room.
- The level still contains the editor-built original track and floor scenery (Tools/TrackGen); the game hides
  them at start. The trees beyond the walls, the ground and the horizon are shared by every track.

### Settings menu (in game)

- **Tab** on the keyboard, or the **View / Back** button on a controller, opens the menu for that player; the same
  button (or B / Esc) closes it. It shows on boards on the front and back walls in the room and on the desktop.
- Up/Down (stick, D-pad, arrows, W/S) picks a row, Left/Right changes it (hold to repeat), A / Enter runs an action.
  While the menu is open that player's car gets no input; the other cars keep racing.
- Rows: laps, new track each race, narrow sections, slipstream, CPU pace / mistakes / spins / easing off, top speed, acceleration, steering, grip,
  handbrake grip, wall and car bounce, engine and beep volume, idle time before a player's car goes back to the CPU,
  then RESTART RACE, RESET ALL TO DEFAULTS and CLOSE. Changes apply straight away (CPU pace from the next race).
- Changes are saved when the menu closes, per machine, to `Saved/RaceSettings.ini` (only values that differ from
  `Config/DefaultGame.ini`), so the repository's defaults are never touched. Delete that file or use RESET ALL TO
  DEFAULTS to go back to the project defaults. Settings changed there override `-ini:` command-line overrides.

### Test aids (console / `-ExecCmds`)

- `race.AutoDrive 1..4` with `race.AutoDriveCars N`: players take cars and drive by themselves — 1 straight,
  2 wall-escape pattern, 3 the computer driver, 4 doughnuts. Logs position, speed, FPS, slipstream and spin.
- `race.DraftEnabled 0/1` overrides the slipstream setting; `race.CaptureAt 8,40` saves the Igloo floor/wall
  camera images to `Saved/RaceCaptures` at those times (inside `-ExecCmds`, which splits on commas, write `8+40`).
- Quick finish test: `-ini:Game:[/Script/xrp_TopDownRace.RaceInputSettings]:RaceLaps=1` gives one-lap races.
- `race.MenuTest 8+6+1`: 8 s in, player 1 opens the settings menu, moves down 6 rows, changes it one step and
  closes (saves) 4 s later.
- `race.TrackSurvey 2000` generates and checks that many random tracks and logs the shapes and lap lengths;
  `race.TrackSeed N` makes the tracks repeatable; `race.TrackCycle 10` restarts the race on a new track every 10 s.

## Performance

Igloo renders its capture cameras (6 by default) plus the desktop window every frame, so the renderer is set up
for cheap views: Lumen GI/reflections, hardware ray tracing, virtual shadow maps and mesh distance fields are off
(`Config/DefaultEngine.ini`). The desktop window renders at `DesktopViewScreenPercentage` (Race Settings, default
50 %); Igloo's captures always render at full size. The room has no ceiling projector, so Igloo's upward-facing
capture camera is switched off at runtime (`bRenderIglooCeilingCamera`, Race Settings); its tile in the Spout feed
stays black and the feed layout is unchanged. If the room is still slow, lower the Igloo output with
`-iglooSetResolution` or `-iglooNumCams`.

## Audio

Each car has a procedural engine voice (`RaceEngineSynth`, no sound assets): pitch follows speed, roar follows
throttle, and wall/car hits add a scrape. Sounds are 3D at the car, and the audio listener sits at the centre of
the room floor, so each car is panned to the speakers on its side of the room as it drives around.

The room has six speakers: two on the front wall, one on each side wall, two on the back wall (no centre, no sub).
The front wall is the level's +Y side: the long straight runs along it, and the start-line straight runs along
the back wall.

- Windows' sound device on the room PC must be set to **7.1**, with the six speakers on the front L/R, side L/R and
  back (rear) L/R outputs; centre and subwoofer outputs are unused. Unreal outputs whatever layout Windows reports.
- Race Settings > `AudioFrontYaw` = 90: the listener faces the front wall (+Y).
- `Config/DefaultEngine.ini` > `[AudioChannelAzimuthMap]`: speaker angles clockwise from the front wall, an even
  ring: FrontRight 30, SideRight 90, BackRight 150, BackLeft 210, SideLeft 270, FrontLeft 330. Centre-channel
  panning is off. Adjust the angles if the speakers are mounted far from even spacing.
- Race Settings > `EngineVolume`: overall car sound level.
- Quick check in the room: a car on the long straight should sound from the front wall, a car on the start-line
  straight from the back wall.
- Start lights beep from every speaker (one beep per light, a higher tone at lights out); `SignalVolume` sets the level.
- **If sound comes out of only one or two speakers**, Unreal isn't on the room's multichannel device: it plays to
  Windows' *default* output device with that device's channel count. Make the room's audio interface the default
  output, set its speaker configuration to 7.1, and restart the game. The game logs the device at start
  (`race.Audio output device '...': N channels`) and shows `(AUDIO: N CH)` on the GET READY banner when fewer than
  6 channels are available.
- **Speaker test**: console `race.SpeakerTest 1` beeps each output channel in turn (front left, front right, centre,
  back left/right, side left/right) and names it on the wall banner, to check the wiring.
- Console `race.RecordAudio 30` records the mixed output to `Saved/BouncedWavFiles/RaceAudio.wav` and logs car
  positions, for checking the panning without being in the room; add `race.AudioSweep 1` to hold car 1 on a circle
  round the listener in 45-degree steps (one measurement per speaker direction).
- `[Audio] UnfocusedVolumeMultiplier=1.0` keeps the sound on when the game window isn't focused (e.g. behind Igloo).

## Igloo / Spout

`IglooManager` sits at the scene centre and follows `IglooViewpoint` (room eye height, 1700 UU). It sends the
Spout feed **`IglooUnreal`** with the plugin's default camera count and output size; override with
`-iglooSenderName`, `-iglooNumCams`, `-iglooSetResolution`. The desktop window shows `TrackCamera` (top-down).

## Regenerating the track

Close the editor first. From `xrp_TopDownRace/`:

```
UnrealEditor-Cmd.exe <abs>/xrp_TopDownRace.uproject -run=pythonscript -script="<abs>/Tools/TrackGen/build_track.py meshes"
UnrealEditor.exe <abs>/xrp_TopDownRace.uproject -ExecutePythonScript="<abs>/Tools/TrackGen/build_and_capture.py"
```

Shape and sizes: `Tools/TrackGen/track_geom.py`. Scenery: `Tools/TrackGen/build_scenery.py`.

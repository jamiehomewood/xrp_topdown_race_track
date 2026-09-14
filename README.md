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

- Press any button on a controller to join; controller N drives car N (up to 4, join any time).
- Pad: RT or A accelerate, LT or X brake/reverse, left stick or D-pad steer.
- Keyboard (player 1): W/S or Up/Down, A/D or Left/Right; Enter/Space joins.
- Project Settings › Game › Race Settings toggles gamepad / keyboard input.
- Console `race.AutoDrive 1`: every car joins and drives at full throttle (test without controllers);
  `race.AutoDrive 2` also steers and, like a player, steers the other way when pinned on a wall. Both log
  position, distance moved and FPS each second.

## Performance

Igloo renders its capture cameras (6 by default) plus the desktop window every frame, so the renderer is set up
for cheap views: Lumen GI/reflections, hardware ray tracing, virtual shadow maps and mesh distance fields are off
(`Config/DefaultEngine.ini`). The desktop window renders at `DesktopViewScreenPercentage` (Race Settings, default
50 %); Igloo's captures always render at full size. The room has no ceiling projector, so Igloo's upward-facing
capture camera is switched off at runtime (`bRenderIglooCeilingCamera`, Race Settings); its tile in the Spout feed
stays black and the feed layout is unchanged. If the room is still slow, lower the Igloo output with
`-iglooSetResolution` or `-iglooNumCams`.

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

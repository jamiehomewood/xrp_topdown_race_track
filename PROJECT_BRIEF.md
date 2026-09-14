# Igloo Vision 360° Room — UE 5.7 Project Brief

## Bootstrap
- Check the working folder for an existing Unreal Engine project file (.uproject)
- If no .uproject exists, create a new blank UE 5.7 project in the working folder
  (Game → Blank template, no starter content)
- Once a project exists, open it and confirm it compiles before proceeding
- The Igloo Plugin will be present in the Plugins folder — load and enable it on first run

## Environment
- Physical room: 5m × 6m
- 360° projection across 4 walls and floor
- Output: Equirectangular scene via Igloo Manager plugin (1:8 ratio)
- Igloo Manager handles all projection warping and produces a Spout feed
- Igloo Server handles final output to the room
- Do not modify or remove the Igloo Manager actor from the scene centre

## Project Structure
- Working folder contains this file, the Igloo Plugin, and any supplied 3D assets
- Use supplied low-poly assets where available before creating new geometry
- Game controller input is always built in. A project setting toggles it on/off

## Experiment 01 — Top-Down 4-Player Racer with Physical Ramp
- Classic top-down racing game, 4 players, inspired by arcade-era overhead racers
- Track renders on the floor projection, sized smaller than the physical room footprint so participants can walk around the outside of the track without interfering with the race
- Walls display a low-level backdrop: trees and sky, static or slow parallax
- One player per corner of the room, each with their own controller
- Camera: single top-down orthographic or perspective camera centred above track
- The Igloo Manager will handle equirectangular output — do not attempt custom projection
- Keep geometry low-poly, performant, and readable at floor level
- 4 player split is handled via controller index, not split screen
- Not all 4 players need to be present — cars start inactive
- Picking up a controller and pressing any button activates that player's car and joins the race
- Players can join at any point, old-school arcade style
- One physical 3D printed ramp placed on the floor of the room at a fixed position
- Claude Code decides the ramp position on the circuit for best gameplay flow
- An invisible trigger volume in UE is placed at that position — when a car enters it, the car launches into a jump arc and lands back on the track
- Jump is visual only to start — no physics-based consequences yet
- A ramp placement mode is toggled by the operator via controller shortcut:
  Start + Left Bumper held for 2 seconds
- When active, a visible floor indicator highlights exactly where the physical ramp should be placed
- When deactivated, the indicator disappears and the trigger volume remains active
- The physical ramp position is fixed per session — if moved, the illusion breaks

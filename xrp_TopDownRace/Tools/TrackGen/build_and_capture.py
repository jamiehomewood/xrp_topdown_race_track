"""Editor startup script: build the track level + scenery, grab screenshots, quit.

Meshes must already exist (build_track.py "meshes" stage, run via the commandlet).

UnrealEditor.exe <project>.uproject -ExecutePythonScript="<abs path>/build_and_capture.py"
Screenshots land in Saved/Screenshots/WindowsEditor/.
"""
import os
import sys
import time

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(PROJECT_DIR, "Tools", "TrackGen"))
import build_igloo  # noqa: E402
import build_scenery  # noqa: E402
import build_track  # noqa: E402

FIRST_SHOT_AFTER_S = 50.0   # after the first tick: let shaders compile
SETTLE_S = 5.0              # after moving the camera, before capturing
QUIT_AFTER_LAST_S = 10.0

CAR_CHECK_MESH = "/Game/Fab/Mobile_Optimize-Free_Low_Poly_Cars/Sport_Car_39/StaticMeshes/Sport_Car_39"
CAR_CHECK_YAW = -90.0       # mesh front is +Y, so yaw -90 points it down +X (race direction)
CAR_SCALE = 0.5             # matches ARaceCarPawn::CarScale

SHOTS = [
    ("TrackTopDown", unreal.Vector(0.0, 0.0, 5200.0), unreal.Rotator(0.0, -90.0, 0.0)),
    # Eye height (1.7 m x 10) at room centre, looking at a wall: what the wall projection shows.
    ("TrackBackdrop", unreal.Vector(0.0, 0.0, 1700.0), unreal.Rotator(0.0, -12.0, 0.0)),
    # Car orientation check: camera behind the grid looking down the race direction (+X).
    ("TrackCarCheck", unreal.Vector(-1300.0, -1650.0, 500.0), unreal.Rotator(0.0, -20.0, 5.0)),
]

# -ExecutePythonScript closes the editor on the tick after this file finishes unless the
# script asks to stay alive; _tick quits once the screenshots have been written.
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def build():
    if not build_track.run(do_meshes=False, do_level=True):
        return False
    try:
        build_scenery.build(build_track.log)
        build_igloo.build(build_track.log)
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        build_track.log("level saved with scenery and igloo")
    except Exception:
        import traceback
        build_track.log("SCENERY/IGLOO FAILED\n" + traceback.format_exc())
        return False
    # Unsaved preview car for the orientation screenshot.
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    car = eas.spawn_actor_from_object(unreal.EditorAssetLibrary.load_asset(CAR_CHECK_MESH),
                                      unreal.Vector(0.0, -1950.0, 2.0), unreal.Rotator(0.0, 0.0, CAR_CHECK_YAW))
    car.set_actor_scale3d(unreal.Vector(CAR_SCALE, CAR_SCALE, CAR_SCALE))
    return True


ok = build()
viewport = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
state = {"start": None, "index": 0, "moved_at": None, "last_shot": None}


def _tick(_dt):
    now = time.time()
    if state["start"] is None:
        state["start"] = now
    done = not ok
    i = state["index"]
    if ok and i < len(SHOTS) and now - state["start"] >= FIRST_SHOT_AFTER_S:
        name, loc, rot = SHOTS[i]
        if state["moved_at"] is None:
            viewport.set_level_viewport_camera_info(loc, rot)
            state["moved_at"] = now
        elif now - state["moved_at"] >= SETTLE_S:
            unreal.log("[TrackGen] capturing %s" % name)
            unreal.SystemLibrary.execute_console_command(None, "HighResShot 1920x1080 filename=%s" % name)
            state["index"] += 1
            state["moved_at"] = None
            state["last_shot"] = now
    if ok and state["index"] >= len(SHOTS) and now - state["last_shot"] >= QUIT_AFTER_LAST_S:
        done = True
    if done:
        unreal.unregister_slate_post_tick_callback(_handle)
        unreal.SystemLibrary.quit_editor()


_handle = unreal.register_slate_post_tick_callback(_tick)

"""Place the Igloo Manager (Igloo Unreal Toolkit) so the level sends the room's Spout feed. Editor only.

- IglooManager at the scene centre. Each tick it moves to its "Follow Object"; left empty it would follow
  the player camera, which here is the high top-down TrackCamera used for the desktop monitor. So it is
  pointed at an IglooViewpoint target point at room eye height (1.7 m x WORLD_SCALE = 1700 UU).
- An IglooManager already in the level is never moved, replaced or removed; only an empty Follow Object
  is filled in.
- Igloo's projection settings (camera count, output size, Spout sender name "IglooUnreal") stay at the
  plugin defaults: Igloo Server and the -iglooSenderName / -iglooNumCams / -iglooSetResolution command line
  own those. The crosshair is switched off because it would sit in the middle of the feed.
"""
import unreal

import track_geom as tg

IGLOO_TAG = "TrackGenIgloo"
MANAGER_ASSET = "/IglooUnrealToolkit/IglooManager"
EYE_HEIGHT_CM = 170.0
VIEWPOINT_HEIGHT = EYE_HEIGHT_CM * tg.WORLD_SCALE
TRACK_CAMERA_TAG = "TrackCamera"        # ARaceGameMode::TrackCameraTag
TRACK_CAMERA_HEIGHT = 5200.0             # desktop monitor view, whole room footprint in frame


def _spawn(eas, cls, location, rotation, label, tags):
    actor = eas.spawn_actor_from_class(cls, location, rotation)
    actor.set_actor_label(label)
    actor.set_editor_property("tags", tags)
    actor.set_folder_path("Igloo")
    return actor


def build(log):
    eal = unreal.EditorAssetLibrary
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/IglooUnrealToolkit"], True)
    manager_bp = eal.load_asset(MANAGER_ASSET)
    if manager_bp is None:
        raise RuntimeError("Igloo Manager blueprint not found at %s - is the IglooUnrealToolkit plugin enabled?"
                           % MANAGER_ASSET)
    manager_class = manager_bp.generated_class()
    actors = eas.get_all_level_actors()

    camera = next((a for a in actors if a.actor_has_tag(TRACK_CAMERA_TAG)), None)
    if camera is None:
        camera = _spawn(eas, unreal.CameraActor, unreal.Vector(0.0, 0.0, TRACK_CAMERA_HEIGHT),
                        unreal.Rotator(0.0, -90.0, 0.0), "TrackCamera", [TRACK_CAMERA_TAG, IGLOO_TAG])
        camera.get_editor_property("camera_component").set_editor_property("constrain_aspect_ratio", False)

    viewpoint = next((a for a in actors if a.actor_has_tag("IglooViewpoint")), None)
    if viewpoint is None:
        viewpoint = _spawn(eas, unreal.TargetPoint, unreal.Vector(0.0, 0.0, VIEWPOINT_HEIGHT),
                           unreal.Rotator(0.0, 0.0, 0.0), "IglooViewpoint", ["IglooViewpoint", IGLOO_TAG])

    managers = [a for a in actors if a.get_class().get_path_name() == manager_class.get_path_name()]
    if managers:
        manager = managers[0]
        log("existing IglooManager kept at %s" % manager.get_actor_location())
    else:
        manager = eas.spawn_actor_from_class(manager_class, unreal.Vector(0.0, 0.0, VIEWPOINT_HEIGHT),
                                             unreal.Rotator(0.0, 0.0, 0.0))
        manager.set_actor_label("IglooManager")
        manager.set_folder_path("Igloo")
        manager.set_editor_property("UseIglooCrosshair", False)

    if manager.get_editor_property("Follow Object") is None:
        manager.set_editor_property("Follow Object", viewpoint)

    log("igloo: manager at %s follows %s; spout '%s' %s, %d cameras; monitor camera at z=%.0f" % (
        manager.get_actor_location(), manager.get_editor_property("Follow Object").get_actor_label(),
        manager.get_editor_property("SpoutOutputName"), manager.get_editor_property("OutputSize"),
        manager.get_editor_property("NumberOfCameras"), camera.get_actor_location().z))

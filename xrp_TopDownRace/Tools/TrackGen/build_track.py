"""Build the grey-box race track inside Unreal (UE 5.7, Python Editor Script Plugin).

Creates / regenerates:
  /Game/RaceTrack/Materials/M_TrackGrey + MI_Track_{Road,Wall,Line,Floor}
  /Game/RaceTrack/Meshes/SM_Track_{Road,Walls,StartLine,RoomFloor}
  /Game/RaceTrack/Maps/Lvl_RaceTrack  (track centred on world origin, basic lighting)

Two stages, because each needs a different editor mode:
  1. meshes + materials -> headless commandlet (rebuilding a mesh in the running editor
     crashes: its live render resources get freed without being released)
       UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<abs>/build_track.py meshes"
  2. level -> full editor (level creation needs a level viewport)
       UnrealEditor.exe <project>.uproject -ExecutePythonScript="<abs>/build_and_capture.py"
Actors tagged "TrackGen" are replaced on every run; everything else in the level is left alone.
"""
import importlib
import os
import sys
import traceback

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(PROJECT_DIR, "Tools", "TrackGen"))
import track_geom  # noqa: E402
importlib.reload(track_geom)

ROOT = "/Game/RaceTrack"
MAT_DIR = ROOT + "/Materials"
MESH_DIR = ROOT + "/Meshes"
MAP_PATH = ROOT + "/Maps/Lvl_RaceTrack"
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "TrackGen", "build_log.txt")

# Linear greys per material slot.
GREYS = {"Road": 0.16, "Wall": 0.5, "Line": 0.8, "Floor": 0.03}
GEN_TAG = "TrackGen"
LIGHT_TAG = "TrackGenLighting"
EXPOSURE_TAG = "TrackGenExposure"
FIXED_EV100 = 3.0   # ~mid grey under the default 10 lux sun + sky light

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
log_lines = []


def log(msg):
    unreal.log("[TrackGen] %s" % msg)
    log_lines.append(str(msg))
    # Write as we go so a hard engine crash still leaves a trail.
    os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
    with open(LOG_PATH, "w") as f:
        f.write("\n".join(log_lines))


def load_or_create(name, folder, cls, factory):
    path = "%s/%s" % (folder, name)
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    return tools.create_asset(name, folder, cls, factory)


def build_materials():
    parent = load_or_create("M_TrackGrey", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if mel.get_num_material_expressions(parent) == 0:
        color = mel.create_material_expression(parent, unreal.MaterialExpressionVectorParameter, -400, 0)
        color.set_editor_property("parameter_name", "Color")
        color.set_editor_property("default_value", unreal.LinearColor(0.2, 0.2, 0.2, 1.0))
        mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
        rough = mel.create_material_expression(parent, unreal.MaterialExpressionScalarParameter, -400, 250)
        rough.set_editor_property("parameter_name", "Roughness")
        rough.set_editor_property("default_value", 0.9)
        mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.recompile_material(parent)
    eal.save_loaded_asset(parent)

    instances = {}
    for slot, grey in GREYS.items():
        mi = load_or_create("MI_Track_" + slot, MAT_DIR, unreal.MaterialInstanceConstant,
                            unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)
        mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(grey, grey, grey, 1.0))
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        instances[slot] = mi
    log("materials ok: %s" % sorted(instances))
    return instances


def load_or_create_mesh(name):
    # There is no Python-exposed "new static mesh" factory, so start from a copy of an
    # engine shape; its geometry is fully replaced by build_from_static_mesh_descriptions.
    path = "%s/%s" % (MESH_DIR, name)
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    return eal.duplicate_asset("/Engine/BasicShapes/Plane", path)


def build_mesh(name, mesh, material):
    sm = load_or_create_mesh(name)
    desc = unreal.StaticMesh.create_static_mesh_description(sm)
    group = desc.create_polygon_group()
    desc.set_polygon_group_material_slot_name(group, mesh.slot)
    tri_count = 0
    for poly in mesh.polys:
        # track_geom stores right-handed CCW corners; Unreal's front face is the reverse.
        instances = []
        for x, y, z in reversed(poly):
            v = desc.create_vertex()
            desc.set_vertex_position(v, unreal.Vector(x, y, z))
            vi = desc.create_vertex_instance(v)
            desc.set_vertex_instance_uv(vi, unreal.Vector2D(x / 100.0, (y + z) / 100.0), 0)
            instances.append(vi)
        for k in range(1, len(instances) - 1):
            desc.create_triangle(group, [instances[0], instances[k], instances[k + 1]])
            tri_count += 1
    sm.build_from_static_mesh_descriptions([desc], False, False)
    # Static lighting is disabled in this project, and there is only one UV channel:
    # don't let the Plane template's lightmap-UV generation (dst channel 1) run.
    # The editor subsystem isn't available in the -run=pythonscript commandlet.
    mesh_subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary
    settings = mesh_subsystem.get_lod_build_settings(sm, 0)
    settings.set_editor_property("generate_lightmap_u_vs", False)
    settings.set_editor_property("recompute_normals", True)
    settings.set_editor_property("recompute_tangents", True)
    sm.set_editor_property("light_map_coordinate_index", 0)
    mesh_subsystem.set_lod_build_settings(sm, 0, settings)
    sm.set_material(0, material)

    body = sm.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    sm.modify()
    eal.save_loaded_asset(sm)

    b = sm.get_bounding_box()
    log("%s: %d tris, bounds min(%.0f, %.0f, %.0f) max(%.0f, %.0f, %.0f), collision=%s"
        % (name, tri_count, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z, bool(body)))
    check_up_normal(sm, name)
    return sm


def check_up_normal(sm, name):
    """Confirm the built render normals of a flat mesh point up (i.e. winding is correct)."""
    lib = getattr(unreal, "ProceduralMeshLibrary", None)
    if lib is None or name == "SM_Track_Walls":
        return
    try:
        section = lib.get_section_from_static_mesh(sm, 0, 0)
        normals = section[2]
        zs = [n.z for n in normals]
        log("  %s normal z range %.2f..%.2f" % (name, min(zs), max(zs)))
    except Exception as exc:  # informational only
        log("  normal check skipped: %s" % exc)


def build_level(meshes):
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if eal.does_asset_exist(MAP_PATH):
        les.load_level(MAP_PATH)
    else:
        les.new_level(MAP_PATH)

    actors = eas.get_all_level_actors()
    for a in actors:
        if a.actor_has_tag(GEN_TAG):
            eas.destroy_actor(a)

    origin = unreal.Vector(0.0, 0.0, 0.0)
    for name, sm in meshes.items():
        if name == "SM_Track_RoomFloor":
            continue  # footprint reference only; build_scenery lays grass instead
        actor = eas.spawn_actor_from_object(sm, origin, unreal.Rotator(0.0, 0.0, 0.0))
        actor.set_actor_label(name.replace("SM_", ""))
        actor.set_editor_property("tags", [GEN_TAG])
        actor.set_folder_path("Track")

    if not any(a.actor_has_tag(LIGHT_TAG) for a in eas.get_all_level_actors()):
        sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 3000),
                                         unreal.Rotator(0.0, -60.0, 35.0))
        sun.get_editor_property("light_component").set_editor_property("atmosphere_sun_light", True)
        sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 3200))
        sky.get_editor_property("light_component").set_editor_property("real_time_capture", True)
        atmos = eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 3400))
        for label, actor in (("Sun", sun), ("SkyLight", sky), ("SkyAtmosphere", atmos)):
            actor.set_actor_label(label)
            actor.set_editor_property("tags", [LIGHT_TAG])
            actor.set_folder_path("Lighting")

    # Fixed exposure: auto exposure stretches the mostly dark scene to mid grey (the greys all
    # read near-white) and would pump as cars move across the projected floor.
    if not any(a.actor_has_tag(EXPOSURE_TAG) for a in eas.get_all_level_actors()):
        ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
        ppv.set_editor_property("unbound", True)
        pps = ppv.get_editor_property("settings")
        pps.set_editor_property("override_auto_exposure_min_brightness", True)
        pps.set_editor_property("auto_exposure_min_brightness", FIXED_EV100)
        pps.set_editor_property("override_auto_exposure_max_brightness", True)
        pps.set_editor_property("auto_exposure_max_brightness", FIXED_EV100)
        ppv.set_editor_property("settings", pps)
        ppv.set_actor_label("FixedExposure")
        ppv.set_editor_property("tags", [EXPOSURE_TAG])
        ppv.set_folder_path("Lighting")

    les.save_current_level()
    log("level saved: %s (%d actors)" % (MAP_PATH, len(eas.get_all_level_actors())))


def verify():
    """Reload every generated asset and report it; a crash here means the saved assets are bad."""
    for path in eal.list_assets(ROOT, recursive=True, include_folder=False):
        asset = eal.load_asset(path)
        extra = ""
        if isinstance(asset, unreal.StaticMesh):
            b = asset.get_bounding_box()
            extra = " bounds (%.0f..%.0f, %.0f..%.0f, %.0f..%.0f) mat=%s" % (
                b.min.x, b.max.x, b.min.y, b.max.y, b.min.z, b.max.z, asset.get_material(0).get_name())
        log("verify %s -> %s%s" % (path, type(asset).__name__ if asset else "NOT LOADED", extra))


def main(do_meshes, do_level):
    log(track_geom.report())
    geo, _ = track_geom.build()
    if do_meshes:
        materials = build_materials()
        for name, mesh in geo.items():
            build_mesh(name, mesh, materials[mesh.slot])
    if do_level:
        meshes = {}
        for name in geo:
            sm = eal.load_asset("%s/%s" % (MESH_DIR, name))
            if sm is None:
                raise RuntimeError("%s missing - run the 'meshes' stage first" % name)
            meshes[name] = sm
        build_level(meshes)
    log("DONE (meshes=%s, level=%s)" % (do_meshes, do_level))


def run(do_meshes=True, do_level=True):
    """Returns True on success. Writes Saved/TrackGen/build_log.txt."""
    del log_lines[:]
    ok = False
    try:
        main(do_meshes, do_level)
        ok = True
    except Exception:
        log("FAILED\n" + traceback.format_exc())
    finally:
        os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
        with open(LOG_PATH, "w") as f:
            f.write("\n".join(log_lines))
    return ok


if __name__ == "__main__":
    stage = sys.argv[1] if len(sys.argv) > 1 else "meshes"
    if stage == "verify":
        del log_lines[:]
        try:
            verify()
            log("DONE (verify)")
        except Exception:
            log("FAILED\n" + traceback.format_exc())
    else:
        run(do_meshes=stage in ("meshes", "all"), do_level=stage in ("level", "all"))

"""Scatter the low-poly Fab / LowPolyNatureLite scenery around the track (editor only: spawns level actors).

Zones, chosen for the Igloo room (camera above the track centre, floor + 4 walls projected):
  ground   - one big grass tile under everything
  floor    - inside the 5 x 6 m room footprint: only LOW props (fences, bushes, rocks, flowers, tents)
             so nothing towers toward the camera or hides cars
  backdrop - trees in a ring just outside the footprint -> they land on the wall projection
  horizon  - hills and mountains far out, plus light height fog for depth

All actors are tagged TrackGenScenery and replaced on every run. Placement is seeded, so runs are repeatable.
"""
import math
import random

import unreal

import track_geom as tg

SCENERY_TAG = "TrackGenScenery"
SEED = 7

NATURE = "/Game/LowPolyNatureLite/Assets/Models/"
FENCES = "/Game/Fab/Low_Poly_Meadow_Barrier_Bundle__Fences___Walls/"
TREE_SET = "/Game/Fab/Low_Poly_Tree_Set/low_poly_tree_set/StaticMeshes/low_poly_tree_set"


def fence(name):
    return "%s%s/StaticMeshes/%s" % (FENCES, name, name)


# (mesh, weight, footprint radius in UU)
FLOOR_PROPS = [
    (NATURE + "SM_Bush_Simple", 5, 90),
    (NATURE + "SM_Bush_Berries_Red", 2, 110),
    (NATURE + "SM_Bush_Berries_blue", 2, 110),
    (NATURE + "SM_Bush_Berries_Empty", 2, 110),
    (NATURE + "SM_Grass_Array01", 5, 85),
    (NATURE + "SM_Grass01", 6, 45),
    (NATURE + "SM_Grass03", 4, 30),
    (NATURE + "SM_Plant02", 3, 85),
    (NATURE + "SM_Flower02_Orange", 3, 40),
    (NATURE + "SM_Flower02_Pink", 3, 40),
    (NATURE + "SM_Flower02_Yellow", 3, 40),
    (NATURE + "SM_Hat_Mushroom_red", 1, 40),
    (NATURE + "SM_Mushrooom01_brown", 1, 35),
    (NATURE + "SM_Stone02", 3, 50),
    (NATURE + "SM_Stones02", 2, 110),
    (NATURE + "SM_Rock02", 2, 120),
]
BACKDROP_TREES = [
    (NATURE + "SM_Pine01", 6, 120),
    (NATURE + "SM_Tree04", 6, 150),
    (NATURE + "SM_Tree_Dead01", 1, 70),
    (NATURE + "SM_Dead_Tree", 1, 170),
    (TREE_SET, 1, 380),
    (NATURE + "SM_Bush_Simple", 4, 90),
    (NATURE + "SM_Bush_Berries_Empty", 2, 110),
    (NATURE + "SM_Rock02", 1, 120),
]
FENCE_PIECES = [fence("EA03_Fence_Plank_01%s" % s) for s in "abcde"]
FENCE_PIECE_LENGTH = 150.0

eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(SEED)
_mesh_cache = {}
counts = {}


def mesh(path):
    if path not in _mesh_cache:
        _mesh_cache[path] = eal.load_asset(path)
        if _mesh_cache[path] is None:
            raise RuntimeError("missing mesh " + path)
    return _mesh_cache[path]


def place(path, x, y, z=0.0, yaw=0.0, scale=1.0, folder="Scenery", shadows=True):
    actor = eas.spawn_actor_from_object(mesh(path), unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    actor.set_editor_property("tags", [SCENERY_TAG])
    actor.set_folder_path(folder)
    comp = actor.get_editor_property("static_mesh_component")
    comp.set_collision_profile_name("NoCollision")
    comp.set_editor_property("cast_shadow", shadows)
    counts[folder] = counts.get(folder, 0) + 1
    return actor


def weighted(choices):
    total = sum(w for _, w, _ in choices)
    r = rng.uniform(0, total)
    for item in choices:
        r -= item[1]
        if r <= 0:
            return item
    return choices[-1]


def seg_dist(p, a, b):
    ax, ay = b[0] - a[0], b[1] - a[1]
    t = ((p[0] - a[0]) * ax + (p[1] - a[1]) * ay) / max(1e-6, ax * ax + ay * ay)
    t = max(0.0, min(1.0, t))
    return math.hypot(p[0] - a[0] - ax * t, p[1] - a[1] - ay * t)


CENTRE = tg.centreline()
TRACK_CLEAR = tg.TRACK_WIDTH / 2.0 + tg.WALL_WIDTH


def track_dist(p):
    n = len(CENTRE)
    return min(seg_dist(p, CENTRE[i], CENTRE[(i + 1) % n]) for i in range(n))


def point_in_loop(p, loop):
    inside = False
    n = len(loop)
    for i in range(n):
        (x1, y1), (x2, y2) = loop[i], loop[(i + 1) % n]
        if (y1 > p[1]) != (y2 > p[1]) and p[0] < x1 + (p[1] - y1) * (x2 - x1) / (y2 - y1):
            inside = not inside
    return inside


def scatter(choices, count, sampler, clearance, folder, scale_fn=lambda p: 1.0, tries=40, taken=None):
    taken = taken if taken is not None else []
    for _ in range(count):
        for _attempt in range(tries):
            path, _w, radius = weighted(choices)
            p = sampler()
            s = scale_fn(p) * rng.uniform(0.85, 1.2)
            r = radius * s
            if clearance(p) < r:
                continue
            if any(math.hypot(p[0] - q[0], p[1] - q[1]) < r + qr + 20 for q, qr in taken):
                continue
            taken.append((p, r))
            place(path, p[0], p[1], 0.0, rng.uniform(0, 360), s, folder, shadows=r > 60)
            break
    return taken


def build_ground():
    # 6 km across: well past the mountains, so no sky shows below the horizon on the wall projection.
    place(NATURE + "SM_Tile_Grass", 0, 0, 0.0, 0.0, 600.0, "Scenery/Ground", shadows=False)


def build_fences(half_x, half_y):
    _, loops = tg.build()
    offset = TRACK_CLEAR + 90.0
    outer_left = abs(sum(loops["left_back"][i - 1][0] * loops["left_back"][i][1] -
                         loops["left_back"][i][0] * loops["left_back"][i - 1][1]
                         for i in range(len(loops["left_back"]))))
    outer_right = abs(sum(loops["right_back"][i - 1][0] * loops["right_back"][i][1] -
                          loops["right_back"][i][0] * loops["right_back"][i - 1][1]
                          for i in range(len(loops["right_back"]))))
    loop = tg.offset_loop(CENTRE, offset if outer_left > outer_right else -offset)
    # Walk the loop by arc length, dropping a piece centre every FENCE_PIECE_LENGTH.
    next_at = FENCE_PIECE_LENGTH / 2.0
    walked = 0.0
    n = len(loop)
    for i in range(n):
        a, b = loop[i], loop[(i + 1) % n]
        seg = math.hypot(b[0] - a[0], b[1] - a[1])
        yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
        while next_at <= walked + seg:
            t = (next_at - walked) / seg
            x, y = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
            if track_dist((x, y)) > TRACK_CLEAR + 40:
                place(rng.choice(FENCE_PIECES), x, y, 0.0, yaw, 1.0, "Scenery/Fences")
            next_at += FENCE_PIECE_LENGTH
        walked += seg

    # Start/finish posts either side of the line.
    sx, sy = tg.START_LINE_X, tg.CONTROL_POINTS[0][1]
    for side in (-1, 1):
        place(fence("EA03_Wooden_Pin_01d"), sx, sy + side * (TRACK_CLEAR + 60), 0.0, 0.0, 1.0, "Scenery/StartLine")
    return loop


def build_floor(half_x, half_y, taken):
    margin = 60.0

    def sampler():
        return (rng.uniform(-half_x + margin, half_x - margin), rng.uniform(-half_y + margin, half_y - margin))

    def clearance(p):
        return track_dist(p) - TRACK_CLEAR - 130.0   # keep a strip clear of the fence line / kerbs

    # Campsites: one tent in the long infield strip, one in the left-hand bay.
    for path, x, y, yaw in ((NATURE + "SM_Tent_Blue", 700.0, 300.0, 25.0),
                            (NATURE + "SM_Tent_Red", -1900.0, 100.0, -70.0)):
        place(path, x, y, 0.0, yaw, 1.0, "Scenery/Floor")
        taken.append(((x, y), 230.0))
    for path, x, y, yaw in ((NATURE + "SM_Log", 700.0, -200.0, 80.0),
                            (NATURE + "SM_Log", -1500.0, -150.0, 10.0),
                            (NATURE + "SM_Stones02", -1450.0, 250.0, 0.0),
                            (NATURE + "SM_Branch01", 650.0, 850.0, 40.0)):
        place(path, x, y, 0.0, yaw, 1.0, "Scenery/Floor")
        taken.append(((x, y), 140.0))

    scatter(FLOOR_PROPS, 190, sampler, clearance, "Scenery/Floor", taken=taken)


def build_backdrop(half_x, half_y):
    inner_pad, outer_pad = 250.0, 4200.0

    def sampler():
        while True:
            x = rng.uniform(-half_x - outer_pad, half_x + outer_pad)
            y = rng.uniform(-half_y - outer_pad, half_y + outer_pad)
            if abs(x) > half_x + inner_pad or abs(y) > half_y + inner_pad:
                return (x, y)

    def depth(p):
        return max(abs(p[0]) - half_x, abs(p[1]) - half_y, 0.0)

    scatter(BACKDROP_TREES, 260, sampler, lambda p: 1e9, "Scenery/Backdrop",
            scale_fn=lambda p: 1.0 + depth(p) / outer_pad * 1.4)


def build_horizon():
    for i in range(14):
        ang = i / 14.0 * 2 * math.pi + rng.uniform(-0.15, 0.15)
        dist = rng.uniform(9000, 16000)
        place(NATURE + rng.choice(["SM_Hills01", "SM_Hills02"]), math.cos(ang) * dist, math.sin(ang) * dist,
              0.0, rng.uniform(0, 360), rng.uniform(3.0, 6.0), "Scenery/Horizon", shadows=False)
    for i in range(8):
        ang = i / 8.0 * 2 * math.pi + rng.uniform(-0.2, 0.2)
        dist = rng.uniform(32000, 42000)
        place(NATURE + "SM_Mountain01", math.cos(ang) * dist, math.sin(ang) * dist,
              0.0, rng.uniform(0, 360), rng.uniform(0.6, 1.0), "Scenery/Horizon", shadows=False)

    fog = eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
    comp = fog.get_editor_property("component")
    comp.set_editor_property("fog_density", 0.0025)
    comp.set_editor_property("start_distance", 9000.0)
    fog.set_actor_label("HorizonFog")
    fog.set_editor_property("tags", [SCENERY_TAG])
    fog.set_folder_path("Scenery/Horizon")


def build(log):
    for a in eas.get_all_level_actors():
        if a.actor_has_tag(SCENERY_TAG):
            eas.destroy_actor(a)
    counts.clear()
    half_x, half_y = tg.ROOM_SIZE[0] / 2.0, tg.ROOM_SIZE[1] / 2.0
    build_ground()
    build_fences(half_x, half_y)
    build_floor(half_x, half_y, [])
    build_backdrop(half_x, half_y)
    build_horizon()
    log("scenery placed: %s (total %d)" % (dict(sorted(counts.items())), sum(counts.values())))

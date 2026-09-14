"""Pure-python geometry for the Experiment 01 race track.

No Unreal imports: used by build_track.py inside the editor and by
preview_track.py for a top-down layout check outside the editor.

Units are Unreal units. World scale is 1 physical cm = 10 UU, so the
5 m x 6 m room floor is 5000 x 6000 UU and the Fab cars keep real size.
Anything tied to the physical room (e.g. Igloo camera height) uses the
same scale: 1.7 m -> 1700 UU.
"""
import math

WORLD_SCALE = 10.0                      # UU per physical cm
ROOM_SIZE = (500.0 * WORLD_SCALE,       # X: 5 m wall
             600.0 * WORLD_SCALE)       # Y: 6 m wall

TRACK_WIDTH = 700.0                     # ~3.5 car widths
CORNER_RADIUS = 500.0                   # centreline fillet radius
ARC_SEGMENTS = 10                       # low-poly corners
WALL_WIDTH = 40.0
WALL_HEIGHT = 60.0
ROAD_Z = 2.0                            # sits just above the floor plane
LINE_Z = ROAD_Z + 1.0

# Centreline control polygon in race order. A bay cut into the left side
# gives a three-corner complex opposite the long right-hand straight.
CONTROL_POINTS = [
    (-1450.0, -1950.0),
    (1450.0, -1950.0),
    (1450.0, 1950.0),
    (-1450.0, 1950.0),
    (-1450.0, 850.0),
    (-50.0, 850.0),
    (-50.0, -650.0),
    (-1450.0, -650.0),
]

START_LINE_X = 300.0                    # on the bottom straight (y = -1950)
START_LINE_DEPTH = 80.0


def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1])


def _norm(v):
    l = math.hypot(v[0], v[1])
    return (v[0] / l, v[1] / l)


def centreline():
    """Closed centreline polyline with filleted corners (no repeated end point)."""
    pts = []
    n = len(CONTROL_POINTS)
    for i in range(n):
        a, p, b = CONTROL_POINTS[i - 1], CONTROL_POINTS[i], CONTROL_POINTS[(i + 1) % n]
        d_in = _norm(_sub(p, a))
        d_out = _norm(_sub(b, p))
        cross = d_in[0] * d_out[1] - d_in[1] * d_out[0]
        theta = math.acos(max(-1.0, min(1.0, d_in[0] * d_out[0] + d_in[1] * d_out[1])))
        t1 = (p[0] - d_in[0] * CORNER_RADIUS * math.tan(theta / 2.0),
              p[1] - d_in[1] * CORNER_RADIUS * math.tan(theta / 2.0))
        side = 1.0 if cross > 0 else -1.0
        centre = (t1[0] - d_in[1] * CORNER_RADIUS * side, t1[1] + d_in[0] * CORNER_RADIUS * side)
        a0 = math.atan2(t1[1] - centre[1], t1[0] - centre[0])
        for s in range(ARC_SEGMENTS + 1):
            ang = a0 + side * theta * s / ARC_SEGMENTS
            pts.append((centre[0] + CORNER_RADIUS * math.cos(ang), centre[1] + CORNER_RADIUS * math.sin(ang)))
    return pts


def offset_loop(pts, dist):
    """Offset a closed polyline to its left (+dist) or right (-dist) with mitred joins."""
    out = []
    n = len(pts)
    for i in range(n):
        prev_p, p, next_p = pts[i - 1], pts[i], pts[(i + 1) % n]
        d0 = _norm(_sub(p, prev_p))
        d1 = _norm(_sub(next_p, p))
        m = _norm((-d0[1] - d1[1], d0[0] + d1[0]))
        scale = dist / max(0.2, m[0] * -d1[1] + m[1] * d1[0])
        out.append((p[0] + m[0] * scale, p[1] + m[1] * scale))
    return out


class Mesh:
    """Flat-shaded polygons (3 or 4 corners) grouped under one material slot.

    Corners are stored counter-clockwise when viewed from the side the
    normal points to (right-handed maths); build_track.py converts to
    Unreal's winding.
    """

    def __init__(self, slot):
        self.slot = slot
        self.polys = []

    def add(self, corners, normal):
        ax, ay, az = corners[0]
        bx, by, bz = corners[1]
        cx, cy, cz = corners[2]
        ux, uy, uz = bx - ax, by - ay, bz - az
        vx, vy, vz = cx - ax, cy - ay, cz - az
        face = (uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx)
        if face[0] * normal[0] + face[1] * normal[1] + face[2] * normal[2] < 0:
            corners = list(reversed(corners))
        self.polys.append(list(corners))


def _band(mesh, loop_a, loop_b, z):
    """Horizontal ring between two closed loops, facing up."""
    n = len(loop_a)
    for i in range(n):
        j = (i + 1) % n
        a0, a1, b0, b1 = loop_a[i], loop_a[j], loop_b[i], loop_b[j]
        mesh.add([(a0[0], a0[1], z), (a1[0], a1[1], z), (b1[0], b1[1], z), (b0[0], b0[1], z)], (0, 0, 1))


def _wall(mesh, face_loop, back_loop, height):
    """Solid kerb wall: top plus both vertical sides. face_loop is the road-side edge."""
    _band(mesh, face_loop, back_loop, height)
    n = len(face_loop)
    for i in range(n):
        j = (i + 1) % n
        for loop, other in ((face_loop, back_loop), (back_loop, face_loop)):
            p0, p1 = loop[i], loop[j]
            mid = ((p0[0] + p1[0]) / 2.0, (p0[1] + p1[1]) / 2.0)
            omid = ((other[i][0] + other[j][0]) / 2.0, (other[i][1] + other[j][1]) / 2.0)
            out = _norm(_sub(mid, omid))
            mesh.add([(p0[0], p0[1], 0.0), (p1[0], p1[1], 0.0),
                      (p1[0], p1[1], height), (p0[0], p0[1], height)], (out[0], out[1], 0.0))


def build():
    """Return ({mesh name: Mesh}, {loop name: points})."""
    c = centreline()
    half = TRACK_WIDTH / 2.0
    left, right = offset_loop(c, half), offset_loop(c, -half)
    left_back, right_back = offset_loop(c, half + WALL_WIDTH), offset_loop(c, -half - WALL_WIDTH)

    road = Mesh("Road")
    _band(road, left, right, ROAD_Z)

    walls = Mesh("Wall")
    _wall(walls, left, left_back, WALL_HEIGHT)
    _wall(walls, right, right_back, WALL_HEIGHT)

    line = Mesh("Line")
    y0 = CONTROL_POINTS[0][1]
    x0, x1 = START_LINE_X - START_LINE_DEPTH / 2.0, START_LINE_X + START_LINE_DEPTH / 2.0
    line.add([(x0, y0 - half, LINE_Z), (x1, y0 - half, LINE_Z),
              (x1, y0 + half, LINE_Z), (x0, y0 + half, LINE_Z)], (0, 0, 1))

    floor = Mesh("Floor")
    hx, hy = ROOM_SIZE[0] / 2.0, ROOM_SIZE[1] / 2.0
    floor.add([(-hx, -hy, 0.0), (hx, -hy, 0.0), (hx, hy, 0.0), (-hx, hy, 0.0)], (0, 0, 1))

    meshes = {
        "SM_Track_Road": road,
        "SM_Track_Walls": walls,
        "SM_Track_StartLine": line,
        "SM_Track_RoomFloor": floor,
    }
    loops = {"centre": c, "left": left, "right": right, "left_back": left_back, "right_back": right_back}
    return meshes, loops


def report():
    _, loops = build()
    pts = loops["left_back"] + loops["right_back"]
    x1 = max(abs(p[0]) for p in pts)
    y1 = max(abs(p[1]) for p in pts)
    c = loops["centre"]
    length = sum(math.dist(c[i], c[(i + 1) % len(c)]) for i in range(len(c)))
    return ("track footprint %.0f x %.0f UU (%.2f m x %.2f m); walkway %.0f cm (X sides), %.0f cm (Y sides); "
            "lap length %.0f UU" % (2 * x1, 2 * y1, 2 * x1 / WORLD_SCALE / 100, 2 * y1 / WORLD_SCALE / 100,
                                    (ROOM_SIZE[0] / 2 - x1) / WORLD_SCALE, (ROOM_SIZE[1] / 2 - y1) / WORLD_SCALE,
                                    length))


if __name__ == "__main__":
    print(report())

"""Write a top-down SVG of the track layout (no Unreal needed).

Usage: python preview_track.py [out.svg]
"""
import os
import sys

import track_geom as tg


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "track_preview.svg")
    meshes, loops = tg.build()
    hx, hy = tg.ROOM_SIZE[0] / 2.0, tg.ROOM_SIZE[1] / 2.0
    fills = {"Floor": "#3a3a3a", "Road": "#7c7c7c", "Wall": "#c4c4c4", "Line": "#eeeeee"}
    parts = []
    for name in ("SM_Track_RoomFloor", "SM_Track_Road", "SM_Track_Walls", "SM_Track_StartLine"):
        mesh = meshes[name]
        for poly in mesh.polys:
            if any(abs(p[2] - poly[0][2]) > 1e-3 for p in poly):
                continue  # vertical faces are invisible from above
            # SVG y grows downwards; flip so +Y is up.
            pts = " ".join("%.1f,%.1f" % (p[0], -p[1]) for p in poly)
            parts.append('<polygon points="%s" fill="%s" stroke="%s" stroke-width="1"/>'
                         % (pts, fills[mesh.slot], fills[mesh.slot]))
    c = loops["centre"]
    parts.append('<polyline points="%s" fill="none" stroke="#ffcc00" stroke-width="12" stroke-dasharray="60 60"/>'
                 % " ".join("%.1f,%.1f" % (p[0], -p[1]) for p in c + c[:1]))
    parts.append('<text x="%.0f" y="%.0f" fill="#fff" font-size="160" font-family="sans-serif">%s</text>'
                 % (-hx + 80, -hy - 120, tg.report().split(";")[0]))
    svg = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="%.0f %.0f %.0f %.0f" width="1000" height="%d">'
           '<rect x="%.0f" y="%.0f" width="100%%" height="100%%" fill="#111"/>%s</svg>'
           % (-hx - 200, -hy - 400, 2 * hx + 400, 2 * hy + 600, int(1000 * (2 * hy + 600) / (2 * hx + 400)),
              -hx - 200, -hy - 400, "".join(parts)))
    with open(out, "w") as f:
        f.write(svg)
    print("wrote", out)
    print(tg.report())


if __name__ == "__main__":
    main()

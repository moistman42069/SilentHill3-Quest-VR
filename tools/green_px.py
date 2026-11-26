# Session 15 (the beam saber, owner 61): where the beam's bright green is in a picture. For each picture given:
# the number of strongly green pixels, their bounding box, and the line through them (principal axis), so two runs'
# beams can be compared by numbers, not only by eye. Run with the project's venv: python -I tools/green_px.py a.png b.png
import sys

from PIL import Image


def green(path):
    im = Image.open(path).convert("RGB")
    w, h = im.size
    px = im.load()
    pts = []
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            r, g, b = px[x, y]
            if g > 150 and g > r + 25 and g > b + 60:
                pts.append((x, y))
    if not pts:
        return f"{path}: no beam green"
    n = len(pts)
    mx = sum(p[0] for p in pts) / n
    my = sum(p[1] for p in pts) / n
    sxx = sum((p[0] - mx) ** 2 for p in pts) / n
    syy = sum((p[1] - my) ** 2 for p in pts) / n
    sxy = sum((p[0] - mx) * (p[1] - my) for p in pts) / n
    import math

    ang = 0.5 * math.degrees(math.atan2(2 * sxy, sxx - syy))
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    return (f"{path}: {n * 4} green px, box {min(xs)} {min(ys)} - {max(xs)} {max(ys)}, middle {mx:.0f} {my:.0f}, "
            f"axis {ang:.0f} deg from the x axis (y down)")


for p in sys.argv[1:]:
    print(green(p))

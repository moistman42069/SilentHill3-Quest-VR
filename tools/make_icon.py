# SH3 Quest VR 0.1.5: the app's icon from the owner's cover picture (cover.webp, 600x900): her face large, the title's
# white letters taken out of the cover and laid across the bottom over a soft dark fade. Writes a 1024 master, the
# Android launcher sizes and a preview.
import sys
from PIL import Image, ImageFilter, ImageChops, ImageDraw

src_path, out_dir = sys.argv[1], sys.argv[2]
src = Image.open(src_path).convert("RGB")
W, H = src.size

# Her face: a square above the title (the title's letters start about y 420 on the 600x900 cover).
face_box = (196, 12, 600, 416)
S = 1024
face = src.crop(face_box).resize((S, S), Image.LANCZOS)

# The title's band, and its white letters: bright and colourless (her jacket beside the "3" is bright but cream).
band_box = (0, 410, 600, 500)
band = src.crop(band_box)
r, g, b = band.split()
mx = ImageChops.lighter(ImageChops.lighter(r, g), b)
mn = ImageChops.darker(ImageChops.darker(r, g), b)
spread = ImageChops.subtract(mx, mn)
bright = mn.point(lambda v: 255 if v > 165 else 0)
grey = spread.point(lambda v: 255 if v < 48 else 0)
mask = ImageChops.multiply(bright, grey)
# only the title's own rows (her pale collar sits just under it): SILENT HILL rows 425-479, the "3" (taller) 418-492
rows = Image.new("L", band.size, 0)
rd = ImageDraw.Draw(rows)
rd.rectangle([5, 425 - band_box[1], 527, 479 - band_box[1]], fill=255)
rd.rectangle([528, 418 - band_box[1], 594, 492 - band_box[1]], fill=255)
mask = ImageChops.multiply(mask, rows).filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.GaussianBlur(0.8))

# Scaled to most of the icon's width, near the bottom.
tw = int(S * 0.94)
th = int(band.size[1] * tw / band.size[0])
letters = Image.new("RGB", band.size, (244, 242, 238)).resize((tw, th), Image.LANCZOS)
lmask = mask.resize((tw, th), Image.LANCZOS)
tx, ty = (S - tw) // 2, int(S * 0.80) - th // 2

out = face.copy()
# A dark fade over the lower part, so the letters read on any launcher.
fade = Image.new("L", (S, S), 0)
d = ImageDraw.Draw(fade)
top = int(S * 0.55)
for y in range(top, S):
    t = (y - top) / (S - top)
    d.line([(0, y), (S, y)], fill=int(210 * min(1.0, t * 1.4)))
out = Image.composite(Image.new("RGB", (S, S), (12, 4, 4)), out, fade)
# The letters' shadow, then the letters.
shadow = Image.new("L", (S, S), 0)
shadow.paste(lmask, (tx + 4, ty + 6))
shadow = shadow.filter(ImageFilter.GaussianBlur(6)).point(lambda v: min(255, int(v * 1.3)))
out = Image.composite(Image.new("RGB", (S, S), (0, 0, 0)), out, shadow)
full_mask = Image.new("L", (S, S), 0)
full_mask.paste(lmask, (tx, ty))
letters_full = Image.new("RGB", (S, S), (0, 0, 0))
letters_full.paste(letters, (tx, ty))
out = Image.composite(letters_full, out, full_mask)

out.save(f"{out_dir}/icon-1024.png")
out.resize((512, 512), Image.LANCZOS).save(f"{out_dir}/icon-512.png")
for name, px in (("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)):
    out.resize((px, px), Image.LANCZOS).save(f"{out_dir}/ic_launcher_sh3-{name}.png")
# Preview: the 1024, and the small sizes at their real size and rounded as a launcher shows them.
prev = Image.new("RGB", (1024 + 40 + 192 + 20, 1024), (40, 40, 44))
prev.paste(out, (0, 0))
y = 0
for px in (192, 144, 96, 72, 48):
    small = out.resize((px, px), Image.LANCZOS)
    m = Image.new("L", (px, px), 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, px - 1, px - 1], radius=px // 5, fill=255)
    prev.paste(small, (1024 + 40, y), m)
    y += px + 20
prev.save(f"{out_dir}/icon-preview.png")
print("done", tw, th, tx, ty)

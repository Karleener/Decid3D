"""Export du logo Decid retenu (piste 22) : SVG, PNG et ICO."""
import sys
from pathlib import Path
import numpy as np
from PIL import Image

OUT = Path(sys.argv[1])
NAVY, OXIDE, IVORY = "#151946", "#7C170D", "#EBE1D5"

# Géométrie à plat (viewBox 100), sans clipPath
NAVY_D = "M16 12H48A38 38 0 0 1 78.725 27.639L16 75.05Z"
HOLE = "M25 36a11 11 0 1 0 22 0a11 11 0 1 0 -22 0Z"
RED_D = "M16 83.85L82.332 33.713A38 38 0 0 1 48 88H16Z"
DOT = '<circle cx="36" cy="36" r="6.5" fill="{c}"/>'


def mark(a, b, dot=True):
    """Éléments SVG du symbole. a = partie haute, b = partie basse et point."""
    s = f'<path fill="{a}" fill-rule="evenodd" d="{NAVY_D}{HOLE}"/>\n  <path fill="{b}" d="{RED_D}"/>'
    if dot:
        s += "\n  " + DOT.format(c=b)
    return s


def svg(body, vb="0 0 100 100", extra=""):
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{vb}">{extra}\n  {body}\n</svg>\n'


files = {
    "decid-symbole-couleur.svg": svg(mark(NAVY, OXIDE)),
    "decid-symbole-bleu.svg": svg(mark(NAVY, NAVY, dot=False)),
    "decid-symbole-ivoire.svg": svg(mark(IVORY, IVORY, dot=False)),
    "decid-icone-app.svg": svg(
        f'<rect width="100" height="100" rx="23" fill="{IVORY}"/>\n  <g transform="translate(15.2 15.2) scale(.696)">{mark(NAVY, OXIDE)}</g>'),
}

FONT = ('\n  <style>@import url("https://fonts.googleapis.com/css2?family=Manrope:wght@600&amp;display=swap");'
        '.w{font-family:Manrope,"Segoe UI",sans-serif;font-weight:600;font-size:38px;letter-spacing:-1.33px}</style>')
for name, a, b, dot, txt in [
    ("decid-signature-couleur.svg", NAVY, OXIDE, True, NAVY),
    ("decid-signature-bleu.svg", NAVY, NAVY, False, NAVY),
    ("decid-signature-ivoire.svg", IVORY, IVORY, False, IVORY),
]:
    body = (f'<g transform="translate(-7 0) scale(.46)">{mark(a, b, dot)}</g>\n'
            f'  <text class="w" x="46" y="36.5" fill="{txt}">Decid</text>')
    files[name] = svg(body, vb="0 0 150 46", extra=FONT)

for n, c in files.items():
    (OUT / n).write_text(c, encoding="utf-8")


# Rendu matriciel par masques, suréchantillonné
def render(px, tile):
    S = px * 8
    u = (np.arange(S) + 0.5) / S * 100
    x, y = np.meshgrid(u, u)
    if tile:  # le symbole occupe 69,6 % de la tuile
        tx, ty = (x - 15.2) / 0.696, (y - 15.2) / 0.696
    else:
        tx, ty = x, y
    in_d = ((tx >= 16) & (tx <= 48) & (ty >= 12) & (ty <= 88)) | (
        (tx >= 48) & ((tx - 48) ** 2 + (ty - 50) ** 2 <= 38 ** 2))
    k = 0.75583
    navy = in_d & (ty <= 94.7 - k * (tx + 10)) & ((tx - 36) ** 2 + (ty - 36) ** 2 > 11 ** 2)
    red = in_d & (ty >= 103.5 - k * (tx + 10))
    red |= (tx - 36) ** 2 + (ty - 36) ** 2 <= 6.5 ** 2
    img = np.zeros((S, S, 4), np.uint8)
    if tile:
        r = 23
        cx, cy = np.clip(x, r, 100 - r), np.clip(y, r, 100 - r)
        inside = (x - cx) ** 2 + (y - cy) ** 2 <= r * r
        img[inside] = (0xEB, 0xE1, 0xD5, 255)
    img[navy] = (0x15, 0x19, 0x46, 255)
    img[red] = (0x7C, 0x17, 0x0D, 255)
    return Image.fromarray(img).resize((px, px), Image.LANCZOS)


for px in (512, 256):
    render(px, False).save(OUT / f"decid-symbole-couleur-{px}.png")
icons = [render(p, True) for p in (256, 128, 64, 48, 32, 24, 16)]
icons[0].save(OUT / "decid-icone-app-256.png")
icons[0].save(OUT / "decid.ico", sizes=[(i.width, i.height) for i in icons], append_images=icons[1:])
print("ok", sorted(p.name for p in OUT.iterdir()))

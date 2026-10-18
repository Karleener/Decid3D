"""Déclinaisons du logo Decid pour l'application (les fichiers d'origine du dossier parent ne sont pas modifiés).

Même géométrie que export_decid.py (piste 22). Ajouts :
- decid-symbole-sombre.svg : pour les fonds sombres de l'application (thème sombre). Le cramoisi d'origine
  #7C170D n'a qu'un contraste de 1,58:1 sur #151946 ; il est remplacé par « Oxide clair » #D4553E
  (4,10:1 sur #151946, 4,65:1 sur #0B0E2A, 3,69:1 sur #1C2156 ; seuil WCAG 1.4.11 des éléments graphiques : 3:1).
- decid-symbole-courant.svg : monochrome en currentColor, pour les petites tailles intégrées à l'interface.
- decid-favicon.svg : l'icône d'application (tuile ivoire), utilisable telle quelle comme favicon.
"""
from pathlib import Path

OUT = Path(__file__).resolve().parent
NAVY, OXIDE, IVORY, OXIDE_CLAIR = "#151946", "#7C170D", "#EBE1D5", "#D4553E"
NAVY_D = "M16 12H48A38 38 0 0 1 78.725 27.639L16 75.05Z"
HOLE = "M25 36a11 11 0 1 0 22 0a11 11 0 1 0 -22 0Z"
RED_D = "M16 83.85L82.332 33.713A38 38 0 0 1 48 88H16Z"


def mark(a, b, dot=True):
    s = f'<path fill="{a}" fill-rule="evenodd" d="{NAVY_D}{HOLE}"/>\n  <path fill="{b}" d="{RED_D}"/>'
    if dot:
        s += f'\n  <circle cx="36" cy="36" r="6.5" fill="{b}"/>'
    return s


def svg(body):
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">\n  {body}\n</svg>\n'


fichiers = {
    "decid-symbole-sombre.svg": svg(mark(IVORY, OXIDE_CLAIR)),
    "decid-symbole-courant.svg": svg(mark("currentColor", "currentColor", dot=False)),
    "decid-favicon.svg": svg(f'<rect width="100" height="100" rx="23" fill="{IVORY}"/>\n  '
                             f'<g transform="translate(15.2 15.2) scale(.696)">{mark(NAVY, OXIDE)}</g>'),
}
for nom, contenu in fichiers.items():
    (OUT / nom).write_text(contenu, encoding="utf-8")
print("ok", sorted(fichiers))

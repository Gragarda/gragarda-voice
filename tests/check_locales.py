"""Verifie les traductions par rapport a en-US.ini : memes cles, valeurs bien quotees,
memes %1 et memes balises HTML. Usage : python tests/check_locales.py"""
import pathlib, re, sys

root = pathlib.Path(__file__).resolve().parent.parent / "data" / "locale"


def load(p):
    out = {}
    for n, line in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        m = re.fullmatch(r'([A-Za-z]+)="([^"]*)"', line)
        if not m:
            raise ValueError(f"{p.name}:{n}: ligne mal formee (guillemet droit dans la valeur ?)")
        out[m[1]] = m[2]
    return out


ref = load(root / "en-US.ini")
errors = []
for p in sorted(root.glob("*.ini")):
    try:
        t = load(p)
    except ValueError as e:
        errors.append(str(e))
        continue
    for k in sorted(set(ref) - set(t)):
        errors.append(f"{p.name}: cle manquante {k}")
    for k in sorted(set(t) - set(ref)):
        errors.append(f"{p.name}: cle inconnue {k}")
    for k in set(ref) & set(t):
        if ref[k].count("%1") != t[k].count("%1"):
            errors.append(f"{p.name}: {k} : nombre de %1 different")
        if re.findall(r"</?(\w+)", ref[k]) != re.findall(r"</?(\w+)", t[k]):
            errors.append(f"{p.name}: {k} : balises HTML differentes")

print("\n".join(errors) or f"{len(list(root.glob('*.ini')))} fichiers OK")
sys.exit(1 if errors else 0)

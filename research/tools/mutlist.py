"""mutlist.py: the planted-error lists of the mutate_*.py runners (tone_muts.txt, enc_muts.txt, files_muts.txt,
fx_muts.txt, fxc_muts.txt). A line is one plant, name|old|new[|field ...], with \\n and \\| escaped; a blank line or
one starting with # is none."""
import re
import sys
from pathlib import Path


def _text(field):
    return field.replace('\\n', '\n').replace('\\|', '|')


def plants(path, names, runner, defaults=()):
    """The list's plants as (name, old, new, *fields): each field after new is its default where the line has none.
    With names, only those, in the list's order; a name the list doesn't have stops the runner."""
    found = []
    for line in Path(path).read_text().split('\n'):
        if not line or line.startswith('#'):
            continue
        p = re.split(r'(?<!\\)\|', line)
        more = [p[3 + i] if len(p) > 3 + i else d for i, d in enumerate(defaults)]
        found.append((p[0], _text(p[1]), _text(p[2]) if len(p) > 2 else '', *more))
    if not names:
        return found
    unknown = set(names) - {m[0] for m in found}
    if unknown:
        sys.exit(f'{runner}: not in {Path(path).name}: ' + ' '.join(sorted(unknown)))
    return [m for m in found if m[0] in names]

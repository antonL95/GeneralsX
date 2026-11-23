import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path("tools").resolve()))
from ui_apt_inventory import Library, texture_path
from ui_fs import MergedFileSystem


def playable_sides(templates):
    sides = set()
    for template in re.split(r"(?im)^PlayerTemplate\s+", templates)[1:]:
        if not re.search(r"(?im)^\s*PlayableSide\s*=\s*Yes\b", template):
            continue
        side = re.search(r"(?im)^\s*Side\s*=\s*(\w+)", template)
        if not side:
            raise RuntimeError("Playable template is missing its Side")
        sides.add(side.group(1))
    if not sides:
        raise RuntimeError("No playable sides found")
    return sorted(sides)


def check_exports(sides, record, lookup):
    movie = record["movie"]
    exports = {entry["name"]: entry["character"] for entry in movie.exports}
    aliases = {"Men": "Gondor", "Elves": "Lothlorien"}
    for side in sides:
        name = "PalantirFrame_" + aliases.get(side, side)
        if name not in exports:
            raise RuntimeError(f"No {name} export for {side}")
        sprite = movie.characters[exports[name]]
        placements = [item for frame in sprite["frames"] for item in frame if item["type"] == "place"]
        images = set()
        for item in placements:
            shape = movie.characters[item["character"]]
            geometry = record["geometries"][shape["geometry"]]
            images.update(style.image for style in geometry.styles if style.kind == "textured")
        if not images:
            raise RuntimeError(f"No frame artwork for {side}")
        for image in sorted(images):
            page = record["image_map"][image].texture
            texture, exists = texture_path(lookup, record["name"], page)
            if not exists:
                raise RuntimeError(f"{name} is missing {texture}")
            print(f"{side}\t{name}\t{texture}")


def main(app):
    fs = MergedFileSystem(app)
    templates = fs.get("data/ini/playertemplate.ini").read().decode("latin-1")
    record = Library(fs.get).load("palantirexport.apt")
    check_exports(playable_sides(templates), record, fs.get)


if __name__ == "__main__":
    main(Path(sys.argv[1]))

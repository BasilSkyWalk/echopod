import argparse
import json
import shutil
import sys
from pathlib import Path

import demo_library
import theme_pack

BOX_FIELDS = ("fill_top", "fill_bottom", "border", "shine", "ink", "border_width", "border_edges",
              "shine_height", "inset_across", "inset_down", "radius")
COLOR_FIELDS = 5
PAGE_FILES = ("index.html", "style.css", "editor.js", "theme_format.js")
MODULE = "echo_preview.wasm"
FACE_BYTES_LIMIT = 200_000


def box_fields(table, colors, where):
    packed = theme_pack.box_bytes(table, colors, where)
    return {field: colors[value] if index < COLOR_FIELDS else value
            for index, (field, value) in enumerate(zip(BOX_FIELDS, packed))}


def schema(themes, definitions):
    colors = theme_pack.names_in(definitions / "theme_colors.def")
    boxes = theme_pack.names_in(definitions / "theme_boxes.def")
    spacing = theme_pack.spacing_in(definitions / "theme_spacing.def")
    dark = theme_pack.read_source(themes / (theme_pack.BASE_THEME + theme_pack.SOURCE_SUFFIX))
    return {
        "colors": colors,
        "boxes": boxes,
        "spacing": [{"name": name, "least": least, "most": most} for name, least, most in spacing],
        "dark": {
            "selection": dark.get("selection", boxes[0]),
            "colors": {name: dark["colors"][name].lower() for name in colors},
            "boxes": {name: box_fields(dark["boxes"][name], colors, f"dark: box {name}")
                      for name in boxes},
        },
        "samples": sorted(path.stem for path in themes.glob("*" + theme_pack.SOURCE_SUFFIX)),
    }


def glyph_faces(config, packs):
    listed = json.loads(config.read_text(encoding="utf-8"))["faces"]
    files = [packs / (entry["name"] + ".bin") for entry in listed]
    return [{"index": index, "file": path.name} for index, path in enumerate(files)
            if path.is_file() and path.stat().st_size <= FACE_BYTES_LIMIT]


def build(arguments):
    output = arguments.output
    (output / "themes").mkdir(parents=True, exist_ok=True)
    for name in PAGE_FILES:
        shutil.copyfile(arguments.pages / name, output / name)
    shutil.copyfile(arguments.module, output / MODULE)
    demo_library.build(output)
    described = schema(arguments.themes, arguments.definitions)
    described["faces"] = glyph_faces(arguments.glyphs, arguments.packs)
    for face in described["faces"]:
        shutil.copyfile(arguments.packs / face["file"], output / face["file"])
    (output / "theme_schema.json").write_text(json.dumps(described, indent=1) + "\n",
                                              encoding="utf-8", newline="\n")
    for stem, package in theme_pack.build_all(arguments.themes, arguments.definitions).items():
        (output / "themes" / (stem + theme_pack.PACKAGE_SUFFIX)).write_bytes(package)
    return described


def parse_arguments(argv):
    parser = argparse.ArgumentParser(description="Assemble the theme editor site")
    parser.add_argument("--pages", type=Path, required=True, help="folder with the editor's pages")
    parser.add_argument("--module", type=Path, required=True, help="echo_preview.wasm")
    parser.add_argument("--themes", type=Path, required=True, help="folder of .toml theme sources")
    parser.add_argument("--definitions", type=Path, required=True,
                        help="folder with the theme .def lists")
    parser.add_argument("--glyphs", type=Path, required=True, help="glyph_pack.json")
    parser.add_argument("--packs", type=Path, required=True, help="folder of packed glyph faces")
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args(argv)


def main(argv=None):
    arguments = parse_arguments(argv)
    try:
        described = build(arguments)
    except (theme_pack.ThemeError, OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"wrote {arguments.output}: {len(described['colors'])} colours, "
          f"{len(described['boxes'])} boxes, {len(described['samples'])} sample themes")
    return 0


if __name__ == "__main__":
    sys.exit(main())

import argparse
import json
import re
import shutil
import sys
from pathlib import Path

import demo_library
import scene_compile
import theme_pack

BOX_FIELDS = ("fill_top", "fill_bottom", "border", "shine", "ink", "border_width", "border_edges",
              "shine_height", "inset_across", "inset_down", "radius", "see_through")
COLOR_FIELDS = 5
PAGE_FILES = ("index.html", "style.css", "editor.js", "theme_format.js", "preview_host.js",
              "icon_editor.js", "wallpaper_editor.js", "font_editor.js",
              "accent_editor.js", "now_playing_designer.js", "scene_compile.js",
              "gallery.html",
              "gallery.js")
MODULE = "echo_preview.wasm"
GALLERY = "index.json"
FACE_BYTES_LIMIT = 200_000
FILE_NAME = re.compile(r"^[a-z0-9][a-z0-9_-]{0,22}$")


def box_fields(table, colors, where):
    packed = theme_pack.box_bytes(table, colors, where)
    return {field: colors[value] if index < COLOR_FIELDS else value
            for index, (field, value) in enumerate(zip(BOX_FIELDS, packed))}


def sources_in(folder):
    return sorted(folder.glob("*" + theme_pack.SOURCE_SUFFIX))


def schema(themes, definitions):
    colors = theme_pack.names_in(definitions / "theme_colors.def")
    boxes = theme_pack.names_in(definitions / "theme_boxes.def")
    spacing = theme_pack.spacing_in(definitions / "theme_spacing.def")
    icons = theme_pack.icons_in(definitions / "theme_icons.def")
    fonts = theme_pack.fonts_in(definitions / "theme_fonts.def")
    layout = theme_pack.layout_in(definitions / "theme_layout.def")
    ranges = json.loads(theme_pack.FONT_RANGES.read_text(encoding="utf-8"))["character_ranges"]
    dark = theme_pack.read_source(themes / (theme_pack.BASE_THEME + theme_pack.SOURCE_SUFFIX))
    return {
        "colors": colors,
        "boxes": boxes,
        "spacing": [{"name": name, "least": least, "most": most} for name, least, most in spacing],
        "icons": [{"name": name, "width": width, "height": height}
                  for name, width, height in icons],
        "icon_pairs": [list(pair) for pair in theme_pack.ICON_PAIRS],
        "wallpapers": [{"name": name, "width": width, "height": height}
                       for name, width, height in theme_pack.WALLPAPERS],
        "fonts": [{"name": name, "height": height} for name, height in fonts],
        "layout": [{"name": name, "height": height} for name, height in layout],
        "layouts": list(theme_pack.LAYOUTS),
        "font_ranges": ranges,
        "asset_memory": theme_pack.ASSET_MEMORY,
        "dark": {
            "selection": dark.get("selection", boxes[0]),
            "colors": {name: dark["colors"][name].lower() for name in colors},
            "boxes": {name: box_fields(dark["boxes"][name], colors, f"dark: box {name}")
                      for name in boxes},
            "accents": [{key: value.lower() if key != "name" else value
                         for key, value in accent.items()} for accent in dark.get("accents", [])],
        },
        "samples": [path.stem for path in sources_in(themes)],
        "checks": list(theme_pack.CHECKS),
        "scene": scene_compile.definitions(definitions.parents[2] / "np_faces" / "include" /
                                           "np_faces"),
    }


def glyph_faces(config, packs):
    listed = json.loads(config.read_text(encoding="utf-8"))["faces"]
    files = [packs / (entry["name"] + ".bin") for entry in listed]
    return [{"index": index, "file": path.name} for index, path in enumerate(files)
            if path.is_file() and path.stat().st_size <= FACE_BYTES_LIMIT]


def shared_sources(folder, taken):
    paths = sources_in(folder) if folder else []
    for path in paths:
        if not FILE_NAME.match(path.stem):
            raise theme_pack.ThemeError(
                f"{path.name}: name the file with 1 to 23 of a-z, 0-9, - and _")
        if path.stem in taken:
            raise theme_pack.ThemeError(f"{path.name}: a built-in theme already has this file name")
    return paths


def gallery_entry(path, shared):
    theme = theme_pack.read_source(path)
    return {"file": path.stem, "name": theme.get("name", path.stem),
            "author": theme.get("author", ""), "description": theme.get("description", ""),
            "shared": shared}


def images_of(path):
    theme = theme_pack.read_source(path)
    fonts = theme.get("fonts") if isinstance(theme.get("fonts"), dict) else {}
    tables = [(theme.get(key), "image") for key, _, _ in theme_pack.WALLPAPERS]
    tables += [(table, "file") for table in fonts.values()]
    tables.append((theme.get("now_playing_scene"), "file"))
    return [path.parent / table[key] for table, key in tables
            if isinstance(table, dict) and isinstance(table.get(key), str)]


def write_json(path, value):
    path.write_text(json.dumps(value, indent=1) + "\n", encoding="utf-8", newline="\n")


def write_themes(arguments, folder):
    own = sources_in(arguments.themes)
    shared = shared_sources(arguments.community, {path.stem for path in own})
    packages = theme_pack.build_all(arguments.themes, arguments.definitions)
    base = arguments.themes / (theme_pack.BASE_THEME + theme_pack.SOURCE_SUFFIX)
    if shared:
        packages |= theme_pack.build_all(arguments.community, arguments.definitions, base)
    for stem, package in packages.items():
        (folder / (stem + theme_pack.PACKAGE_SUFFIX)).write_bytes(package)
    for path in (*own, *shared):
        shutil.copyfile(path, folder / path.name)
        for image in images_of(path):
            shutil.copyfile(image, folder / image.name)
    write_json(folder / GALLERY, [gallery_entry(path, path in shared) for path in (*own, *shared)])
    return [path.stem for path in shared]


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
    described["samples"] += write_themes(arguments, output / "themes")
    write_json(output / "theme_schema.json", described)
    return described


def parse_arguments(argv):
    parser = argparse.ArgumentParser(description="Assemble the theme editor site")
    parser.add_argument("--pages", type=Path, required=True, help="folder with the editor's pages")
    parser.add_argument("--module", type=Path, required=True, help="echo_preview.wasm")
    parser.add_argument("--themes", type=Path, required=True, help="folder of .toml theme sources")
    parser.add_argument("--community", type=Path,
                        help="folder of shared .toml theme sources, packed over the dark theme")
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
          f"{len(described['boxes'])} boxes, {len(described['samples'])} themes")
    return 0


if __name__ == "__main__":
    sys.exit(main())

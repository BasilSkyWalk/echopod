import {
  AREAS,
  CONDITIONS,
  KINDS,
  MARKS,
  NUMBERS,
  TEXT_SOURCES,
  VALUES,
  colorOf,
  element,
  findElement,
  newElement,
  walk,
} from "./drawing.js";

const HANDLE = 7;
const HIT = 4;
const DEGREES = 180 / Math.PI;
const BINDABLE = {
  box: [["w", "Width"], ["h", "Height"], ["x", "Left"], ["y", "Top"]],
  circle: [["w", "Size"], ["x", "Left"], ["y", "Top"]],
  line: [["angle", "Angle"], ["w", "Length"], ["x", "Left"], ["y", "Top"]],
  text: [["x", "Left"], ["y", "Top"], ["w", "Width"]],
  cover: [],
  symbol: [["x", "Left"], ["y", "Top"]],
  number: [],
  menu: [],
  group: [["x", "Left"], ["y", "Top"]],
};
const COLOURED = {
  box: [["color", "Colour"]],
  circle: [["color", "Colour"]],
  line: [["color", "Colour"]],
  text: [["color", "Colour"]],
  symbol: [["color", "Colour"]],
  number: [["lit", "Lit"]],
};

export function drawingEditor(ctx) {
  const { el, field, group, chips, range, toggle, choiceColour, icon } = ctx;
  let drag = null;
  const expanded = new Set();
  let listScroll = 0;
  let shownPick = null;
  let settingsShown = null;

  const drawing = () => ctx.drawing();
  const shape = () => ctx.shape();
  const picked = () => (drawing() ? findElement(drawing(), ctx.state.picked)?.item ?? null : null);

  function nameOf(item) {
    if (item.name) {
      return item.name;
    }
    if (item.kind === "text") {
      return item.source === "custom" ? `“${item.custom ?? ""}”` : TEXT_SOURCES[item.source] ?? "Text";
    }
    return KINDS[item.kind] ?? item.kind;
  }

  function placements() {
    const found = [];
    const area = AREAS[shape()];
    const visit = (items, ox, oy, chain) => {
      for (const item of items) {
        const at = item.at?.[shape()];
        if (!at || item.hidden) {
          continue;
        }
        const x = ox + Number(at.x);
        const y = oy + Number(at.y);
        found.push({ item, chain, x, y, w: Number(at.w), h: Number(at.h) });
        if (item.kind === "group") {
          visit(item.children ?? [], x, y, [...chain, item]);
        }
      }
    };
    visit(drawing()?.elements ?? [], 0, area.top, []);
    return found;
  }

  function reveal(id) {
    const found = placements().find((place) => place.item.id === id);
    for (const parent of found?.chain ?? []) {
      expanded.add(parent.id);
    }
  }

  function shownLevel(places) {
    const current = places.find((place) => place.item.id === ctx.state.picked);
    if (!current) {
      return (place) => place.chain.length === 0;
    }
    const parent = current.chain.at(-1) ?? null;
    return (place) => (place.chain.at(-1) ?? null) === parent || place.chain.at(-1) === current.item;
  }

  function lineEnds(place) {
    const radians = (Number(place.item.angle) || 0) / DEGREES;
    const start = Number(place.item.start) || 0;
    const reach = start + Math.max(1, place.w);
    return {
      from: { x: place.x + Math.sin(radians) * start, y: place.y - Math.cos(radians) * start },
      to: { x: place.x + Math.sin(radians) * reach, y: place.y - Math.cos(radians) * reach },
    };
  }

  function nearSegment(point, from, to) {
    const [dx, dy] = [to.x - from.x, to.y - from.y];
    const length = dx * dx + dy * dy || 1;
    const t = Math.max(0, Math.min(1, ((point.x - from.x) * dx + (point.y - from.y) * dy) / length));
    return Math.hypot(point.x - (from.x + t * dx), point.y - (from.y + t * dy)) <= HIT;
  }

  function hit(place, point) {
    if (place.item.kind === "line") {
      const { from, to } = lineEnds(place);
      return nearSegment(point, from, to) || Math.hypot(point.x - place.x, point.y - place.y) <= HIT;
    }
    return point.x >= place.x && point.x < place.x + place.w && point.y >= place.y && point.y < place.y + place.h;
  }

  function handleOf(place) {
    if (place.item.kind === "line") {
      return lineEnds(place).to;
    }
    return { x: place.x + place.w, y: place.y + place.h };
  }

  function overlay(pen, zoom) {
    const places = placements();
    const shown = shownLevel(places);
    for (const place of places) {
      if (!shown(place)) {
        continue;
      }
      const chosen = place.item.id === ctx.state.picked;
      pen.setLineDash(chosen ? [] : [4, 3]);
      pen.lineWidth = chosen ? 1.5 : 1;
      pen.strokeStyle = chosen ? "#ffffff" : "rgba(255, 255, 255, 0.3)";
      if (place.item.kind === "line") {
        const { from, to } = lineEnds(place);
        pen.beginPath();
        pen.moveTo(from.x * zoom, from.y * zoom);
        pen.lineTo(to.x * zoom, to.y * zoom);
        pen.stroke();
        pen.strokeRect(place.x * zoom - 2, place.y * zoom - 2, 4, 4);
      } else {
        pen.strokeRect(place.x * zoom + 0.5, place.y * zoom + 0.5, Math.max(1, place.w * zoom - 1), Math.max(1, place.h * zoom - 1));
      }
      if (chosen) {
        const handle = handleOf(place);
        pen.setLineDash([]);
        pen.fillStyle = "#ffffff";
        pen.fillRect(handle.x * zoom - HANDLE / 2, handle.y * zoom - HANDLE / 2, HANDLE, HANDLE);
        pen.strokeStyle = "#1c1b19";
        pen.strokeRect(handle.x * zoom - HANDLE / 2 + 0.5, handle.y * zoom - HANDLE / 2 + 0.5, HANDLE - 1, HANDLE - 1);
      }
    }
  }

  function nearHandle(place, point) {
    const handle = handleOf(place);
    return Math.abs(point.x - handle.x) <= HANDLE / ctx.state.zoom + 1 && Math.abs(point.y - handle.y) <= HANDLE / ctx.state.zoom + 1;
  }

  function down(point) {
    const places = placements();
    const current = places.find((place) => place.item.id === ctx.state.picked);
    if (current && nearHandle(current, point)) {
      drag = { place: current, resize: true, from: point, start: { ...current.item.at[shape()] } };
      return true;
    }
    const path = pathAt(places, point);
    if (!path) {
      ctx.state.picked = null;
      ctx.render();
      return false;
    }
    const index = path.findIndex((item) => item.id === ctx.state.picked);
    const parent = current?.chain.at(-1);
    const level = index >= 0 ? index : Math.max(0, path.indexOf(parent) + 1);
    const target = places.find((place) => place.item === path[level]);
    ctx.state.picked = target.item.id;
    reveal(target.item.id);
    drag = { place: target, resize: false, from: point, start: { ...target.item.at[shape()] } };
    ctx.render();
    return true;
  }

  function pathAt(places, point) {
    const hits = places.filter((place) => hit(place, point));
    const leaf = hits.findLast((place) => place.item.kind !== "group") ?? hits.at(-1);
    return leaf ? [...leaf.chain, leaf.item] : null;
  }

  function deeper(point) {
    const places = placements();
    const path = pathAt(places, point);
    const index = path ? path.findIndex((item) => item.id === ctx.state.picked) : -1;
    if (index < 0 || index === path.length - 1) {
      return;
    }
    ctx.state.picked = path[index + 1].id;
    reveal(ctx.state.picked);
    ctx.render();
  }

  function move(point) {
    if (!drag) {
      const places = placements();
      const current = places.find((place) => place.item.id === ctx.state.picked);
      if (current && nearHandle(current, point)) {
        return current.item.kind === "line" ? "crosshair" : "nwse-resize";
      }
      return places.some((place) => hit(place, point)) ? "move" : "default";
    }
    const { place, resize, from, start } = drag;
    const at = place.item.at[shape()];
    const dx = Math.round(point.x - from.x);
    const dy = Math.round(point.y - from.y);
    if (resize && place.item.kind === "line") {
      const ax = point.x - place.x;
      const ay = point.y - place.y;
      const angle = Math.round((Math.atan2(ax, -ay) * DEGREES + 360) % 360);
      place.item.angle = angle;
      at.w = Math.max(1, Math.round(Math.hypot(ax, ay) - (Number(place.item.start) || 0)));
    } else if (resize) {
      at.w = Math.max(1, start.w + dx);
      at.h = Math.max(place.item.kind === "line" ? 0 : 1, start.h + dy);
    } else {
      at.x = start.x + dx;
      at.y = start.y + dy;
    }
    ctx.apply();
    return null;
  }

  function up() {
    if (drag) {
      drag = null;
      ctx.changed({ rebuild: true });
    }
  }

  function nudge(dx, dy) {
    const item = picked();
    const at = item?.at?.[shape()];
    if (!at) {
      return false;
    }
    at.x += dx;
    at.y += dy;
    ctx.changed({ rebuild: true });
    return true;
  }

  function siblingsOf(item) {
    const found = findElement(drawing(), item.id);
    return found?.parent ? found.parent.children : drawing().elements;
  }

  function moveWithin(item, step) {
    const list = siblingsOf(item);
    const index = list.indexOf(item);
    const target = index + step;
    if (target < 0 || target >= list.length) {
      return;
    }
    list.splice(index, 1);
    list.splice(target, 0, item);
    ctx.changed({ rebuild: true });
  }

  function intoGroup(item) {
    const list = siblingsOf(item);
    const index = list.indexOf(item);
    const host = list.slice(0, index).reverse().find((other) => other.kind === "group");
    if (!host) {
      ctx.say("Put a group above it first, then move it in.", true);
      return;
    }
    list.splice(index, 1);
    for (const shapeName of Object.keys(AREAS)) {
      const own = item.at[shapeName];
      const outer = host.at[shapeName];
      if (own && outer) {
        own.x -= outer.x;
        own.y -= outer.y;
      }
    }
    host.children.push(item);
    ctx.changed({ rebuild: true });
  }

  function outOfGroup(item) {
    const found = findElement(drawing(), item.id);
    if (!found?.parent) {
      return;
    }
    const host = found.parent;
    host.children.splice(host.children.indexOf(item), 1);
    for (const shapeName of Object.keys(AREAS)) {
      const own = item.at[shapeName];
      const outer = host.at[shapeName];
      if (own && outer) {
        own.x += outer.x;
        own.y += outer.y;
      }
    }
    const list = siblingsOf(host);
    list.splice(list.indexOf(host) + 1, 0, item);
    ctx.changed({ rebuild: true });
  }

  function remove(item) {
    const list = siblingsOf(item);
    list.splice(list.indexOf(item), 1);
    ctx.state.picked = null;
    ctx.changed({ rebuild: true });
  }

  function duplicate(item) {
    const copy = JSON.parse(JSON.stringify(item));
    const renumber = (node) => {
      const fresh = element(node.kind, null, null);
      node.id = fresh.id;
      for (const child of node.children ?? []) {
        renumber(child);
      }
    };
    renumber(copy);
    const at = copy.at[shape()];
    if (at) {
      at.x += 6;
      at.y += 6;
    }
    const list = siblingsOf(item);
    list.splice(list.indexOf(item) + 1, 0, copy);
    ctx.state.picked = copy.id;
    ctx.changed({ rebuild: true });
  }

  function tool(title, path, act, disabled = false) {
    return el("button", { type: "button", title, "aria-label": title, disabled, onclick: (event) => {
      event.stopPropagation();
      act();
    } }, icon(path));
  }

  function tree(items, depth) {
    return items.flatMap((item) => {
      const row = el("li", {
        role: "option",
        "aria-selected": String(item.id === ctx.state.picked),
        class: [item.hidden ? "hidden-layer" : "", item.at?.[shape()] ? "" : "elsewhere"].join(" ").trim(),
        style: `padding-left: ${10 + depth * 16}px`,
        onclick: () => {
          ctx.state.picked = item.id;
          ctx.render();
        },
      },
      el("span", {}, nameOf(item)),
      el("span", { class: "tools" },
        tool("Move back", "m6 8 4 4 4-4", () => moveWithin(item, -1)),
        tool("Move forward", "m6 12 4-4 4 4", () => moveWithin(item, 1)),
        tool(item.hidden ? "Show" : "Hide", item.hidden
          ? "M3 10s2.5-5 7-5 7 5 7 5-2.5 5-7 5-7-5-7-5zM4 4l12 12"
          : "M3 10s2.5-5 7-5 7 5 7 5-2.5 5-7 5-7-5-7-5zM10 12a2 2 0 1 0 0-4 2 2 0 0 0 0 4", () => {
          item.hidden = !item.hidden;
          ctx.changed({ rebuild: true });
        }),
        tool("Duplicate", "M7 7h9v9H7zM4 13V4h9", () => duplicate(item)),
        tool("Remove", "M5 6h10M8 6V4.5h4V6m-6 0 .7 9.5h6.6L14 6", () => remove(item))));
      if (item.kind !== "group") {
        return [row];
      }
      const open = expanded.has(item.id);
      row.firstChild.prepend(el("button", { type: "button", class: "fold", title: open ? "Fold away" : "Show what's inside", "aria-expanded": String(open), onclick: (event) => {
        event.stopPropagation();
        if (open) {
          expanded.delete(item.id);
        } else {
          expanded.add(item.id);
        }
        ctx.render();
      } }, icon(open ? "m6 8 4 4 4-4" : "m8 6 4 4-4 4")));
      return open ? [row, ...tree(item.children ?? [], depth + 1)] : [row];
    });
  }

  function addMenu(screen) {
    const kinds = Object.entries(KINDS).filter(([kind]) => kind !== "menu" || screen === "home");
    return el("div", { class: "add-menu" }, kinds.map(([kind, label]) => el("button", { type: "button", onclick: () => {
      const made = newElement(kind, screen);
      const host = picked()?.kind === "group" ? picked() : null;
      if (host) {
        for (const shapeName of Object.keys(AREAS)) {
          made.at[shapeName] = host.at[shapeName] ? { ...made.at[shapeName], x: 4, y: 4 } : null;
        }
        host.children.push(made);
        expanded.add(host.id);
      } else {
        drawing().elements.push(made);
      }
      if (!ctx.fits()) {
        (host ? host.children : drawing().elements).pop();
        ctx.fits();
        ctx.say("There's no room for more on this screen. Remove something first.", true);
        return;
      }
      ctx.state.picked = made.id;
      ctx.changed({ rebuild: true });
    } }, `+ ${label}`)));
  }

  function numberInput(value, set, { least = -400, most = 400, step = 1, width = 64 } = {}) {
    const input = el("input", { type: "number", value, min: least, max: most, step, style: `width:${width}px` });
    input.addEventListener("change", () => {
      const number = Math.max(least, Math.min(most, Number(input.value) || 0));
      input.value = String(number);
      set(number);
    });
    return input;
  }

  function stacked(label, control, note) {
    const row = field(label, control, note);
    row.classList.add("stack");
    return row;
  }

  function turned(angle) {
    return Math.round(((((Number(angle) || 0) + 180) % 360) + 360) % 360 - 180);
  }

  function pair(...parts) {
    return el("span", { class: "color-control" }, ...parts);
  }

  function placeFields(item) {
    const at = item.at[shape()];
    const here = shape() === "wide" ? "sideways" : "upright";
    if (!at) {
      const other = shape() === "wide" ? "tall" : "wide";
      return [field(`Not on the ${here} screen`, el("button", { type: "button", onclick: () => {
        item.at[shape()] = { ...(item.at[other] ?? { x: 0, y: 0, w: 60, h: 30 }) };
        ctx.changed({ rebuild: true });
      } }, `Show it ${here} too`))];
    }
    const set = (key) => (value) => {
      at[key] = value;
      ctx.changed();
    };
    const fields = [];
    if (item.kind === "line") {
      fields.push(
        stacked("Pivot", pair("Left", numberInput(at.x, set("x")), "Top", numberInput(at.y, set("y")))),
        stacked("Length", pair(numberInput(at.w, set("w"), { least: 1, most: 400 }), "Gap from the pivot", numberInput(item.start ?? 0, (value) => {
          item.start = value;
          ctx.changed();
        }, { least: 0, most: 300 }))),
        stacked("Angle", range(turned(item.angle), -180, 180, (value) => {
          item.angle = value;
          ctx.changed();
        }, "°"), "Drag the end of the line on the screen too"),
      );
      return fields;
    }
    fields.push(stacked("Place", pair("Left", numberInput(at.x, set("x")), "Top", numberInput(at.y, set("y")))));
    fields.push(stacked("Size", pair("Width", numberInput(at.w, set("w"), { least: 1, most: 400 }), "Height", numberInput(at.h, set("h"), { least: 1, most: 400 }))));
    return fields;
  }

  function colourField(item, key, label) {
    return field(label, choiceColour(item[key], (value) => {
      item[key] = value;
      ctx.changed();
    }));
  }

  function kindFields(item, screen) {
    const set = (key, show = false) => (value) => {
      item[key] = value;
      ctx.changed({ rebuild: show });
    };
    const fields = [];
    switch (item.kind) {
      case "box":
        fields.push(field("Fill", chips([["solid", "Colour"], ["gradient", "Top to bottom"], ["wallpaper", "Wallpaper"], ["none", "None"]], item.fill ?? "solid", set("fill", true))));
        if (item.fill === "gradient") {
          fields.push(colourField(item, "top", "Top"), colourField(item, "bottom", "Bottom"));
        } else if (item.fill === "solid" || !item.fill) {
          fields.push(colourField(item, "color", "Colour"));
        }
        fields.push(field("Corners", range(item.radius ?? 0, 0, 40, set("radius"))));
        fields.push(field("Border", range(item.border ?? 0, 0, 12, set("border", true))));
        if ((item.border ?? 0) > 0) {
          fields.push(colourField(item, "borderColor", "Border colour"));
        }
        fields.push(field("Narrower at the bottom", range(item.taper ?? 0, 0, 60, set("taper"))));
        fields.push(field("Grid lines over it", range(item.grid ?? 0, 0, 12, set("grid", true)), "Lines every so many pixels; 0 for none"));
        if ((item.grid ?? 0) > 1) {
          fields.push(colourField(item, "gridColor", "Grid colour"));
        }
        break;
      case "circle":
        fields.push(colourField(item, "color", "Colour"));
        fields.push(field("Ring", range(item.thickness ?? 0, 0, 60, set("thickness")), "0 fills the circle"));
        break;
      case "line":
        fields.push(colourField(item, "color", "Colour"));
        fields.push(field("Thickness", numberInput(item.thickness ?? 2, set("thickness"), { least: 0.25, most: 20, step: 0.25 })));
        break;
      case "text": {
        const sources = Object.entries(TEXT_SOURCES).filter(([key]) => key !== "item" || screen === "home");
        const source = el("select", {}, sources.map(([value, label]) => new Option(label, value)));
        source.value = item.source;
        source.addEventListener("change", () => set("source", true)(source.value));
        fields.push(field("Shows", source));
        if (item.source === "custom") {
          const words = el("input", { value: item.custom ?? "", maxlength: 40 });
          words.addEventListener("input", () => {
            item.custom = words.value;
            ctx.changed();
            const named = ctx.inspector.querySelector('.layers [aria-selected="true"] span');
            if (named) {
              named.textContent = nameOf(item);
            }
          });
          fields.push(field("Words", words));
        }
        fields.push(field("Font", chips([["large", "Titles"], ["medium", "Lists"], ["small", "Small text"]], item.size ?? "medium", set("size", true)), "The theme's three fonts"));
        const at = item.at[shape()];
        if (at) {
          fields.push(field("Line up", chips([["left", "Left"], ["center", "Centre"], ["right", "Right"]], at.align ?? item.align ?? "left", (value) => {
            at.align = value;
            ctx.changed({ rebuild: true });
          })));
        }
        fields.push(colourField(item, "color", "Colour"));
        fields.push(field("Scroll long text", toggle(Boolean(item.scroll), set("scroll"))));
        break;
      }
      case "cover":
        fields.push(field("Round", toggle(item.shape === "circle", (round) => set("shape", true)(round ? "circle" : "square"))));
        if (item.shape !== "circle") {
          fields.push(field("Reflection", toggle(Boolean(item.reflection), set("reflection")), "Make the box taller than it is wide to show more"));
        }
        fields.push(colourField(item, "placeholder", screen === "home" ? "With no album art" : "Without a cover"));
        break;
      case "symbol": {
        const marks = el("select", {}, Object.entries(MARKS).map(([value, label]) => new Option(label, value)));
        marks.value = item.mark ?? "state";
        marks.addEventListener("change", () => set("mark")(marks.value));
        fields.push(field("Shows", marks), colourField(item, "color", "Colour"));
        break;
      }
      case "number": {
        const shows = el("select", {}, Object.entries(NUMBERS).map(([value, label]) => new Option(label, value)));
        shows.value = item.show ?? "time";
        shows.addEventListener("change", () => set("show")(shows.value));
        fields.push(field("Shows", shows), colourField(item, "lit", "Lit"), colourField(item, "dim", "Unlit"));
        fields.push(field("Slant", range(item.lean ?? 8, 0, 20, set("lean"), "")));
        break;
      }
      case "menu":
        fields.push(el("p", { class: "hint" }, "The theme's own list, in its rows, selected row and arrows. Change those under Lists & shapes."));
        break;
      case "group":
        fields.push(...groupFields(item, screen));
        break;
      default:
        break;
    }
    return fields;
  }

  function groupFields(item, screen) {
    const fields = [field("Only show inside the box", toggle(Boolean(item.clip), (on) => {
      item.clip = on;
      ctx.changed();
    }), "Cuts off what spills over")];
    const kinds = [["none", "Once"], ["copies", "Copies"]];
    if (screen === "home") {
      kinds.push(["items", "One per menu item"]);
    }
    const mode = !item.repeat ? "none" : item.repeat.count === "items" ? "items" : "copies";
    fields.push(field("Draw it", chips(kinds, mode, (value) => {
      item.repeat = value === "none" ? null : {
        count: value === "items" ? "items" : 6, columns: value === "items" ? 2 : 6, dx: 30, dy: 30, turn: 0, start: 0, spin: 0,
      };
      ctx.changed({ rebuild: true });
    })));
    if (!item.repeat) {
      fields.push(el("p", { class: "hint" }, "Add parts inside by picking the group, then adding. Drag the group to move them together."));
      return fields;
    }
    const repeat = item.repeat;
    const set = (key) => (value) => {
      repeat[key] = value;
      ctx.changed();
    };
    if (repeat.count !== "items") {
      fields.push(field("How many", numberInput(repeat.count, set("count"), { least: 1, most: 96 })));
    }
    fields.push(field("Across", numberInput(repeat.columns ?? 1, set("columns"), { least: 1, most: 999 }), "Copies in a row before starting the next"));
    fields.push(stacked("Step", pair("Right", numberInput(repeat.dx ?? 0, set("dx")), "Down", numberInput(repeat.dy ?? 0, set("dy")))));
    fields.push(field("Turn each copy", numberInput(repeat.turn ?? 0, set("turn"), { least: -360, most: 360, step: 0.5, width: 72 }), "Degrees, around the middle of the group"));
    fields.push(field("Keep turning", range(repeat.spin ?? 0, -16, 16, set("spin"), ""), "While playing; 0 stands still"));
    return fields;
  }

  function sourceSelect(current, set, { spin = false, copy = false } = {}) {
    const options = [["", "nothing"], ...Object.entries(VALUES).filter(([key]) => copy || (key !== "beat" && key !== "copy"))];
    if (spin) {
      options.push(["spin", "keeps turning"]);
    }
    const select = el("select", {}, options.map(([value, label]) => new Option(label, value)));
    select.value = current ?? "";
    select.addEventListener("change", () => set(select.value));
    return select;
  }

  function insideCopies(item) {
    let found = false;
    let parent = findElement(drawing(), item.id)?.parent;
    while (parent) {
      found ||= Boolean(parent.repeat);
      parent = findElement(drawing(), parent.id)?.parent;
    }
    return found;
  }

  function movesFields(item) {
    const props = BINDABLE[item.kind] ?? [];
    const copy = insideCopies(item);
    return props.map(([key, label]) => {
      const rule = item.bind?.[key];
      const at = item.at[shape()] ?? {};
      const sized = key === "w" || key === "h";
      const own = key === "angle" ? turned(item.angle) : Number(at[key]) || 0;
      const update = (source) => {
        item.bind ??= {};
        if (!source) {
          delete item.bind[key];
        } else if (source === "spin") {
          item.bind[key] = { source, speed: 3 };
        } else {
          item.bind[key] = { source, from: rule?.from ?? (sized ? 0 : own), to: rule?.to ?? (sized ? 100 : own) };
        }
        ctx.changed({ rebuild: true });
      };
      const parts = [sourceSelect(rule?.source, update, { spin: key === "angle", copy })];
      if (rule?.source === "spin") {
        parts.push("speed", numberInput(rule.speed ?? 3, (value) => {
          rule.speed = value;
          ctx.changed();
        }, { least: -16, most: 16, width: 56 }));
      } else if (rule?.source) {
        const limits = sized ? { least: 0, most: 100, width: 60 } : { step: 0.1, width: 72 };
        const unit = sized ? "%" : key === "angle" ? "°" : "";
        parts.push("from", numberInput(Math.round((rule.from ?? (sized ? 0 : own)) * 10) / 10, (value) => {
          rule.from = value;
          ctx.changed();
        }, limits), unit, "to", numberInput(Math.round((rule.to ?? (sized ? 100 : own)) * 10) / 10, (value) => {
          rule.to = value;
          ctx.changed();
        }, limits), unit);
      }
      return stacked(`${label} follows`, el("span", { class: "color-control wrap" }, ...parts));
    });
  }

  function conditionControls(rule, update, copy) {
    const options = [["", "never"], ...Object.entries(CONDITIONS).filter(([key]) => copy || (key !== "lit" && key !== "highlighted"))];
    const select = el("select", {}, options.map(([value, label]) => new Option(label, value)));
    select.value = rule?.on ?? "";
    select.addEventListener("change", () => update(select.value ? { on: select.value, not: false } : null));
    const parts = [select];
    if (rule?.on) {
      parts.unshift(el("label", { class: "inline" }, toggle(Boolean(rule.not), (not) => update({ ...rule, not })), "not"));
    }
    if (rule?.on === "lit" || rule?.on === "above") {
      parts.push(sourceSelect(rule.source ?? (rule.on === "lit" ? "progress" : "left"), (source) => update({ ...rule, source }), { copy }));
    }
    if (rule?.on === "above") {
      parts.push(numberInput(rule.level ?? 74, (level) => update({ ...rule, level }), { least: 0, most: 100, width: 56 }), "%");
    }
    return el("span", { class: "color-control wrap" }, ...parts);
  }

  function colourChangeFields(item) {
    const copy = insideCopies(item);
    return (COLOURED[item.kind] ?? []).map(([key, label]) => {
      const rule = item.bind?.[key];
      const update = (when) => {
        item.bind ??= {};
        if (!when) {
          delete item.bind[key];
        } else {
          item.bind[key] = { when, color: rule?.color ?? item[key] };
        }
        ctx.changed({ rebuild: true });
      };
      const row = [conditionControls(rule?.when, update, copy)];
      if (rule?.when) {
        row.push(choiceColour(rule.color, (value) => {
          rule.color = value;
          ctx.changed();
        }));
      }
      return stacked(`${label} changes when`, el("span", { class: "stack-control" }, ...row));
    });
  }

  function showField(item) {
    const rule = item.bind?.show?.when;
    return stacked("Show only when", conditionControls(rule, (when) => {
      item.bind ??= {};
      if (when) {
        item.bind.show = { when };
      } else {
        delete item.bind.show;
      }
      ctx.changed({ rebuild: true });
    }, insideCopies(item)));
  }

  function itemPanel(item, screen) {
    const name = el("input", { value: item.name ?? "", placeholder: nameOf({ ...item, name: "" }), maxlength: 30 });
    name.addEventListener("input", () => {
      item.name = name.value;
      ctx.remember();
    });
    const found = findElement(drawing(), item.id);
    const actions = [el("button", { type: "button", onclick: () => {
      ctx.state.picked = null;
      ctx.render();
    } }, "Done")];
    if (found?.parent) {
      actions.push(el("button", { type: "button", onclick: () => outOfGroup(item) }, "Take out of the group"));
    } else {
      const list = siblingsOf(item);
      if (list.slice(0, list.indexOf(item)).some((other) => other.kind === "group")) {
        actions.push(el("button", { type: "button", onclick: () => intoGroup(item) }, "Put in the group above"));
      }
    }
    const moves = movesFields(item);
    const colours = colourChangeFields(item);
    const bound = Object.keys(item.bind ?? {}).length > 0;
    const sections = [
      el("h3", {}, nameOf(item)),
      group(field("Name", name), ...kindFields(item, screen), ...placeFields(item)),
      el("div", { class: "actions" }, ...actions),
    ];
    const live = [...moves, ...colours, showField(item)];
    sections.push(el("details", { class: "moves", open: bound || undefined },
      el("summary", {}, "Moves and changes with the music"), group(...live)));
    return sections;
  }

  function copyOtherSide() {
    const from = shape() === "wide" ? "tall" : "wide";
    const [a, b] = [AREAS[from], AREAS[shape()]];
    const scale = Math.min(b.width / a.width, b.height / a.height);
    const across = (b.width - a.width * scale) / 2;
    const down = (b.height - a.height * scale) / 2;
    const scaleOne = (item, top) => {
      const box = item.at?.[from];
      if (!box) {
        return;
      }
      item.at[shape()] = {
        ...box,
        x: Math.round(box.x * scale + (top ? across : 0)),
        y: Math.round(box.y * scale + (top ? down : 0)),
        w: Math.max(1, Math.round(box.w * scale)),
        h: Math.max(item.kind === "line" ? 0 : 1, Math.round(box.h * scale)),
      };
      for (const child of item.children ?? []) {
        scaleOne(child, false);
      }
    };
    for (const item of drawing().elements) {
      scaleOne(item, true);
    }
    ctx.changed({ rebuild: true });
  }

  function partList() {
    const list = el("ul", { class: "layers group scroller", role: "listbox", "aria-label": "Parts, back to front", onscroll: () => {
      listScroll = list.scrollTop;
    } }, tree(drawing().elements, 0));
    requestAnimationFrame(() => {
      list.scrollTop = listScroll;
      const row = list.querySelector('[aria-selected="true"]');
      if (row && shownPick !== ctx.state.picked && (row.offsetTop < list.scrollTop || row.offsetTop + row.offsetHeight > list.scrollTop + list.clientHeight)) {
        list.scrollTop = row.offsetTop - list.clientHeight / 2;
      }
      shownPick = ctx.state.picked;
      listScroll = list.scrollTop;
    });
    return list;
  }

  function panel(screen) {
    const item = picked();
    const other = shape() === "wide" ? "upright" : "sideways";
    const settings = item ? itemPanel(item, screen) : [];
    if (item && settingsShown !== item.id) {
      requestAnimationFrame(() => settings[0]?.scrollIntoView({ block: "nearest" }));
    }
    settingsShown = item?.id ?? null;
    return [
      ...settings,
      el("h3", {}, "On the screen, back to front"),
      el("p", { class: "hint" }, `You're drawing the ${shape() === "wide" ? "sideways" : "upright"} screen. Turn the player above to draw the ${other} one.`),
      partList(),
      el("p", { class: "hint" }, item?.kind === "group" ? "New parts go inside the picked group." : "Pick a group first to add parts inside it."),
      addMenu(screen),
      el("div", { class: "actions" }, el("button", { type: "button", onclick: copyOtherSide }, `Copy the ${other} drawing`)),
    ];
  }

  return { panel, overlay, down, deeper, move, up, nudge, nameOf, colorOf, walk };
}

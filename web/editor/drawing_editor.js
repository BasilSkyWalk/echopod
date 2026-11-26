import {
  AREAS,
  CONDITIONS,
  KINDS,
  MARKS,
  NUMBERS,
  TEXT_SOURCES,
  VALUES,
  canMoveParts,
  canStretch,
  colorOf,
  element,
  findElement,
  fitStretched,
  moveParts,
  newElement,
  pathOf,
  walk,
} from "./drawing.js";
import { SHELF, SHELF_ART, SHELF_MORE, fitTiles, isTiles, tileRows, tilesFor } from "./drawings.js";

const HANDLE = 7;
const HIT = 4;
const SNAP = 3;
const GUIDE = "#ff4fd8";
const PASTE_STEP = 6;
const CLIPBOARD = "echopod-parts-clipboard";
const DEGREES = 180 / Math.PI;
const DRAG_START = 5;
const LIST_EDGE = 28;
const LIST_SCROLL = 10;
const GRIP = "M8 5h.01M12 5h.01M8 10h.01M12 10h.01M8 15h.01M12 15h.01";
const LOCK = "M6.5 9V7a3.5 3.5 0 0 1 7 0v2M5 9h10v7.5H5z";
const UNLOCK = "M6.5 9V7a3.5 3.5 0 0 1 6.6-1.6M5 9h10v7.5H5z";
const ALIGN = [
  ["left", "Line up the left edges", "M4 3v14M7 6h9v3H7zM7 11h5v3H7z"],
  ["centre", "Line up the middles, across", "M10 3v14M5 6h10v3H5zM7 11h6v3H7z"],
  ["right", "Line up the right edges", "M16 3v14M4 6h9v3H4zM8 11h5v3H8z"],
  ["top", "Line up the tops", "M3 4h14M6 7h3v9H6zM11 7h3v5h-3z"],
  ["middle", "Line up the middles, down", "M3 10h14M6 5h3v10H6zM11 7h3v6h-3z"],
  ["bottom", "Line up the bottoms", "M3 16h14M6 4h3v9H6zM11 8h3v5h-3z"],
];
const SPREAD = [
  ["across", "Space evenly across", "M3 4v12M17 4v12M7 7h2v6H7zM11 7h2v6h-2z"],
  ["down", "Space evenly down", "M4 3h12M4 17h12M7 7h6v2H7zM7 11h6v2H7z"],
];
let clipboard = null;
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
  let inside = null;

  const drawing = () => ctx.drawing();
  const shape = () => ctx.shape();
  const picked = () => (drawing() ? findElement(drawing(), ctx.state.picked)?.item ?? null : null);

  function chosen() {
    const first = picked();
    if (!first) {
      return [];
    }
    const more = ctx.state.also?.anchor === first.id ? ctx.state.also.ids : [];
    return [first, ...more.map((id) => findElement(drawing(), id)?.item).filter(Boolean)];
  }

  function setChosen(ids) {
    ctx.state.picked = ids[0] ?? null;
    ctx.state.also = ids.length > 1 ? { anchor: ids[0], ids: ids.slice(1) } : null;
  }

  function choose(id, add) {
    const current = chosen().map((item) => item.id);
    const parentOf = (one) => findElement(drawing(), one)?.parent ?? null;
    if (!add || current.length === 0 || parentOf(id) !== parentOf(current[0])) {
      setChosen([id]);
      return;
    }
    setChosen(current.includes(id) ? current.filter((one) => one !== id) : [...current, id]);
  }

  function clear() {
    setChosen([]);
  }

  function locked(place) {
    return Boolean(place.item.locked || place.chain.some((parent) => parent.locked));
  }

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

  function boxOf(place) {
    if (place.item.kind === "line") {
      const { from, to } = lineEnds(place);
      return { x: Math.min(from.x, to.x), y: Math.min(from.y, to.y), w: Math.abs(to.x - from.x), h: Math.abs(to.y - from.y) };
    }
    return { x: place.x, y: place.y, w: place.w, h: place.h };
  }

  function union(boxes) {
    const left = Math.min(...boxes.map((box) => box.x));
    const top = Math.min(...boxes.map((box) => box.y));
    const right = Math.max(...boxes.map((box) => box.x + box.w));
    const bottom = Math.max(...boxes.map((box) => box.y + box.h));
    return { x: left, y: top, w: right - left, h: bottom - top };
  }

  function frameOf(place) {
    const parent = place.chain.at(-1);
    if (parent) {
      const outer = placements().find((one) => one.item === parent);
      if (outer) {
        return { x: outer.x, y: outer.y, w: outer.w, h: outer.h };
      }
    }
    const area = AREAS[shape()];
    return { x: 0, y: area.top, w: area.width, h: area.height };
  }

  function frame() {
    const picks = new Set(chosen());
    const boxes = placements().filter((place) => picks.has(place.item)).map(boxOf);
    return boxes.length ? union(boxes) : null;
  }

  function snapTargets(moving) {
    const area = AREAS[shape()];
    const xs = [0, area.width / 2, area.width];
    const ys = [area.top, area.top + area.height / 2, area.top + area.height];
    const skip = new Set(moving.map(({ place }) => place.item));
    for (const place of placements()) {
      if (skip.has(place.item) || place.chain.some((parent) => skip.has(parent)) || place.item.kind === "group") {
        continue;
      }
      const box = boxOf(place);
      xs.push(box.x, box.x + box.w / 2, box.x + box.w);
      ys.push(box.y, box.y + box.h / 2, box.y + box.h);
    }
    return { xs, ys };
  }

  function snapped(lines, targets) {
    let best = null;
    for (const line of lines) {
      for (const target of targets) {
        const gap = target - line;
        if (Math.abs(gap) <= SNAP && (!best || Math.abs(gap) < Math.abs(best.shift))) {
          best = { shift: gap, at: target };
        }
      }
    }
    return best ? { shift: Math.round(best.shift), at: best.at } : null;
  }

  function overlay(pen, zoom) {
    const places = placements();
    const shown = shownLevel(places);
    const picks = new Set(chosen().map((item) => item.id));
    for (const place of places) {
      if (!shown(place) && !picks.has(place.item.id)) {
        continue;
      }
      const isPicked = picks.has(place.item.id);
      const chosen = isPicked && picks.size === 1 && !locked(place) && !stretched(place);
      pen.setLineDash(isPicked ? [] : [4, 3]);
      pen.lineWidth = isPicked ? 1.5 : 1;
      pen.strokeStyle = isPicked ? "#ffffff" : locked(place) ? "rgba(255, 255, 255, 0.12)" : "rgba(255, 255, 255, 0.3)";
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
    pen.setLineDash([]);
    pen.lineWidth = 1;
    pen.strokeStyle = GUIDE;
    for (const guide of drag?.guides ?? []) {
      pen.beginPath();
      if ("x" in guide) {
        pen.moveTo(Math.round(guide.x * zoom) + 0.5, 0);
        pen.lineTo(Math.round(guide.x * zoom) + 0.5, pen.canvas.height);
      } else {
        pen.moveTo(0, Math.round(guide.y * zoom) + 0.5);
        pen.lineTo(pen.canvas.width, Math.round(guide.y * zoom) + 0.5);
      }
      pen.stroke();
    }
    if (drag?.marquee) {
      const box = marqueeBox(drag.marquee);
      pen.setLineDash([3, 3]);
      pen.fillStyle = "rgba(255, 79, 216, 0.08)";
      pen.fillRect(box.x * zoom, box.y * zoom, box.w * zoom, box.h * zoom);
      pen.strokeRect(box.x * zoom + 0.5, box.y * zoom + 0.5, box.w * zoom, box.h * zoom);
      pen.setLineDash([]);
    }
  }

  function marqueeBox(marquee) {
    return {
      x: Math.min(marquee.x0, marquee.x1),
      y: Math.min(marquee.y0, marquee.y1),
      w: Math.abs(marquee.x1 - marquee.x0),
      h: Math.abs(marquee.y1 - marquee.y0),
    };
  }

  function nearHandle(place, point) {
    const handle = handleOf(place);
    const reach = (ctx.coarse?.() ? 2 * HANDLE : HANDLE) / ctx.state.zoom + 1;
    return Math.abs(point.x - handle.x) <= reach && Math.abs(point.y - handle.y) <= reach;
  }

  function startDrag(primary, resize, point) {
    const places = placements();
    const picked = resize ? [primary] : chosen().map((item) => places.find((place) => place.item === item)).filter((place) => place && !locked(place));
    const moving = resize ? picked : [...new Set(picked.map((place) => carrier(place, places)).filter(Boolean))];
    const held = moving.map((place) => ({ place, start: { ...place.item.at[shape()] } }));
    drag = { primary, resize, from: point, moving: held, targets: snapTargets(held), guides: [] };
  }

  function down(point, { add = false } = {}) {
    const places = placements();
    const picks = chosen();
    const current = places.find((place) => place.item.id === ctx.state.picked);
    if (!add && picks.length === 1 && current && !locked(current) && !stretched(current) && nearHandle(current, point)) {
      startDrag(current, true, point);
      return true;
    }
    const path = pathAt(places.filter((place) => !locked(place)), point);
    if (!path) {
      if (!add) {
        clear();
      }
      drag = { marquee: { x0: point.x, y0: point.y, x1: point.x, y1: point.y }, parent: current?.chain.at(-1) ?? null };
      ctx.render();
      return true;
    }
    const index = path.findIndex((item) => item.id === ctx.state.picked);
    const parent = current?.chain.at(-1);
    const level = index >= 0 ? index : Math.max(0, path.indexOf(parent) + 1);
    const target = places.find((place) => place.item === path[level]);
    if (add) {
      choose(target.item.id, true);
      ctx.render();
      return false;
    }
    if (!picks.includes(target.item)) {
      choose(target.item.id, false);
    }
    reveal(target.item.id);
    startDrag(target, false, point);
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

  function move(point, { free = false } = {}) {
    if (!drag) {
      const places = placements();
      const current = places.find((place) => place.item.id === ctx.state.picked);
      if (current && !locked(current) && !stretched(current) && chosen().length === 1 && nearHandle(current, point)) {
        return current.item.kind === "line" ? "crosshair" : "nwse-resize";
      }
      return places.some((place) => !locked(place) && hit(place, point)) ? "move" : "default";
    }
    if (drag.marquee) {
      drag.marquee.x1 = point.x;
      drag.marquee.y1 = point.y;
      return null;
    }
    const { primary, resize, from, moving, targets } = drag;
    let dx = Math.round(point.x - from.x);
    let dy = Math.round(point.y - from.y);
    drag.guides = [];
    if (resize && primary.item.kind === "line") {
      const at = primary.item.at[shape()];
      const ax = point.x - primary.x;
      const ay = point.y - primary.y;
      primary.item.angle = Math.round((Math.atan2(ax, -ay) * DEGREES + 360) % 360);
      at.w = Math.max(1, Math.round(Math.hypot(ax, ay) - (Number(primary.item.start) || 0)));
      ctx.apply();
      return null;
    }
    if (resize) {
      const { start } = moving[0];
      if (!free) {
        const across = snapped([primary.x + start.w + dx], targets.xs);
        const down = snapped([primary.y + start.h + dy], targets.ys);
        if (across) {
          dx += across.shift;
          drag.guides.push({ x: across.at });
        }
        if (down) {
          dy += down.shift;
          drag.guides.push({ y: down.at });
        }
      }
      const at = primary.item.at[shape()];
      at.w = Math.max(1, start.w + dx);
      at.h = Math.max(primary.item.kind === "line" ? 0 : 1, start.h + dy);
      if (isTiles(primary.item)) {
        fitTiles(primary.item, shape());
      }
      ctx.apply();
      return null;
    }
    if (!free && moving.length) {
      const box = union(moving.map(({ place }) => boxOf(place)));
      const across = snapped([box.x + dx, box.x + box.w / 2 + dx, box.x + box.w + dx], targets.xs);
      const down = snapped([box.y + dy, box.y + box.h / 2 + dy, box.y + box.h + dy], targets.ys);
      if (across) {
        dx += across.shift;
        drag.guides.push({ x: across.at });
      }
      if (down) {
        dy += down.shift;
        drag.guides.push({ y: down.at });
      }
    }
    for (const { place, start } of moving) {
      const at = place.item.at[shape()];
      at.x = start.x + dx;
      at.y = start.y + dy;
    }
    ctx.apply();
    return null;
  }

  function up() {
    if (!drag) {
      return;
    }
    if (drag.marquee) {
      const box = marqueeBox(drag.marquee);
      const parent = drag.parent;
      drag = null;
      if (box.w < 2 && box.h < 2) {
        ctx.render();
        return;
      }
      const inside = placements().filter((place) => (place.chain.at(-1) ?? null) === parent && !locked(place)).filter((place) => {
        const own = boxOf(place);
        return own.x < box.x + box.w && own.x + own.w > box.x && own.y < box.y + box.h && own.y + own.h > box.y;
      });
      const before = chosen().map((item) => item.id);
      const ids = [...before, ...inside.map((place) => place.item.id).filter((id) => !before.includes(id))];
      setChosen(ids);
      ctx.render();
      return;
    }
    drag = null;
    ctx.changed({ rebuild: true });
  }

  function nudge(dx, dy) {
    const places = placements();
    const picks = chosen().filter((item) => item.at?.[shape()]);
    if (!picks.length) {
      return false;
    }
    const items = [...new Set(picks.map((item) => {
      const place = places.find((one) => one.item === item);
      return place && stretched(place) ? carrier(place, places)?.item : item;
    }))].filter(Boolean);
    if (!items.length) {
      ctx.say("It fills the screen, so it stays put.", true);
      return true;
    }
    if (items.some((item) => item.locked)) {
      ctx.say("That part is locked. Unlock it in the list to move it.", true);
      return true;
    }
    for (const item of items) {
      item.at[shape()].x += dx;
      item.at[shape()].y += dy;
    }
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
      ctx.say("Put a group above it first, then move it in, or drag it onto a group in the list.", true);
      return;
    }
    moveInList([item.id], host.id, "inside");
  }

  function outOfGroup(item) {
    const host = findElement(drawing(), item.id)?.parent;
    if (host) {
      moveInList([item.id], host.id, "after");
    }
  }

  function moveInList(ids, targetId, where) {
    const before = JSON.stringify(drawing().elements);
    if (!moveParts(drawing(), ids, targetId, where)) {
      return false;
    }
    if (!ctx.fits()) {
      drawing().elements = JSON.parse(before);
      ctx.fits();
      ctx.say("There's no room to move that on this screen.", true);
      ctx.render();
      return false;
    }
    if (where === "inside") {
      expanded.add(targetId);
    }
    setChosen(ids);
    ctx.changed({ rebuild: true });
    return true;
  }

  function remove(item) {
    const list = siblingsOf(item);
    list.splice(list.indexOf(item), 1);
    ctx.state.picked = null;
    ctx.changed({ rebuild: true });
  }

  function renumber(node) {
    node.id = element(node.kind, null, null).id;
    for (const child of node.children ?? []) {
      renumber(child);
    }
  }

  function removeChosen() {
    const items = chosen();
    if (!items.length) {
      return false;
    }
    if (items.some((item) => item.locked)) {
      ctx.say("That part is locked. Unlock it in the list to remove it.", true);
      return true;
    }
    for (const item of items) {
      const list = siblingsOf(item);
      list.splice(list.indexOf(item), 1);
    }
    clear();
    ctx.changed({ rebuild: true });
    return true;
  }

  function duplicateChosen() {
    const items = chosen();
    if (!items.length) {
      return false;
    }
    const made = items.map((item) => {
      const copy = JSON.parse(JSON.stringify(item));
      renumber(copy);
      copy.locked = false;
      const at = copy.at[shape()];
      if (at) {
        at.x += PASTE_STEP;
        at.y += PASTE_STEP;
      }
      const list = siblingsOf(item);
      list.splice(list.indexOf(item) + 1, 0, copy);
      return copy;
    });
    if (!ctx.fits()) {
      for (const copy of made) {
        const list = siblingsOf(copy);
        list.splice(list.indexOf(copy), 1);
      }
      ctx.fits();
      ctx.say("There's no room for more on this screen. Remove something first.", true);
      return true;
    }
    setChosen(made.map((copy) => copy.id));
    ctx.changed({ rebuild: true });
    return true;
  }

  function lineBox(item, shapeName) {
    const at = item.at[shapeName];
    if (item.kind !== "line") {
      return { x: Number(at.x), y: Number(at.y), w: Number(at.w), h: Number(at.h) };
    }
    const ends = lineEnds({ item, x: Number(at.x), y: Number(at.y), w: Number(at.w) });
    return union([{ ...ends.from, w: 0, h: 0 }, { ...ends.to, w: 0, h: 0 }]);
  }

  function groupChosen() {
    const items = chosen();
    if (items.length < 2) {
      ctx.say("Pick two or more parts first: hold Shift and click them.", true);
      return true;
    }
    const list = siblingsOf(items[0]);
    const ordered = list.filter((item) => items.includes(item));
    const places = {};
    for (const shapeName of Object.keys(AREAS)) {
      const boxes = ordered.filter((item) => item.at[shapeName]).map((item) => lineBox(item, shapeName));
      if (boxes.length) {
        const box = union(boxes);
        places[shapeName] = { x: Math.floor(box.x), y: Math.floor(box.y), w: Math.max(1, Math.ceil(box.w)), h: Math.max(1, Math.ceil(box.h)) };
      } else {
        places[shapeName] = null;
      }
    }
    const made = element("group", places.wide, places.tall);
    const first = list.indexOf(ordered[0]);
    for (const item of ordered) {
      list.splice(list.indexOf(item), 1);
      for (const shapeName of Object.keys(AREAS)) {
        if (item.at[shapeName] && places[shapeName]) {
          item.at[shapeName].x -= places[shapeName].x;
          item.at[shapeName].y -= places[shapeName].y;
        }
      }
    }
    made.children = ordered;
    list.splice(first, 0, made);
    if (!ctx.fits()) {
      ungroup(made, false);
      ctx.fits();
      ctx.say("There's no room to group these on this screen.", true);
      return true;
    }
    expanded.add(made.id);
    setChosen([made.id]);
    ctx.changed({ rebuild: true });
    return true;
  }

  function ungroup(host, record = true) {
    const list = siblingsOf(host);
    const children = host.children ?? [];
    for (const item of children) {
      for (const shapeName of Object.keys(AREAS)) {
        if (item.at[shapeName] && host.at[shapeName]) {
          item.at[shapeName].x += host.at[shapeName].x;
          item.at[shapeName].y += host.at[shapeName].y;
        }
      }
    }
    list.splice(list.indexOf(host), 1, ...children);
    if (record) {
      setChosen(children.map((item) => item.id));
      ctx.changed({ rebuild: true });
    }
  }

  function lineUp(edge) {
    const places = placements();
    const mine = chosen().map((item) => places.find((place) => place.item === item)).filter((place) => place && !locked(place) && !stretched(place));
    if (!mine.length) {
      return;
    }
    const frame = mine.length === 1 ? frameOf(mine[0]) : union(mine.map(boxOf));
    for (const place of mine) {
      const box = boxOf(place);
      const at = place.item.at[shape()];
      const shift = {
        left: [frame.x - box.x, 0],
        centre: [frame.x + (frame.w - box.w) / 2 - box.x, 0],
        right: [frame.x + frame.w - box.x - box.w, 0],
        top: [0, frame.y - box.y],
        middle: [0, frame.y + (frame.h - box.h) / 2 - box.y],
        bottom: [0, frame.y + frame.h - box.y - box.h],
      }[edge];
      at.x += Math.round(shift[0]);
      at.y += Math.round(shift[1]);
    }
    ctx.changed({ rebuild: true });
  }

  function spread(axis) {
    const places = placements();
    const mine = chosen().map((item) => places.find((place) => place.item === item)).filter((place) => place && !locked(place) && !stretched(place));
    if (mine.length < 3) {
      return;
    }
    const [start, size, key] = axis === "across" ? ["x", "w", "x"] : ["y", "h", "y"];
    const sorted = mine.map((place) => ({ place, box: boxOf(place) })).sort((a, b) => a.box[start] - b.box[start]);
    const first = sorted[0].box[start];
    const last = sorted.at(-1).box[start] + sorted.at(-1).box[size];
    const gap = (last - first - sorted.reduce((sum, one) => sum + one.box[size], 0)) / (sorted.length - 1);
    let next = first;
    for (const { place, box } of sorted) {
      place.item.at[shape()][key] += Math.round(next) - Math.round(box[start]);
      next += box[size] + gap;
    }
    ctx.changed({ rebuild: true });
  }

  function heldParts() {
    if (clipboard) {
      return clipboard;
    }
    try {
      return localStorage.getItem(CLIPBOARD);
    } catch {
      return null;
    }
  }

  function keepParts(text) {
    try {
      localStorage.setItem(CLIPBOARD, text);
      return true;
    } catch {
      return false;
    }
  }

  function copyChosen() {
    const items = chosen();
    if (!items.length) {
      return false;
    }
    clipboard = JSON.stringify({ screen: drawing().screen, items });
    keepParts(clipboard);
    const other = drawing().screen === "home" ? "Now Playing" : "Home";
    ctx.say(`Copied ${items.length === 1 ? nameOf(items[0]) : `${items.length} parts`}. Paste here or on ${other}.`);
    ctx.render();
    return true;
  }

  function suitedTo(item, screen) {
    if (screen === "home") {
      return true;
    }
    if (item.kind === "menu") {
      return false;
    }
    if (item.kind === "text" && item.source === "item") {
      item.source = "song";
    }
    if (item.kind === "symbol" && item.mark === "item") {
      item.mark = "state";
    }
    if (item.repeat?.count === "items") {
      item.repeat.count = 6;
    }
    for (const [key, rule] of Object.entries(item.bind ?? {})) {
      if (rule?.when?.on === "highlighted" || rule?.source === "copy") {
        delete item.bind[key];
      }
    }
    if (item.children) {
      item.children = item.children.filter((child) => suitedTo(child, screen));
    }
    return true;
  }

  function paste() {
    const text = heldParts();
    if (!text || !drawing()) {
      return false;
    }
    const held = JSON.parse(text);
    const screen = drawing().screen;
    const host = picked()?.kind === "group" && chosen().length === 1 ? picked() : null;
    const list = host ? host.children : drawing().elements;
    const made = [];
    let left = 0;
    for (const original of held.items) {
      const copy = JSON.parse(JSON.stringify(original));
      if (!suitedTo(copy, screen)) {
        left++;
        continue;
      }
      renumber(copy);
      copy.locked = false;
      const at = copy.at?.[shape()];
      if (at && held.screen === screen && !host) {
        at.x += PASTE_STEP;
        at.y += PASTE_STEP;
      }
      list.push(copy);
      made.push(copy);
    }
    if (!made.length) {
      ctx.say("The menu only goes on Home, so there was nothing to paste here.", true);
      return true;
    }
    if (!ctx.fits()) {
      list.splice(list.length - made.length, made.length);
      ctx.fits();
      ctx.say("There's no room for those on this screen. Remove something first.", true);
      return true;
    }
    if (host) {
      expanded.add(host.id);
    }
    setChosen(made.map((item) => item.id));
    ctx.changed({ rebuild: true });
    if (left) {
      ctx.say("The menu only goes on Home, so it wasn't pasted.", true);
    }
    return true;
  }

  function pickEverything() {
    const parent = findElement(drawing(), ctx.state.picked)?.parent ?? null;
    const list = parent ? parent.children : drawing().elements;
    setChosen(list.filter((item) => !item.locked && !item.hidden).map((item) => item.id));
    ctx.render();
    return true;
  }

  function key(event) {
    if (!drawing()) {
      return false;
    }
    const command = event.ctrlKey || event.metaKey;
    const name = event.key.toLowerCase();
    if (command && name === "c") {
      return copyChosen();
    }
    if (command && name === "x") {
      return copyChosen() && removeChosen();
    }
    if (command && name === "v") {
      return paste();
    }
    if (command && name === "d") {
      return duplicateChosen();
    }
    if (command && name === "a") {
      return pickEverything();
    }
    if (command && name === "g") {
      const item = picked();
      if (event.shiftKey) {
        if (item?.kind === "group" && chosen().length === 1) {
          ungroup(item);
          return true;
        }
        return false;
      }
      return groupChosen();
    }
    if (name === "delete" || name === "backspace") {
      return removeChosen();
    }
    if (name === "escape" && picked()) {
      clear();
      ctx.render();
      return true;
    }
    return false;
  }

  function duplicate(item) {
    const copy = JSON.parse(JSON.stringify(item));
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

  function renameInPlace(item, label) {
    const input = el("input", { class: "rename", value: item.name ?? "", placeholder: nameOf({ ...item, name: "" }), maxlength: 30, "aria-label": "Name" });
    let done = false;
    const finish = (keep) => {
      if (done) {
        return;
      }
      done = true;
      if (keep && input.value !== (item.name ?? "")) {
        item.name = input.value.trim();
        ctx.changed({ rebuild: true });
      } else {
        ctx.render();
      }
    };
    input.addEventListener("click", (event) => event.stopPropagation());
    input.addEventListener("keydown", (event) => {
      event.stopPropagation();
      if (event.key === "Enter") {
        finish(true);
      } else if (event.key === "Escape") {
        finish(false);
      }
    });
    input.addEventListener("blur", () => finish(true));
    label.replaceWith(input);
    input.focus();
    input.select();
  }

  function tree(items, depth) {
    const picks = new Set(chosen().map((item) => item.id));
    return items.flatMap((item) => {
      const label = el("span", { class: "layer-name", title: "Double-click to rename" }, nameOf(item));
      label.addEventListener("dblclick", (event) => {
        event.stopPropagation();
        renameInPlace(item, label);
      });
      const row = el("li", {
        role: "option",
        "data-part": String(item.id),
        "aria-selected": String(picks.has(item.id)),
        class: [item.hidden ? "hidden-layer" : "", item.at?.[shape()] ? "" : "elsewhere", item.locked ? "locked-layer" : ""].join(" ").trim(),
        style: `padding-left: ${10 + depth * 16}px`,
        onclick: (event) => {
          if (listDrag?.dropped) {
            listDrag = null;
            return;
          }
          choose(item.id, event.shiftKey || event.ctrlKey || event.metaKey);
          ctx.render();
        },
        onpointerdown: (event) => grab(event, item),
      },
      label,
      el("span", { class: "tools" },
        tool(item.locked ? "Unlock" : "Lock so it can't be moved on the screen", item.locked ? LOCK : UNLOCK, () => {
          item.locked = !item.locked;
          ctx.changed({ rebuild: true });
        }),
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
      const grip = el("span", { class: "grip", title: "Drag to move it, or onto a group to put it inside", "aria-hidden": "true" }, icon(GRIP));
      label.prepend(grip);
      if (item.kind !== "group") {
        return [row];
      }
      const open = expanded.has(item.id);
      label.prepend(el("button", { type: "button", class: "fold", title: open ? "Fold away" : "Show what's inside", "aria-expanded": String(open), onclick: (event) => {
        event.stopPropagation();
        if (open) {
          expanded.delete(item.id);
        } else {
          expanded.add(item.id);
        }
        ctx.render();
      } }, icon(open ? "m6 8 4 4 4-4" : "m8 6 4 4-4 4")));
      label.prepend(grip);
      return open ? [row, ...tree(item.children ?? [], depth + 1)] : [row];
    });
  }

  let listDrag = null;

  function grab(event, item) {
    const fromGrip = Boolean(event.target.closest(".grip"));
    if (event.button !== 0 || event.target.closest("button, input") || (event.pointerType === "touch" && !fromGrip)) {
      return;
    }
    const picks = chosen().map((one) => one.id);
    const ids = picks.includes(item.id) ? picks : [item.id];
    listDrag = { ids, x: event.clientX, y: event.clientY, started: false, drop: null, list: event.currentTarget.closest(".layers"), dropped: false };
    if (fromGrip) {
      event.preventDefault();
    }
    window.addEventListener("pointermove", drift);
    window.addEventListener("pointerup", release, { once: true });
    window.addEventListener("pointercancel", release, { once: true });
  }

  function dropAt(event) {
    const row = document.elementFromPoint(event.clientX, event.clientY)?.closest?.(".layers li[data-part]");
    if (!row || !listDrag.list.contains(row)) {
      return null;
    }
    const id = Number(row.dataset.part);
    const target = findElement(drawing(), id)?.item;
    if (!target) {
      return null;
    }
    const box = row.getBoundingClientRect();
    const share = (event.clientY - box.top) / Math.max(1, box.height);
    const where = target.kind === "group"
      ? share < 0.25 ? "before" : share > 0.75 ? "after" : "inside"
      : share < 0.5 ? "before" : "after";
    return canMoveParts(drawing(), listDrag.ids, id, where) ? { row, id, where } : null;
  }

  function showDrop(drop) {
    for (const marked of listDrag.list.querySelectorAll(".drop-before, .drop-after, .drop-inside")) {
      marked.classList.remove("drop-before", "drop-after", "drop-inside");
    }
    drop?.row.classList.add(`drop-${drop.where}`);
  }

  function drift(event) {
    if (!listDrag) {
      return;
    }
    if (!listDrag.started) {
      if (Math.hypot(event.clientX - listDrag.x, event.clientY - listDrag.y) < DRAG_START) {
        return;
      }
      listDrag.started = true;
      listDrag.list.classList.add("dragging-parts");
    }
    event.preventDefault();
    const box = listDrag.list.getBoundingClientRect();
    if (event.clientY < box.top + LIST_EDGE) {
      listDrag.list.scrollTop -= LIST_SCROLL;
    } else if (event.clientY > box.bottom - LIST_EDGE) {
      listDrag.list.scrollTop += LIST_SCROLL;
    }
    listDrag.drop = dropAt(event);
    showDrop(listDrag.drop);
  }

  function release() {
    window.removeEventListener("pointermove", drift);
    window.removeEventListener("pointerup", release);
    window.removeEventListener("pointercancel", release);
    if (!listDrag) {
      return;
    }
    const { started, drop, ids } = listDrag;
    showDrop(null);
    listDrag.list.classList.remove("dragging-parts");
    if (!started) {
      listDrag = null;
      return;
    }
    listDrag.dropped = true;
    setTimeout(() => {
      listDrag = null;
    });
    if (drop) {
      listScroll = listDrag.list.scrollTop;
      moveInList(ids, drop.id, drop.where);
    }
  }

  function isMenu(item) {
    return item.kind === "menu" || (item.kind === "group" && item.repeat?.count === "items");
  }

  function swapMenu(key, made) {
    const list = drawing().elements;
    const old = list.filter(isMenu);
    if (old.length === 0) {
      return false;
    }
    if (old.every((item) => (item.kind === "menu") === (key === "menu"))) {
      setChosen([old[0].id]);
      ctx.say("The menu is already on this screen. It's picked now.");
      ctx.render();
      return true;
    }
    const frames = {};
    for (const shapeName of Object.keys(AREAS)) {
      const boxes = old.map((item) => item.at?.[shapeName]).filter(Boolean);
      frames[shapeName] = boxes.length ? union(boxes.map((box) => ({ x: Number(box.x), y: Number(box.y), w: Number(box.w), h: Number(box.h) }))) : null;
    }
    const fresh = key === "tiles" ? tilesFor(frames) : [made];
    if (key === "menu") {
      made.at = { wide: frames.wide ?? made.at.wide, tall: frames.tall ?? made.at.tall };
    }
    const at = list.indexOf(old[0]);
    const kept = list.filter((item) => !old.includes(item));
    kept.splice(Math.min(at, kept.length), 0, ...fresh);
    drawing().elements = kept;
    if (!ctx.fits()) {
      drawing().elements = list;
      ctx.fits();
      ctx.say("There's no room to change the menu on this screen. Remove something first.", true);
      return true;
    }
    if (key === "tiles") {
      ctx.tileIcons?.();
    }
    setChosen([fresh[0].id]);
    ctx.changed({ rebuild: true });
    return true;
  }

  function add(key, screen) {
    const entry = [...SHELF, ...SHELF_MORE].find(([name]) => name === key);
    if (!entry) {
      return;
    }
    const made = entry[2](screen);
    if ((key === "menu" || key === "tiles") && swapMenu(key, made)) {
      return;
    }
    const all = Array.isArray(made) ? made : [made];
    if (key === "tiles") {
      const list = drawing().elements;
      list.push(...all);
      if (!ctx.fits()) {
        list.splice(list.length - all.length, all.length);
        ctx.fits();
        ctx.say("There's no room for more on this screen. Remove something first.", true);
        return;
      }
      ctx.tileIcons?.();
      setChosen([all[0].id]);
      ctx.changed({ rebuild: true });
      return;
    }
    if (insert(all[0], true)) {
      ctx.wantSong?.(key);
    }
  }

  function addMenu(screen) {
    const offered = (list) => list.filter(([, , , only]) => !only || only === screen);
    const button = ([key, label]) => {
      const picture = el("span", { class: "part-art", "aria-hidden": "true" });
      picture.innerHTML = `<svg viewBox="0 0 44 26">${SHELF_ART[key]}</svg>`;
      return el("button", { type: "button", class: "part-btn", onclick: () => add(key, screen) }, picture, el("span", {}, label));
    };
    const group = picked()?.kind === "group" && chosen().length === 1 && !picked().repeat ? picked() : null;
    if (inside !== group?.id) {
      inside = null;
    }
    return [
      group ? el("label", { class: "inside" }, toggle(inside === group.id, (on) => {
        inside = on ? group.id : null;
      }, "Add inside the picked group"), `Add inside “${nameOf(group)}”`) : null,
      el("div", { class: "parts-grid" }, offered(SHELF).map(button)),
      el("details", { class: "more" }, el("summary", {}, "More parts"),
        el("div", { class: "add-menu" }, offered(SHELF_MORE).map(([key, label]) => el("button", { type: "button", onclick: () => add(key, screen) }, label)))),
    ];
  }

  function insert(made, open = false) {
    const host = picked()?.kind === "group" && chosen().length === 1 && inside === picked().id ? picked() : null;
    if (host) {
      for (const shapeName of Object.keys(AREAS)) {
        made.at[shapeName] = host.at[shapeName] ? { ...made.at[shapeName], x: 4, y: 4 } : null;
      }
      host.children.push(made);
      expanded.add(host.id);
    } else {
      if (open) {
        for (const [shapeName, area] of Object.entries(AREAS)) {
          const at = made.at[shapeName];
          if (at) {
            at.x = Math.round((area.width - at.w) / 2);
            at.y = Math.round((area.height - at.h) / 2);
            const taken = (x, y) => drawing().elements.some((other) => other.at?.[shapeName]?.x === x && other.at?.[shapeName]?.y === y);
            for (let step = 0; step < 6 && taken(at.x, at.y); step++) {
              at.x = Math.min(area.width - at.w, at.x + PASTE_STEP);
              at.y = Math.min(area.height - Math.max(at.h, 1), at.y + PASTE_STEP);
            }
          }
        }
      }
      drawing().elements.push(made);
    }
    if (!ctx.fits()) {
      (host ? host.children : drawing().elements).pop();
      ctx.fits();
      ctx.say("There's no room for more on this screen. Remove something first.", true);
      return false;
    }
    if (open) {
      expanded.add(made.id);
    }
    setChosen([made.id]);
    ctx.changed({ rebuild: true });
    return true;
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
      if ((key === "w" || key === "h") && isTiles(item)) {
        fitTiles(item, shape());
      }
      ctx.changed({ rebuild: isTiles(item) });
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
    const chain = pathOf(drawing(), item.id) ?? [];
    if (canStretch(item, chain)) {
      const outer = chain.at(-1);
      fields.push(field(outer ? "Fill its group" : "Fill the screen", toggle(Boolean(item.stretch), (on) => {
        if (on) {
          item.stretch = true;
          fitStretched(drawing());
        } else {
          delete item.stretch;
        }
        ctx.changed({ rebuild: true });
      }, outer ? "Fill its group" : "Fill the screen"), outer
        ? "Takes the group's place and size, and keeps them when the group moves or is resized"
        : "Takes the whole screen"));
    }
    if (item.stretch && canStretch(item, chain)) {
      return fields;
    }
    fields.push(stacked("Place", pair("Left", numberInput(at.x, set("x")), "Top", numberInput(at.y, set("y")))));
    fields.push(stacked("Size", pair("Width", numberInput(at.w, set("w"), { least: 1, most: 400 }), "Height", numberInput(at.h, set("h"), { least: 1, most: 400 }))));
    return fields;
  }

  function stretched(place) {
    return Boolean(place.item.stretch) && canStretch(place.item, place.chain);
  }

  function carrier(place, places) {
    let current = place;
    while (current && stretched(current)) {
      const outer = current.chain.at(-1);
      current = outer ? places.find((one) => one.item === outer) : null;
    }
    return current && !locked(current) ? current : null;
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
            const named = document.querySelector('.layers [aria-selected="true"] span');
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
          fields.push(field("Rounded corners", range(item.corner ?? 0, 0, 40, (value) => {
            const was = item.corner ?? 0;
            item.corner = value;
            ctx.changed({ rebuild: (was > 0) !== (value > 0) });
          })));
          if ((item.corner ?? 0) > 0) {
            fields.push(field("Corner colour", chips([["auto", "From what's behind"], ["own", "My own"]], item.behind ? "own" : "auto", (mode) => {
              if (mode === "own") {
                item.behind ??= "@background";
              } else {
                delete item.behind;
              }
              ctx.changed({ rebuild: true });
            }), "The corners are painted over the cover, so they need one plain colour behind it"));
            if (item.behind) {
              fields.push(colourField(item, "behind", "Colour behind the cover"));
            }
          }
          fields.push(field("Reflection", toggle(Boolean(item.reflection), set("reflection")), "Make the box taller than it is wide to show more"));
        }
        fields.push(colourField(item, "placeholder", screen === "home" ? "With no album art" : "Without a cover"));
        break;
      case "symbol": {
        const offered = Object.entries(MARKS).filter(([value]) => value !== "item" || screen === "home");
        const marks = el("select", {}, offered.map(([value, label]) => new Option(label, value)));
        marks.value = item.mark ?? "state";
        marks.addEventListener("change", () => {
          if (marks.value === "item") {
            ctx.tileIcons?.();
          }
          set("mark")(marks.value);
        });
        fields.push(field("Shows", marks, item.mark === "item" ? "Draw each item's icon in the Icons step" : undefined), colourField(item, "color", "Colour"));
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
    if (isTiles(item)) {
      const grid = (key) => (value) => {
        item.repeat[key] = value;
        fitTiles(item, shape());
        ctx.changed({ rebuild: true });
      };
      fields.push(field("Tiles across", numberInput(repeat.columns ?? 1, grid("columns"), { least: 1, most: 6 }), "The tiles stretch to fill the group"));
      fields.push(field("Rows showing", numberInput(tileRows(item, shape()), grid("rows"), { least: 1, most: 4 }), "More items scroll into view"));
      return fields;
    }
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
    const pinned = item.stretch && canStretch(item, pathOf(drawing(), item.id) ?? []);
    const props = (BINDABLE[item.kind] ?? []).filter(([key]) => !pinned || (key !== "x" && key !== "y"));
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

  function copyColourFields(item) {
    if (!insideCopies(item)) {
      return [];
    }
    return (COLOURED[item.kind] ?? []).map(([key, label]) => {
      const list = item.each?.[key] ?? [];
      const put = (next) => {
        item.each ??= {};
        if (next.length < 2) {
          delete item.each[key];
        } else {
          item.each[key] = next;
        }
        ctx.changed({ rebuild: true });
      };
      if (list.length === 0) {
        return field(`${label} for each copy`, el("button", { type: "button", onclick: () => put([item[key], item[key]]) }, "Give each copy its own"));
      }
      const rows = list.map((colour, index) => el("span", { class: "each-colour" },
        choiceColour(colour, (value) => {
          list[index] = value;
          ctx.changed();
        }),
        el("button", { type: "button", title: "Remove this colour", "aria-label": `Remove colour ${index + 1}`, onclick: () => put(list.filter((_, at) => at !== index)) }, "×")));
      return stacked(`${label} for each copy`, el("span", { class: "stack-control" }, ...rows,
        el("button", { type: "button", onclick: () => put([...list, list.at(-1)]) }, "+ Colour")),
      "Copies take these in turn, then start again");
    });
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

  function lineUpRow(count) {
    const row = [el("span", { class: "align-tools", role: "group", "aria-label": "Line up" },
      ALIGN.map(([edge, title, path]) => tool(count === 1 ? `${title} with the ${findElement(drawing(), picked().id)?.parent ? "group" : "screen"}` : title, path, () => lineUp(edge))))];
    if (count >= 3) {
      row.push(el("span", { class: "align-tools", role: "group", "aria-label": "Space evenly" },
        SPREAD.map(([axis, title, path]) => tool(title, path, () => spread(axis)))));
    }
    return stacked(count === 1 ? "Line up" : "Line up and space", el("span", { class: "color-control wrap" }, ...row));
  }

  function selectionPanel(items) {
    return [
      el("h3", { class: "keep" }, `${items.length} parts picked`),
      el("p", { class: "hint" }, "Drag any of them to move them together. Shift-click to add or take away parts."),
      group(lineUpRow(items.length)),
      el("div", { class: "actions" },
        el("button", { type: "button", onclick: () => groupChosen() }, "Group them"),
        el("button", { type: "button", onclick: () => copyChosen() }, "Copy"),
        el("button", { type: "button", onclick: () => duplicateChosen() }, "Duplicate"),
        el("button", { type: "button", onclick: () => removeChosen() }, "Remove"),
        el("button", { type: "button", onclick: () => {
          clear();
          ctx.render();
        } }, "Done")),
    ];
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
    } }, "Done"), el("button", { type: "button", onclick: () => copyChosen() }, "Copy")];
    if (item.kind === "group") {
      actions.push(el("button", { type: "button", onclick: () => ungroup(item) }, "Ungroup"));
    }
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
      el("h3", { class: "keep" }, nameOf(item)),
      group(field("Name", name), ...kindFields(item, screen), ...copyColourFields(item), ...placeFields(item), ...(item.at[shape()] && !(item.stretch && canStretch(item, pathOf(drawing(), item.id) ?? [])) ? [lineUpRow(1)] : [])),
      el("div", { class: "actions" }, ...actions),
    ];
    const live = [...moves, ...colours, showField(item)];
    sections.push(el("details", { class: "moves", open: bound || undefined },
      el("summary", {}, "Moves and changes with the music"), group(...live)));
    return sections;
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

  function settings(screen) {
    const item = picked();
    const items = chosen();
    const shown = items.length > 1 ? selectionPanel(items) : item ? itemPanel(item, screen) : [];
    if (item && settingsShown !== item.id) {
      requestAnimationFrame(() => shown[0]?.scrollIntoView({ block: "nearest" }));
    }
    settingsShown = item?.id ?? null;
    return shown;
  }

  function parts() {
    return [
      el("p", { class: "hint" }, `${ctx.sideNote?.() ?? "Back to front."} Drag a part by its dots to move it, or onto a group to put it inside.`),
      partList(),
      heldParts() ? el("div", { class: "actions" }, el("button", { type: "button", onclick: () => paste() }, "Paste")) : null,
    ];
  }

  function adding(screen) {
    return addMenu(screen);
  }

  return { settings, parts, adding, add, frame, picks: chosen, dragging: () => Boolean(drag), removeChosen, duplicateChosen, groupChosen, overlay, down, deeper, move, up, nudge, key, lineUp, spread, nameOf, colorOf, walk };
}

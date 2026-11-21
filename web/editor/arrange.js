import { AREAS } from "./drawing.js";

const MARGIN = 8;
const GAP = 6;
const THIN = 6;
const EDGE = 2;
const WHOLE = 0.9;
const LOW = 0.78;
const LEAST_SCALE = 0.3;

function flat(item) {
  if (item.kind === "text" || item.kind === "line" || item.kind === "menu") {
    return true;
  }
  if (item.kind === "box") {
    return Number(item.at.wide.h) <= THIN;
  }
  return item.kind === "group" && !item.repeat && (item.children ?? []).length > 0 && item.children.every((child) => child.at?.wide && flat(child));
}

function touching(a, b) {
  return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + Math.max(b.h, 1) && b.y < a.y + Math.max(a.h, 1);
}

function union(boxes) {
  const left = Math.min(...boxes.map((box) => box.x));
  const top = Math.min(...boxes.map((box) => box.y));
  return {
    x: left,
    y: top,
    w: Math.max(...boxes.map((box) => box.x + box.w)) - left,
    h: Math.max(...boxes.map((box) => box.y + box.h)) - top,
  };
}

function numbers(at) {
  return { x: Number(at.x), y: Number(at.y), w: Number(at.w), h: Number(at.h) };
}

function joined(groups) {
  for (let merged = true; merged;) {
    merged = false;
    for (let a = 0; a < groups.length && !merged; a++) {
      for (let b = a + 1; b < groups.length && !merged; b++) {
        if (groups[a].some((one) => groups[b].some((other) => touching(numbers(one.at.wide), numbers(other.at.wide))))) {
          groups[a].push(...groups.splice(b, 1)[0]);
          merged = true;
        }
      }
    }
  }
  return groups;
}

function shared(a, b) {
  const across = Math.min(a.x + a.w, b.x + b.w) - Math.max(a.x, b.x);
  const down = Math.min(a.y + Math.max(a.h, 1), b.y + Math.max(b.h, 1)) - Math.max(a.y, b.y);
  return across > 0 && down > 0 ? across * down : 0;
}

function clustersOf(items) {
  const solid = items.filter((item) => !flat(item) || item.kind === "menu");
  const bodies = joined(solid.map((item) => [item])).map((members) => ({ members, box: union(members.map((item) => numbers(item.at.wide))) }));
  const loose = [];
  for (const item of items.filter((one) => !solid.includes(one))) {
    const box = numbers(item.at.wide);
    const over = bodies.map((body) => body.members.reduce((sum, one) => sum + shared(box, numbers(one.at.wide)), 0));
    const most = Math.max(0, ...over);
    if (most > 0 && most * 2 >= box.w * Math.max(box.h, 1)) {
      bodies[over.indexOf(most)].members.push(item);
    } else {
      loose.push([item]);
    }
  }
  return [...bodies, ...joined(loose).map((members) => ({ members, box: union(members.map((item) => numbers(item.at.wide))) }))];
}

function within(box, frame) {
  const x = Math.max(frame.x, Math.min(box.x, frame.x + frame.w - 1));
  const y = Math.max(frame.y, Math.min(box.y, frame.y + frame.h - 1));
  return { x, y, w: Math.max(1, Math.min(box.w, frame.x + frame.w - x)), h: Math.min(box.h, Math.max(0, frame.y + frame.h - y)) };
}

function sized(cluster, wide, tall) {
  const { members, box } = cluster;
  const edge = box.x <= EDGE || box.x + box.w >= wide.width - EDGE;
  const most = edge ? tall.width : tall.width - 2 * MARGIN;
  const level = members.every(flat);
  const fixed = members.some((item) => item.kind === "group" && item.repeat);
  const across = fixed ? 1 : level ? most / Math.max(1, box.w) : Math.min(edge ? 1.25 : 1, most / Math.max(1, box.w));
  return { ...cluster, edge, level, fixed, across, down: level || fixed ? 1 : across, low: level && box.y + box.h / 2 >= wide.height * LOW };
}

function childBoxes(item, across, down, found) {
  for (const child of item.children ?? []) {
    if (!child.at?.wide || child.at.tall === null) {
      continue;
    }
    const from = numbers(child.at.wide);
    found.set(child, {
      x: Math.round(from.x * across),
      y: Math.round(from.y * down),
      w: Math.max(1, Math.round(from.w * across)),
      h: Math.max(child.kind === "line" ? 0 : 1, Math.round(from.h * down)),
    });
    childBoxes(child, across, down, found);
  }
}

function centred(cluster) {
  const texts = cluster.members.filter((item) => item.kind === "text");
  return texts.length > 0 && texts.every((item) => !item.at.wide.align);
}

export function arrangedBoxes(drawing) {
  const [wide, tall] = [AREAS.wide, AREAS.tall];
  const found = new Map();
  const items = drawing.elements.filter((item) => item.at?.wide && item.at.tall !== null);
  const whole = items.filter((item) => {
    const box = numbers(item.at.wide);
    return box.w * box.h >= wide.width * wide.height * WHOLE;
  });
  for (const item of whole) {
    found.set(item, { x: 0, y: 0, w: tall.width, h: tall.height });
    childBoxes(item, tall.width / wide.width, tall.height / wide.height, found);
  }
  const clusters = clustersOf(items.filter((item) => !whole.includes(item)))
    .map((cluster) => sized(cluster, wide, tall))
    .sort((a, b) => a.box.y - b.box.y || a.box.x - b.box.x);
  const gapAfter = (index) => (index + 1 < clusters.length && !(clusters[index].edge && clusters[index + 1].edge) ? GAP : 0);
  const gaps = clusters.reduce((sum, cluster, index) => sum + gapAfter(index), 0);
  const pad = clusters.some((cluster) => !cluster.edge) ? 2 * MARGIN : 0;
  const firm = clusters.filter((cluster) => cluster.level || cluster.fixed).reduce((sum, cluster) => sum + cluster.box.h, 0);
  const loose = clusters.filter((cluster) => !cluster.level && !cluster.fixed).reduce((sum, cluster) => sum + cluster.box.h * cluster.down, 0);
  const squeeze = loose > 0 ? Math.max(LEAST_SCALE, Math.min(1, (tall.height - firm - gaps - pad) / loose)) : 1;
  for (const cluster of clusters) {
    if (!cluster.level && !cluster.fixed) {
      cluster.across *= squeeze;
      cluster.down *= squeeze;
    }
    cluster.w = cluster.box.w * cluster.across;
    cluster.h = cluster.box.h * cluster.down;
  }
  const place = (cluster, top) => {
    const across = cluster.edge && !cluster.level && !cluster.fixed ? tall.width : cluster.w;
    const frame = { x: Math.round((tall.width - across) / 2), y: Math.round(top), w: Math.round(across), h: Math.round(cluster.h) };
    const left = frame.x + (frame.w - cluster.w) / 2;
    const middle = centred(cluster);
    for (const item of cluster.members) {
      const from = within(numbers(item.at.wide), cluster.box);
      const backing = !cluster.level && item.kind === "box" && from.w * from.h >= cluster.box.w * cluster.box.h * WHOLE;
      const rail = !cluster.level && item.kind === "box" && from.w <= THIN / 2 && from.h >= cluster.box.h * WHOLE;
      const box = backing ? { ...frame } : rail ? { x: frame.x, y: frame.y, w: frame.w, h: Math.max(1, Math.round(from.w)) } : {
        x: Math.round(left + (from.x - cluster.box.x) * cluster.across),
        y: Math.round(top + (from.y - cluster.box.y) * cluster.down),
        w: Math.max(1, Math.round(from.w * cluster.across)),
        h: Math.max(item.kind === "line" ? 0 : 1, Math.round(from.h * cluster.down)),
      };
      const align = item.kind === "text" ? item.at.wide.align ?? (middle ? "center" : null) : null;
      found.set(item, align ? { ...box, align } : box);
      childBoxes(item, cluster.across, cluster.down, found);
    }
  };
  const upper = clusters.filter((cluster) => !cluster.low);
  const lower = clusters.filter((cluster) => cluster.low);
  let top = upper[0]?.edge ? 0 : MARGIN;
  for (const cluster of upper) {
    place(cluster, top);
    top += cluster.h + gapAfter(clusters.indexOf(cluster));
  }
  let bottom = tall.height - MARGIN;
  for (const cluster of [...lower].reverse()) {
    bottom -= cluster.h;
    place(cluster, bottom);
    bottom -= GAP;
  }
  return found;
}

function same(box, at) {
  return Boolean(at) && ["x", "y", "w", "h"].every((key) => Number(at[key]) === box[key]) && (at.align ?? null) === (box.align ?? null);
}

export function uprightMatches(drawing) {
  return [...arrangedBoxes(drawing)].every(([item, box]) => same(box, item.at.tall));
}

export function arrangeUpright(drawing) {
  for (const [item, box] of arrangedBoxes(drawing)) {
    item.at.tall = box;
  }
}

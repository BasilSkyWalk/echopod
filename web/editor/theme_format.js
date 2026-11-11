const MAGIC = [0x45, 0x54, 0x48, 0x4d];
const VERSION = 1;
const HEADER_SIZE = 48;
const CHECKED_FROM = 16;
const NAME_SIZE = 24;
const BOX_SIZE = 11;
const MAX_SIZE = 512;
const HASH_START = 2166136261;
const HASH_PRIME = 16777619;

export const BOX_COLOR_FIELDS = ["fill_top", "fill_bottom", "border", "shine", "ink"];
export const BOX_NUMBER_FIELDS = [
  "border_width",
  "border_edges",
  "shine_height",
  "inset_across",
  "inset_down",
  "radius",
];
export const EDGES = { top: 1, bottom: 2, left: 4, right: 8 };

export class ThemeError extends Error {}

export function checksum(bytes) {
  let value = HASH_START;
  for (const byte of bytes) {
    value = Math.imul(value ^ byte, HASH_PRIME) >>> 0;
  }
  return value;
}

export function rgb565(color) {
  const match = /^#([0-9a-fA-F]{6})$/.exec(color);
  if (!match) {
    throw new ThemeError(`${color} is not a colour like #1a2b3c`);
  }
  const value = parseInt(match[1], 16);
  const red = value >> 16;
  const green = (value >> 8) & 0xff;
  const blue = value & 0xff;
  return ((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue >> 3);
}

export function hexColor(value) {
  const red = value >> 11;
  const green = (value >> 5) & 0x3f;
  const blue = value & 0x1f;
  const wide = [(red << 3) | (red >> 2), (green << 2) | (green >> 4), (blue << 3) | (blue >> 2)];
  return "#" + wide.map((part) => part.toString(16).padStart(2, "0")).join("");
}

export function defaultTheme(schema) {
  return {
    name: "My theme",
    selection: schema.dark.selection,
    colors: { ...schema.dark.colors },
    boxes: Object.fromEntries(
      schema.boxes.map((name) => [name, { ...schema.dark.boxes[name] }]),
    ),
    spacing: Object.fromEntries(schema.spacing.map((entry) => [entry.name, null])),
  };
}

function nameBytes(name) {
  const encoded = new TextEncoder().encode(name);
  if (encoded.length === 0 || encoded.length >= NAME_SIZE) {
    throw new ThemeError(`the name needs 1 to ${NAME_SIZE - 1} bytes`);
  }
  const padded = new Uint8Array(NAME_SIZE);
  padded.set(encoded);
  return padded;
}

function boxBytes(box, schema) {
  const ids = BOX_COLOR_FIELDS.map((field) => {
    const id = schema.colors.indexOf(box[field]);
    if (id < 0) {
      throw new ThemeError(`${box[field]} is not a colour name`);
    }
    return id;
  });
  return [...ids, ...BOX_NUMBER_FIELDS.map((field) => box[field] & 0xff)];
}

function spacingBytes(theme, schema) {
  return schema.spacing.map((entry) => {
    const value = theme.spacing[entry.name];
    if (value === null || value === undefined) {
      return 0;
    }
    if (!Number.isInteger(value) || value < entry.least || value > entry.most) {
      throw new ThemeError(`${entry.name} must be from ${entry.least} to ${entry.most}`);
    }
    return value + 1;
  });
}

export function pack(theme, schema) {
  const selection = schema.boxes.indexOf(theme.selection);
  if (selection < 0) {
    throw new ThemeError(`${theme.selection} is not a box name`);
  }
  const spaces = spacingBytes(theme, schema);
  const size = HEADER_SIZE + 2 * schema.colors.length + BOX_SIZE * schema.boxes.length + spaces.length;
  if (size > MAX_SIZE) {
    throw new ThemeError(`${size} bytes; a theme may be at most ${MAX_SIZE}`);
  }
  const bytes = new Uint8Array(size);
  const view = new DataView(bytes.buffer);
  bytes.set(MAGIC);
  view.setUint16(4, VERSION, true);
  view.setUint16(6, HEADER_SIZE, true);
  view.setUint32(8, size, true);
  view.setUint16(16, schema.colors.length, true);
  view.setUint16(18, schema.boxes.length, true);
  bytes[20] = BOX_SIZE;
  bytes[21] = selection;
  bytes[22] = spaces.length;
  bytes.set(nameBytes(theme.name), 24);
  let offset = HEADER_SIZE;
  for (const name of schema.colors) {
    view.setUint16(offset, rgb565(theme.colors[name]), true);
    offset += 2;
  }
  for (const name of schema.boxes) {
    bytes.set(boxBytes(theme.boxes[name], schema), offset);
    offset += BOX_SIZE;
  }
  bytes.set(spaces, offset);
  view.setUint32(12, checksum(bytes.subarray(CHECKED_FROM)), true);
  return bytes;
}

function checkHeader(bytes, view) {
  const magic = MAGIC.every((byte, index) => bytes[index] === byte);
  if (bytes.length < HEADER_SIZE || !magic) {
    throw new ThemeError("this is not a theme file");
  }
  if (view.getUint16(4, true) !== VERSION) {
    throw new ThemeError("this theme file needs a newer editor");
  }
  if (view.getUint32(8, true) !== bytes.length) {
    throw new ThemeError("the theme file is cut short");
  }
  if (view.getUint32(12, true) !== checksum(bytes.subarray(CHECKED_FROM))) {
    throw new ThemeError("the theme file is damaged");
  }
}

function unpackBox(bytes, start, length, box, schema) {
  const known = [...BOX_COLOR_FIELDS, ...BOX_NUMBER_FIELDS];
  const taken = { ...box };
  for (let index = 0; index < Math.min(length, known.length); index++) {
    const value = bytes[start + index];
    const field = known[index];
    taken[field] = index < BOX_COLOR_FIELDS.length ? schema.colors[value] : value;
  }
  return BOX_COLOR_FIELDS.every((field) => taken[field] !== undefined) ? taken : box;
}

export function unpack(bytes, schema) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  checkHeader(bytes, view);
  const theme = defaultTheme(schema);
  const header = view.getUint16(6, true);
  const colors = view.getUint16(16, true);
  const boxes = view.getUint16(18, true);
  const boxSize = bytes[20];
  const boxesAt = header + 2 * colors;
  const spacingAt = boxesAt + boxes * boxSize;
  if (spacingAt + bytes[22] > bytes.length) {
    throw new ThemeError("the theme file is cut short");
  }
  const text = new TextDecoder().decode(bytes.subarray(24, 24 + NAME_SIZE));
  theme.name = text.replace(/\0.*$/s, "");
  schema.colors.slice(0, colors).forEach((name, index) => {
    theme.colors[name] = hexColor(view.getUint16(header + 2 * index, true));
  });
  schema.boxes.slice(0, boxes).forEach((name, index) => {
    theme.boxes[name] = unpackBox(bytes, boxesAt + index * boxSize, boxSize, theme.boxes[name], schema);
  });
  schema.spacing.slice(0, bytes[22]).forEach((entry, index) => {
    const stored = bytes[spacingAt + index];
    theme.spacing[entry.name] = stored === 0 ? null : stored - 1;
  });
  theme.selection = schema.boxes[bytes[21]] ?? schema.dark.selection;
  return theme;
}

function tomlBox(box) {
  const lines = [];
  const fill = box.fill_top === box.fill_bottom
    ? `"${box.fill_top}"`
    : `["${box.fill_top}", "${box.fill_bottom}"]`;
  lines.push(`fill = ${fill}`);
  if (box.border_width > 0) {
    const edges = Object.keys(EDGES).filter((edge) => box.border_edges & EDGES[edge]);
    lines.push(`border = "${box.border}"`, `border_width = ${box.border_width}`);
    lines.push(edges.length === 4 ? 'border_edges = "all"' : `border_edges = ${JSON.stringify(edges)}`);
  }
  if (box.shine_height > 0) {
    lines.push(`shine = "${box.shine}"`, `shine_height = ${box.shine_height}`);
  }
  lines.push(`ink = "${box.ink}"`);
  if (box.inset_across > 0 || box.inset_down > 0) {
    lines.push(`inset = [${box.inset_across}, ${box.inset_down}]`);
  }
  if (box.radius > 0) {
    lines.push(`radius = ${box.radius}`);
  }
  return lines.join("\n");
}

export function toToml(theme, schema) {
  const parts = [`name = ${JSON.stringify(theme.name)}`, `selection = "${theme.selection}"`, ""];
  const spacing = schema.spacing.filter((entry) => theme.spacing[entry.name] !== null);
  if (spacing.length > 0) {
    parts.push("[spacing]", ...spacing.map((entry) => `${entry.name} = ${theme.spacing[entry.name]}`), "");
  }
  parts.push("[colors]", ...schema.colors.map((name) => `${name} = "${theme.colors[name]}"`), "");
  for (const name of schema.boxes) {
    parts.push(`[boxes.${name}]`, tomlBox(theme.boxes[name]), "");
  }
  return parts.join("\n");
}

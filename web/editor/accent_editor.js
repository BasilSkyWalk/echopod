import { ACCENT_SHADES, MOST_ACCENTS } from "./theme_format.js";

const SHADE_LABELS = {
  selection_background: "Fill",
  selection_top: "Top",
  selection_shine: "Shine",
};

export function newAccent(theme) {
  return {
    name: `Accent ${(theme.accents?.length ?? 0) + 1}`,
    selection_background: theme.colors.selection_background,
    selection_top: theme.colors.selection_top,
    selection_shine: theme.colors.selection_shine,
  };
}

export function shownAccent(theme, wanted) {
  const count = theme.accents?.length ?? 0;
  return wanted >= 0 && wanted < count ? wanted : 0;
}

function accentRow(theme, index, context, rebuild) {
  const accent = theme.accents[index];
  const row = document.createElement("div");
  row.className = "row accent";
  const shown = document.createElement("input");
  Object.assign(shown, { type: "radio", name: "accent-shown", checked: context.shown() === index });
  shown.title = "Show this accent in the preview";
  shown.addEventListener("change", () => context.show(index));
  const name = document.createElement("input");
  Object.assign(name, { type: "text", value: accent.name, maxLength: 12, size: 10 });
  name.setAttribute("aria-label", "Accent name");
  name.addEventListener("input", () => {
    accent.name = name.value;
    context.changed();
  });
  row.append(shown, name);
  for (const key of ACCENT_SHADES) {
    const color = document.createElement("input");
    Object.assign(color, { type: "color", value: accent[key], title: SHADE_LABELS[key] });
    color.setAttribute("aria-label", `${accent.name} ${SHADE_LABELS[key]}`);
    color.addEventListener("input", () => {
      accent[key] = color.value;
      context.changed();
    });
    row.append(color);
  }
  const remove = document.createElement("button");
  Object.assign(remove, { type: "button", textContent: "Remove" });
  remove.addEventListener("click", () => {
    theme.accents.splice(index, 1);
    rebuild();
    context.changed();
  });
  row.append(remove);
  return row;
}

export function buildAccents(host, theme, context) {
  theme.accents ??= [];
  const rebuild = () => {
    host.replaceChildren(...theme.accents.map((_, index) => accentRow(theme, index, context, rebuild)));
    const add = document.createElement("button");
    Object.assign(add, { type: "button", textContent: "Add accent" });
    add.disabled = theme.accents.length >= MOST_ACCENTS;
    add.addEventListener("click", () => {
      theme.accents.push(newAccent(theme));
      rebuild();
      context.changed();
    });
    host.append(add);
  };
  rebuild();
}

export const REGISTERS = 128;
export const STRING_BASE = 64;
const MOST_STRINGS = 64;
const IMMEDIATE_BIT = 0x8000;
const IMMEDIATE_LOW = -(1 << 14);
const IMMEDIATE_HIGH = (1 << 14) - 1;
const FONTS = { title: 0, body: 1, small: 2 };
const ALIGNS = { left: 0, center: 1, right: 2 };
const ICONS = { none: 0, play: 1, pause: 2, next: 3, previous: 4 };
const TRUTH = { true: 1, false: 0 };
const TOKEN = /(\s+|\/\/[^\n]*)|(#[0-9a-fA-F]{6}\b)|(0x[0-9a-fA-F]+|\d+)|([A-Za-z_]\w*)|("(?:[^"\\\n]|\\.)*")|(>>>|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^<>!?:(){},=;])/y;
const KINDS = ["space", "color", "number", "name", "string", "op"];
const BINARY = [
  ["||"], ["&&"], ["|"], ["^"], ["&"], ["==", "!="], ["<", "<=", ">", ">="],
  ["<<", ">>", ">>>"], ["+", "-"], ["*", "/", "%"],
];
const BINARY_OPS = {
  "+": "add", "-": "sub", "*": "mul", "/": "div", "%": "mod", "&": "and", "|": "or", "^": "xor",
  "<<": "shl", ">>": "shr", ">>>": "ushr", "<": "lt", "<=": "le", "==": "eq", "!=": "ne",
};
const SWAPPED = { ">": "lt", ">=": "le" };
const INT_MIN = -2147483648;

export class SceneError extends Error {}

function signed(value) {
  return Number(BigInt.asIntN(32, BigInt(value)));
}

function divided(first, second) {
  if (second === 0) {
    return 0;
  }
  if (first === INT_MIN && second === -1) {
    return first;
  }
  return Math.trunc(first / second);
}

function remainder(first, second) {
  if (second === 0 || (first === INT_MIN && second === -1)) {
    return 0;
  }
  return first - divided(first, second) * second;
}

const FOLD = {
  add: (a, b) => (a + b) | 0,
  sub: (a, b) => (a - b) | 0,
  mul: (a, b) => Math.imul(a, b),
  div: divided,
  mod: (a, b) => remainder(a, b) | 0,
  and: (a, b) => a & b,
  or: (a, b) => a | b,
  xor: (a, b) => a ^ b,
  shl: (a, b) => a << (b & 31),
  shr: (a, b) => a >> (b & 31),
  ushr: (a, b) => (a >>> (b & 31)) | 0,
  lt: (a, b) => Number(a < b),
  le: (a, b) => Number(a <= b),
  eq: (a, b) => Number(a === b),
  ne: (a, b) => Number(a !== b),
  min: Math.min,
  max: Math.max,
};
const FOLD_ONE = {
  neg: (a) => (-a) | 0,
  not: (a) => Number(a === 0),
  abs: (a) => Math.abs(a) | 0,
};

function rgb565(text) {
  const value = parseInt(text.slice(1), 16);
  const [red, green, blue] = [value >> 16, (value >> 8) & 0xff, value & 0xff];
  return ((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue >> 3);
}

function tokens(source) {
  const found = [];
  let line = 1;
  TOKEN.lastIndex = 0;
  while (TOKEN.lastIndex < source.length) {
    const at = TOKEN.lastIndex;
    const match = TOKEN.exec(source);
    if (match === null) {
      throw new SceneError(`line ${line}: can't read ${JSON.stringify(source.slice(at, at + 12))}`);
    }
    const group = match.findIndex((part, index) => index > 0 && part !== undefined);
    const kind = KINDS[group - 1];
    if (kind !== "space") {
      found.push([kind, match[group], line]);
    }
    line += (match[0].match(/\n/g) ?? []).length;
  }
  found.push(["end", "", line]);
  return found;
}

function unescaped(text) {
  return text.slice(1, -1).replace(/\\(.)/gs, (_, character) => ({ n: "\n", t: "\t" })[character] ?? character);
}

class Parser {
  constructor(source) {
    this.tokens = tokens(source);
    this.at = 0;
  }

  peek(offset = 0) {
    return this.tokens[Math.min(this.at + offset, this.tokens.length - 1)];
  }

  line() {
    return this.peek()[2];
  }

  take(text, kind) {
    const token = this.peek();
    if ((text !== undefined && token[1] !== text) || (kind !== undefined && token[0] !== kind)) {
      throw new SceneError(`line ${token[2]}: expected ${text ?? kind}, found ${JSON.stringify(token[1] || "the end")}`);
    }
    this.at += 1;
    return token;
  }

  accept(text) {
    if (this.peek()[1] === text && (this.peek()[0] === "op" || this.peek()[0] === "name")) {
      this.at += 1;
      return true;
    }
    return false;
  }

  program() {
    const statements = [];
    while (this.peek()[0] !== "end") {
      statements.push(this.statement());
    }
    return statements;
  }

  block() {
    this.take("{");
    const statements = [];
    while (!this.accept("}")) {
      if (this.peek()[0] === "end") {
        throw new SceneError(`line ${this.line()}: a block is missing its }`);
      }
      statements.push(this.statement());
    }
    return statements;
  }

  statement() {
    let line = this.line();
    while (this.accept(";")) {
      line = this.line();
    }
    const [kind, text] = this.peek();
    if (kind === "name" && (text === "let" || text === "const")) {
      this.at += 1;
      const name = this.take(undefined, "name")[1];
      this.take("=");
      return [text, name, this.expression(), line];
    }
    if (kind === "name" && text === "if") {
      this.at += 1;
      const condition = this.expression();
      const then = this.block();
      let otherwise = [];
      if (this.accept("else")) {
        otherwise = this.peek()[1] === "if" ? [this.statement()] : this.block();
      }
      return ["if", condition, then, otherwise, line];
    }
    if (kind === "name" && text === "repeat") {
      this.at += 1;
      const name = this.take(undefined, "name")[1];
      this.take("from");
      const first = this.expression();
      this.take("to");
      const last = this.expression();
      return ["repeat", name, first, last, this.block(), line];
    }
    if (kind === "name" && this.peek(1)[1] === "=") {
      this.at += 2;
      return ["assign", text, this.expression(), line];
    }
    if (kind === "name" && this.peek(1)[1] === "(") {
      return ["call", this.call(), line];
    }
    throw new SceneError(`line ${line}: ${JSON.stringify(text || "the end")} doesn't start a statement`);
  }

  call() {
    const name = this.take(undefined, "name")[1];
    this.take("(");
    const args = [];
    if (!this.accept(")")) {
      args.push(this.expression());
      while (this.accept(",")) {
        args.push(this.expression());
      }
      this.take(")");
    }
    return ["call", name, args];
  }

  expression() {
    const condition = this.binary(0);
    if (this.accept("?")) {
      const first = this.expression();
      this.take(":");
      return ["select", condition, first, this.expression()];
    }
    return condition;
  }

  binary(level) {
    if (level === BINARY.length) {
      return this.unary();
    }
    let left = this.binary(level + 1);
    while (this.peek()[0] === "op" && BINARY[level].includes(this.peek()[1])) {
      const operator = this.take()[1];
      left = ["binary", operator, left, this.binary(level + 1)];
    }
    return left;
  }

  unary() {
    if (this.accept("-")) {
      return ["unary", "neg", this.unary()];
    }
    if (this.accept("!")) {
      return ["unary", "not", this.unary()];
    }
    return this.primary();
  }

  primary() {
    const [kind, text, line] = this.peek();
    if (kind === "number") {
      this.at += 1;
      return ["number", signed(text)];
    }
    if (kind === "color") {
      this.at += 1;
      return ["number", rgb565(text)];
    }
    if (kind === "string") {
      this.at += 1;
      return ["string", unescaped(text)];
    }
    if (kind === "name" && this.peek(1)[1] === "(") {
      return this.call();
    }
    if (kind === "name") {
      this.at += 1;
      return ["name", text, line];
    }
    if (this.accept("(")) {
      const inner = this.expression();
      this.take(")");
      return inner;
    }
    throw new SceneError(`line ${line}: ${JSON.stringify(text || "the end")} isn't a value`);
  }
}

class Compiler {
  constructor(names) {
    this.ops = names.ops;
    this.code = [];
    this.registers = new Map(names.inputs.map((name, index) => [name, index]));
    this.constants = new Map(Object.entries({ ...FONTS, ...ALIGNS, ...ICONS, ...TRUTH }));
    names.sources.forEach((name, index) => this.constants.set(name, index));
    this.nextRegister = names.inputs.length;
    this.temporary = this.nextRegister;
    this.firstVariable = names.inputs.length;
    this.strings = [];
  }

  emit(op, operands) {
    const entry = this.ops[op];
    if (operands.length !== entry.count) {
      throw new SceneError(`${op} takes ${entry.count} values, not ${operands.length}`);
    }
    this.code.push(entry.code);
    for (const value of operands) {
      this.code.push(value & 0xff, (value >> 8) & 0xff);
    }
  }

  fresh() {
    if (this.temporary >= REGISTERS) {
      throw new SceneError(`the scene needs more than ${REGISTERS} values at once`);
    }
    this.temporary += 1;
    return this.temporary - 1;
  }

  variable(name, line) {
    if (this.registers.has(name) || this.constants.has(name)) {
      throw new SceneError(`line ${line}: ${JSON.stringify(name)} is already named`);
    }
    if (this.nextRegister >= REGISTERS) {
      throw new SceneError(`the scene names more than ${REGISTERS} values`);
    }
    this.registers.set(name, this.nextRegister);
    this.nextRegister += 1;
    this.temporary = Math.max(this.temporary, this.nextRegister);
    return this.registers.get(name);
  }

  string(text) {
    if (!this.strings.includes(text)) {
      if (this.strings.length >= MOST_STRINGS) {
        throw new SceneError(`a scene holds at most ${MOST_STRINGS} pieces of text`);
      }
      this.strings.push(text);
    }
    return STRING_BASE + this.strings.indexOf(text);
  }

  constant(node) {
    const kind = node[0];
    if (kind === "number") {
      return node[1];
    }
    if (kind === "string") {
      return this.string(node[1]);
    }
    if (kind === "name") {
      return this.constants.has(node[1]) ? this.constants.get(node[1]) : null;
    }
    if (kind === "unary") {
      const value = this.constant(node[2]);
      return value === null ? null : FOLD_ONE[node[1]](value);
    }
    if (kind === "binary") {
      const first = this.constant(node[2]);
      const second = this.constant(node[3]);
      return first === null || second === null ? null : this.foldBinary(node[1], first, second);
    }
    if (kind === "select") {
      const condition = this.constant(node[1]);
      return condition === null ? null : this.constant(condition ? node[2] : node[3]);
    }
    if (kind === "call" && ["min", "max", "abs"].includes(node[1]) && [1, 2].includes(node[2].length)) {
      const values = node[2].map((argument) => this.constant(argument));
      if (values.includes(null)) {
        return null;
      }
      return values.length === 2 ? FOLD[node[1]](...values) : FOLD_ONE.abs(values[0]);
    }
    return null;
  }

  foldBinary(operator, first, second) {
    if (operator === "&&") {
      return Number(first !== 0 && second !== 0);
    }
    if (operator === "||") {
      return Number(first !== 0 || second !== 0);
    }
    if (operator in SWAPPED) {
      return FOLD[SWAPPED[operator]](second, first);
    }
    return FOLD[BINARY_OPS[operator]](first, second);
  }

  immediate(value, target) {
    if (target === undefined && value >= IMMEDIATE_LOW && value <= IMMEDIATE_HIGH) {
      return IMMEDIATE_BIT | (value & 0x7fff);
    }
    const place = target === undefined ? this.fresh() : target;
    if (value >= IMMEDIATE_LOW && value <= IMMEDIATE_HIGH) {
      this.emit("set", [place, IMMEDIATE_BIT | (value & 0x7fff)]);
    } else {
      const bits = value >>> 0;
      this.emit("const", [place, bits & 0xffff, bits >>> 16]);
    }
    return place;
  }

  value(node, target) {
    const folded = this.constant(node);
    if (folded !== null) {
      return this.immediate(folded, target);
    }
    const kind = node[0];
    if (kind === "name") {
      if (!this.registers.has(node[1])) {
        throw new SceneError(`line ${node[2]}: ${JSON.stringify(node[1])} isn't named`);
      }
      const place = this.registers.get(node[1]);
      if (target !== undefined) {
        this.emit("set", [target, place]);
        return target;
      }
      return place;
    }
    if (kind === "unary") {
      const operand = this.value(node[2]);
      const place = target === undefined ? this.fresh() : target;
      this.emit(node[1], [place, operand]);
      return place;
    }
    if (kind === "binary") {
      return this.binary(node, target);
    }
    if (kind === "select") {
      const operands = node.slice(1).map((part) => this.value(part));
      const place = target === undefined ? this.fresh() : target;
      this.emit("select", [place, ...operands]);
      return place;
    }
    if (kind === "call") {
      return this.function(node, target);
    }
    throw new SceneError("can't compute this");
  }

  truth(node) {
    const operand = this.value(node);
    const place = this.fresh();
    this.emit("ne", [place, operand, IMMEDIATE_BIT]);
    return place;
  }

  binary(node, target) {
    const operator = node[1];
    if (operator === "&&" || operator === "||") {
      const first = this.truth(node[2]);
      const second = this.truth(node[3]);
      const place = target === undefined ? this.fresh() : target;
      this.emit(operator === "&&" ? "and" : "or", [place, first, second]);
      return place;
    }
    let first;
    let second;
    let op;
    if (operator in SWAPPED) {
      first = this.value(node[3]);
      second = this.value(node[2]);
      op = SWAPPED[operator];
    } else {
      first = this.value(node[2]);
      second = this.value(node[3]);
      op = BINARY_OPS[operator];
    }
    const place = target === undefined ? this.fresh() : target;
    this.emit(op, [place, first, second]);
    return place;
  }

  function(node, target) {
    const [, name, args] = node;
    const entry = this.ops[name];
    if (!entry || entry.kind !== "VALUE" || ["set", "const", "select"].includes(name)) {
      throw new SceneError(`${name}() isn't a function that gives a value`);
    }
    if (args.length !== entry.count - 1) {
      throw new SceneError(`${name}() takes ${entry.count - 1} values`);
    }
    const operands = args.map((argument) => this.value(argument));
    const place = target === undefined ? this.fresh() : target;
    this.emit(name, [place, ...operands]);
    return place;
  }

  label() {
    return this.code.length;
  }

  patch(at, target) {
    this.code[at] = target & 0xff;
    this.code[at + 1] = (target >> 8) & 0xff;
  }

  statements(statements) {
    for (const statement of statements) {
      const saved = this.temporary;
      this.statement(statement);
      this.temporary = Math.max(saved, this.nextRegister);
    }
  }

  scoped(work) {
    const registers = new Map(this.registers);
    const constants = new Map(this.constants);
    const [nextRegister, temporary] = [this.nextRegister, this.temporary];
    work();
    this.registers = registers;
    this.constants = constants;
    this.nextRegister = nextRegister;
    this.temporary = temporary;
  }

  block(statements) {
    this.scoped(() => this.statements(statements));
  }

  statement(statement) {
    const kind = statement[0];
    if (kind === "const") {
      const value = this.constant(statement[2]);
      if (value === null) {
        throw new SceneError(`line ${statement[3]}: ${statement[1]} must be known before playing`);
      }
      if (this.registers.has(statement[1]) || this.constants.has(statement[1])) {
        throw new SceneError(`line ${statement[3]}: ${JSON.stringify(statement[1])} is already named`);
      }
      this.constants.set(statement[1], value);
    } else if (kind === "let") {
      this.value(statement[2], this.variable(statement[1], statement[3]));
    } else if (kind === "assign") {
      const place = this.registers.get(statement[1]);
      if (place === undefined || place < this.firstVariable) {
        throw new SceneError(`line ${statement[3]}: ${JSON.stringify(statement[1])} isn't a value made with let`);
      }
      this.value(statement[2], place);
    } else if (kind === "if") {
      this.branch(statement);
    } else if (kind === "repeat") {
      this.scoped(() => this.counted(statement));
    } else if (kind === "call") {
      this.draw(statement[1], statement[2]);
    }
  }

  branch(statement) {
    const [, condition, then, otherwise] = statement;
    const known = this.constant(condition);
    if (known !== null) {
      this.block(known ? then : otherwise);
      return;
    }
    const operand = this.value(condition);
    this.emit("jump_zero", [operand, 0]);
    const skip = this.label() - 2;
    this.block(then);
    if (otherwise.length > 0) {
      this.emit("jump", [0]);
      const leave = this.label() - 2;
      this.patch(skip, this.label());
      this.block(otherwise);
      this.patch(leave, this.label());
    } else {
      this.patch(skip, this.label());
    }
  }

  counted(statement) {
    const [, name, first, last, body, line] = statement;
    const counter = this.variable(name, line);
    this.value(first, counter);
    const limit = this.value(last, this.variable(` limit ${this.nextRegister}`, line));
    const start = this.label();
    const test = this.fresh();
    this.emit("lt", [test, counter, limit]);
    this.emit("jump_zero", [test, 0]);
    const leave = this.label() - 2;
    this.temporary -= 1;
    this.block(body);
    this.emit("add", [counter, counter, IMMEDIATE_BIT | 1]);
    this.emit("jump", [start]);
    this.patch(leave, this.label());
  }

  draw(node, line) {
    const [, name, args] = node;
    const entry = this.ops[name];
    if (!entry || entry.kind !== "DRAW") {
      throw new SceneError(`line ${line}: ${name}() isn't something a scene can draw`);
    }
    if (args.length !== entry.count) {
      throw new SceneError(`line ${line}: ${name}() takes ${entry.count} values`);
    }
    this.emit(name, args.map((argument) => this.value(argument)));
  }
}

export function compileScene(source, names) {
  const compiler = new Compiler(names);
  compiler.statements(new Parser(source).program());
  compiler.emit("end", []);
  const encoder = new TextEncoder();
  const strings = compiler.strings.flatMap((text) => [...encoder.encode(text), 0]);
  return { code: Uint8Array.from(compiler.code), strings: Uint8Array.from(strings) };
}

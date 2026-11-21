"""Compile a Now Playing scene script into the bytecode the player's scene interpreter runs."""

import re
import struct
from pathlib import Path

REGISTERS = 128
STRING_BASE = 64
MOST_STRINGS = 64
IMMEDIATE_BIT = 0x8000
IMMEDIATE_LOW = -(1 << 14)
IMMEDIATE_HIGH = (1 << 14) - 1
INT_MASK = 0xFFFFFFFF
FONTS = {"title": 0, "body": 1, "small": 2}
ALIGNS = {"left": 0, "center": 1, "right": 2}
ICONS = {"none": 0, "play": 1, "pause": 2, "next": 3, "previous": 4}
TRUTH = {"true": 1, "false": 0}
ESCAPES = {"n": "\n", "t": "\t"}
OP_ENTRY = re.compile(r"^SCENE_OP\(\w+,\s*(\w+),\s*(\w+),\s*(\d+),\s*(\d+)\)\s*$")
NAMED_ENTRY = re.compile(r"^SCENE_(?:INPUT|SOURCE)\(\w+,\s*(\w+)\)\s*$")
TOKEN = re.compile(r"""
    (?P<space>\s+|//[^\n]*)
  | (?P<color>\#[0-9a-fA-F]{6}\b)
  | (?P<number>0x[0-9a-fA-F]+|\d+)
  | (?P<name>[A-Za-z_]\w*)
  | (?P<string>"(?:[^"\\\n]|\\.)*")
  | (?P<op>>>>|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^<>!?:(){},=;])
""", re.VERBOSE)
BINARY = [
    ("||",), ("&&",), ("|",), ("^",), ("&",), ("==", "!="), ("<", "<=", ">", ">="),
    ("<<", ">>", ">>>"), ("+", "-"), ("*", "/", "%"),
]
BINARY_OPS = {"+": "add", "-": "sub", "*": "mul", "/": "div", "%": "mod", "&": "and",
              "|": "or", "^": "xor", "<<": "shl", ">>": "shr", ">>>": "ushr", "<": "lt",
              "<=": "le", "==": "eq", "!=": "ne"}
SWAPPED = {">": "lt", ">=": "le"}


class SceneError(Exception):
    pass


def signed(value):
    value &= INT_MASK
    return value - (1 << 32) if value & 0x80000000 else value


def divided(first, second):
    if second == 0:
        return 0
    if first == -(1 << 31) and second == -1:
        return first
    quotient = abs(first) // abs(second)
    return quotient if (first < 0) == (second < 0) else -quotient


def remainder(first, second):
    if second == 0:
        return 0
    if first == -(1 << 31) and second == -1:
        return 0
    return first - divided(first, second) * second


FOLD = {
    "add": lambda a, b: signed(a + b),
    "sub": lambda a, b: signed(a - b),
    "mul": lambda a, b: signed(a * b),
    "div": divided,
    "mod": remainder,
    "and": lambda a, b: signed(a & b),
    "or": lambda a, b: signed(a | b),
    "xor": lambda a, b: signed(a ^ b),
    "shl": lambda a, b: signed((a & INT_MASK) << (b & 31)),
    "shr": lambda a, b: a >> (b & 31),
    "ushr": lambda a, b: signed((a & INT_MASK) >> (b & 31)),
    "lt": lambda a, b: int(a < b),
    "le": lambda a, b: int(a <= b),
    "eq": lambda a, b: int(a == b),
    "ne": lambda a, b: int(a != b),
    "min": min,
    "max": max,
}
FOLD_ONE = {"neg": lambda a: signed(-a), "not": lambda a: int(a == 0), "abs": lambda a: signed(abs(a))}


def definitions(folder):
    folder = Path(folder)
    ops = {}
    for index, line in enumerate(_entries(folder / "scene_ops.def", OP_ENTRY)):
        name, kind, count, raw = line
        ops[name] = {"code": index, "kind": kind, "count": int(count), "raw": int(raw)}
    inputs = [match[0] for match in _entries(folder / "scene_inputs.def", NAMED_ENTRY)]
    sources = [match[0] for match in _entries(folder / "scene_sources.def", NAMED_ENTRY)]
    return {"ops": ops, "inputs": inputs, "sources": sources}


def _entries(path, pattern):
    found = []
    for line in path.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line.strip())
        if match:
            found.append(match.groups())
    if not found:
        raise SceneError(f"{path} lists nothing")
    return found


def rgb565(text):
    value = int(text[1:], 16)
    red, green, blue = value >> 16, value >> 8 & 0xFF, value & 0xFF
    return (red & 0xF8) << 8 | (green & 0xFC) << 3 | blue >> 3


def tokens(source):
    found, at, line = [], 0, 1
    while at < len(source):
        match = TOKEN.match(source, at)
        if match is None:
            raise SceneError(f"line {line}: can't read {source[at:at + 12]!r}")
        kind = match.lastgroup
        text = match.group(kind)
        if kind != "space":
            found.append((kind, text, line))
        line += text.count("\n")
        at = match.end()
    found.append(("end", "", line))
    return found


class Parser:
    def __init__(self, source):
        self.tokens = tokens(source)
        self.at = 0

    def peek(self, offset=0):
        return self.tokens[min(self.at + offset, len(self.tokens) - 1)]

    def line(self):
        return self.peek()[2]

    def take(self, text=None, kind=None):
        token = self.peek()
        if (text is not None and token[1] != text) or (kind is not None and token[0] != kind):
            wanted = text or kind
            raise SceneError(f"line {token[2]}: expected {wanted}, found {token[1] or 'the end'!r}")
        self.at += 1
        return token

    def accept(self, text):
        if self.peek()[1] == text and self.peek()[0] in ("op", "name"):
            self.at += 1
            return True
        return False

    def program(self):
        statements = []
        while self.peek()[0] != "end":
            statements.append(self.statement())
        return statements

    def block(self):
        self.take("{")
        statements = []
        while not self.accept("}"):
            if self.peek()[0] == "end":
                raise SceneError(f"line {self.line()}: a block is missing its }}")
            statements.append(self.statement())
        return statements

    def statement(self):
        line = self.line()
        while self.accept(";"):
            line = self.line()
        kind, text, _ = self.peek()
        if kind == "name" and text in ("let", "const"):
            self.at += 1
            name = self.take(kind="name")[1]
            self.take("=")
            return (text, name, self.expression(), line)
        if kind == "name" and text == "if":
            self.at += 1
            condition = self.expression()
            then = self.block()
            otherwise = []
            if self.accept("else"):
                otherwise = [self.statement()] if self.peek()[1] == "if" else self.block()
            return ("if", condition, then, otherwise, line)
        if kind == "name" and text == "repeat":
            self.at += 1
            name = self.take(kind="name")[1]
            self.take("from")
            first = self.expression()
            self.take("to")
            last = self.expression()
            return ("repeat", name, first, last, self.block(), line)
        if kind == "name" and self.peek(1)[1] == "=":
            self.at += 2
            return ("assign", text, self.expression(), line)
        if kind == "name" and self.peek(1)[1] == "(":
            return ("call", self.call(), line)
        raise SceneError(f"line {line}: {text or 'the end'!r} doesn't start a statement")

    def call(self):
        name = self.take(kind="name")[1]
        self.take("(")
        arguments = []
        if not self.accept(")"):
            arguments.append(self.expression())
            while self.accept(","):
                arguments.append(self.expression())
            self.take(")")
        return ("call", name, arguments)

    def expression(self):
        condition = self.binary(0)
        if self.accept("?"):
            first = self.expression()
            self.take(":")
            second = self.expression()
            return ("select", condition, first, second)
        return condition

    def binary(self, level):
        if level == len(BINARY):
            return self.unary()
        left = self.binary(level + 1)
        while self.peek()[0] == "op" and self.peek()[1] in BINARY[level]:
            operator = self.take()[1]
            left = ("binary", operator, left, self.binary(level + 1))
        return left

    def unary(self):
        if self.accept("-"):
            return ("unary", "neg", self.unary())
        if self.accept("!"):
            return ("unary", "not", self.unary())
        return self.primary()

    def primary(self):
        kind, text, line = self.peek()
        if kind == "number":
            self.at += 1
            return ("number", signed(int(text, 16) if text.startswith("0x") else int(text)))
        if kind == "color":
            self.at += 1
            return ("number", rgb565(text))
        if kind == "string":
            self.at += 1
            return ("string", re.sub(r"\\(.)", lambda match: ESCAPES.get(match.group(1), match.group(1)),
                                     text[1:-1], flags=re.S))
        if kind == "name" and self.peek(1)[1] == "(":
            return self.call()
        if kind == "name":
            self.at += 1
            return ("name", text, line)
        if self.accept("("):
            inner = self.expression()
            self.take(")")
            return inner
        raise SceneError(f"line {line}: {text or 'the end'!r} isn't a value")


class Compiler:
    def __init__(self, names):
        self.ops = names["ops"]
        self.code = bytearray()
        self.registers = {name: index for index, name in enumerate(names["inputs"])}
        self.constants = dict(FONTS)
        self.constants.update(ALIGNS)
        self.constants.update(ICONS)
        self.constants.update(TRUTH)
        self.constants.update({name: index for index, name in enumerate(names["sources"])})
        self.next_register = len(names["inputs"])
        self.temporary = self.next_register
        self.strings = []
        self.first_variable = len(names["inputs"])

    def emit(self, op, operands):
        entry = self.ops[op]
        if len(operands) != entry["count"]:
            raise SceneError(f"{op} takes {entry['count']} values, not {len(operands)}")
        self.code.append(entry["code"])
        for value in operands:
            self.code += struct.pack("<H", value & 0xFFFF)

    def fresh(self):
        if self.temporary >= REGISTERS:
            raise SceneError(f"the scene needs more than {REGISTERS} values at once")
        self.temporary += 1
        return self.temporary - 1

    def variable(self, name, line):
        if name in self.registers or name in self.constants:
            raise SceneError(f"line {line}: {name!r} is already named")
        if self.next_register >= REGISTERS:
            raise SceneError(f"the scene names more than {REGISTERS} values")
        self.registers[name] = self.next_register
        self.next_register += 1
        self.temporary = max(self.temporary, self.next_register)
        return self.registers[name]

    def string(self, text):
        if text not in self.strings:
            if len(self.strings) >= MOST_STRINGS:
                raise SceneError(f"a scene holds at most {MOST_STRINGS} pieces of text")
            self.strings.append(text)
        return STRING_BASE + self.strings.index(text)

    def constant(self, node):
        kind = node[0]
        if kind == "number":
            return node[1]
        if kind == "string":
            return self.string(node[1])
        if kind == "name":
            return self.constants.get(node[1])
        if kind == "unary":
            value = self.constant(node[2])
            return None if value is None else FOLD_ONE[node[1]](value)
        if kind == "binary":
            first, second = self.constant(node[2]), self.constant(node[3])
            if first is None or second is None:
                return None
            return self.fold_binary(node[1], first, second)
        if kind == "select":
            condition = self.constant(node[1])
            if condition is None:
                return None
            return self.constant(node[2] if condition else node[3])
        if kind == "call" and node[1] in ("min", "max", "abs") and len(node[2]) in (1, 2):
            values = [self.constant(argument) for argument in node[2]]
            if None in values:
                return None
            return FOLD[node[1]](*values) if len(values) == 2 else FOLD_ONE["abs"](values[0])
        return None

    def fold_binary(self, operator, first, second):
        if operator == "&&":
            return int(first != 0 and second != 0)
        if operator == "||":
            return int(first != 0 or second != 0)
        if operator in SWAPPED:
            return FOLD[SWAPPED[operator]](second, first)
        return FOLD[BINARY_OPS[operator]](first, second)

    def immediate(self, value, target=None):
        if target is None and IMMEDIATE_LOW <= value <= IMMEDIATE_HIGH:
            return IMMEDIATE_BIT | (value & 0x7FFF)
        place = self.fresh() if target is None else target
        if IMMEDIATE_LOW <= value <= IMMEDIATE_HIGH:
            self.emit("set", [place, IMMEDIATE_BIT | (value & 0x7FFF)])
        else:
            bits = value & INT_MASK
            self.emit("const", [place, bits & 0xFFFF, bits >> 16])
        return place

    def value(self, node, target=None):
        folded = self.constant(node)
        if folded is not None:
            return self.immediate(folded, target)
        kind = node[0]
        if kind == "name":
            if node[1] not in self.registers:
                raise SceneError(f"line {node[2]}: {node[1]!r} isn't named")
            place = self.registers[node[1]]
            if target is not None:
                self.emit("set", [target, place])
                return target
            return place
        if kind == "unary":
            operand = self.value(node[2])
            place = self.fresh() if target is None else target
            self.emit(node[1], [place, operand])
            return place
        if kind == "binary":
            return self.binary(node, target)
        if kind == "select":
            operands = [self.value(part) for part in node[1:]]
            place = self.fresh() if target is None else target
            self.emit("select", [place] + operands)
            return place
        if kind == "call":
            return self.function(node, target)
        raise SceneError(f"can't compute {node!r}")

    def truth(self, node):
        operand = self.value(node)
        place = self.fresh()
        self.emit("ne", [place, operand, IMMEDIATE_BIT])
        return place

    def binary(self, node, target):
        operator = node[1]
        if operator in ("&&", "||"):
            first, second = self.truth(node[2]), self.truth(node[3])
            place = self.fresh() if target is None else target
            self.emit("and" if operator == "&&" else "or", [place, first, second])
            return place
        if operator in SWAPPED:
            first, second = self.value(node[3]), self.value(node[2])
            op = SWAPPED[operator]
        else:
            first, second = self.value(node[2]), self.value(node[3])
            op = BINARY_OPS[operator]
        place = self.fresh() if target is None else target
        self.emit(op, [place, first, second])
        return place

    def function(self, node, target):
        name, arguments = node[1], node[2]
        entry = self.ops.get(name)
        if entry is None or entry["kind"] != "VALUE" or name in ("set", "const", "select"):
            raise SceneError(f"{name}() isn't a function that gives a value")
        if len(arguments) != entry["count"] - 1:
            raise SceneError(f"{name}() takes {entry['count'] - 1} values")
        operands = [self.value(argument) for argument in arguments]
        place = self.fresh() if target is None else target
        self.emit(name, [place] + operands)
        return place

    def label(self):
        return len(self.code)

    def patch(self, at, target):
        struct.pack_into("<H", self.code, at, target)

    def statements(self, statements):
        for statement in statements:
            saved = self.temporary
            self.statement(statement)
            self.temporary = max(saved, self.next_register)

    def block(self, statements):
        registers, constants = dict(self.registers), dict(self.constants)
        next_register, temporary = self.next_register, self.temporary
        self.statements(statements)
        self.registers, self.constants = registers, constants
        self.next_register, self.temporary = next_register, temporary

    def statement(self, statement):
        kind = statement[0]
        if kind == "const":
            value = self.constant(statement[2])
            if value is None:
                raise SceneError(f"line {statement[3]}: {statement[1]} must be known before playing")
            if statement[1] in self.registers or statement[1] in self.constants:
                raise SceneError(f"line {statement[3]}: {statement[1]!r} is already named")
            self.constants[statement[1]] = value
        elif kind == "let":
            place = self.variable(statement[1], statement[3])
            self.value(statement[2], place)
        elif kind == "assign":
            if statement[1] not in self.registers or statement[1] in self.inputs_set():
                raise SceneError(f"line {statement[3]}: {statement[1]!r} isn't a value made with let")
            self.value(statement[2], self.registers[statement[1]])
        elif kind == "if":
            self.branch(statement)
        elif kind == "repeat":
            self.loop(statement)
        elif kind == "call":
            self.draw(statement[1], statement[2])

    def inputs_set(self):
        return {name for name, index in self.registers.items() if index < self.first_variable}

    def branch(self, statement):
        _, condition, then, otherwise, _ = statement
        known = self.constant(condition)
        if known is not None:
            self.block(then if known else otherwise)
            return
        operand = self.value(condition)
        self.emit("jump_zero", [operand, 0])
        skip = self.label() - 2
        self.block(then)
        if otherwise:
            self.emit("jump", [0])
            leave = self.label() - 2
            self.patch(skip, self.label())
            self.block(otherwise)
            self.patch(leave, self.label())
        else:
            self.patch(skip, self.label())

    def loop(self, statement):
        registers, constants = dict(self.registers), dict(self.constants)
        next_register, temporary = self.next_register, self.temporary
        self.counted(statement)
        self.registers, self.constants = registers, constants
        self.next_register, self.temporary = next_register, temporary

    def counted(self, statement):
        _, name, first, last, body, line = statement
        counter = self.variable(name, line)
        self.value(first, counter)
        limit = self.value(last, self.variable(f" limit {self.next_register}", line))
        start = self.label()
        test = self.fresh()
        self.emit("lt", [test, counter, limit])
        self.emit("jump_zero", [test, 0])
        leave = self.label() - 2
        self.temporary -= 1
        self.block(body)
        self.emit("add", [counter, counter, IMMEDIATE_BIT | 1])
        self.emit("jump", [start])
        self.patch(leave, self.label())

    def draw(self, node, line):
        name, arguments = node[1], node[2]
        entry = self.ops.get(name)
        if entry is None or entry["kind"] != "DRAW":
            raise SceneError(f"line {line}: {name}() isn't something a scene can draw")
        if len(arguments) != entry["count"]:
            raise SceneError(f"line {line}: {name}() takes {entry['count']} values")
        self.emit(name, [self.value(argument) for argument in arguments])


def compile_scene(source, names):
    compiler = Compiler(names)
    compiler.first_variable = len(names["inputs"])
    compiler.statements(Parser(source).program())
    compiler.emit("end", [])
    strings = b"".join(text.encode("utf-8") + b"\0" for text in compiler.strings)
    return bytes(compiler.code), strings

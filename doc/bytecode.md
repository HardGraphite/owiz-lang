# Wo Bytecode

## Binary Format

An instruction consists of opcode and operand (optional).

Instructions without operand:

```
7       0
+-------+
|Opcode |
+-------+
```

Instructions with i8/u8 operand:

```
15     8 7      0
+-------+-------+
|Opcode |Operand|
+-------+-------+
```

Instructions with i16/u16 operand:

```
23    16 15             0
+-------+---------------+
|Opcode |    Operand    |
+-------+---------------+
```

## Table of Instructions

| Instruction  | Opcode | Operand | Stack Change        | Explanation                                 |
|--------------|--------|---------|---------------------|---------------------------------------------|
| `Nop`        | `0x00` | 0       |                     | Do nothing.                                 |
| `_01`        | `0x01` | 0       |                     | *(reserved)*                                |
| `Trap`       | `0x02` | 0       |                     | Pause and debug.                            |
| `_03`        | `0x03` | 0       |                     | *(reserved)*                                |
| `Swap`       | `0x04` | 0       | `a,b -> b,a`        | Swap 2 top values on stack.                 |
| `SwapN`      | `0x05` | u8: N   | ...                 | Circularly shift up top N values.           |
| `Drop`       | `0x06` | 0       | `v -> .`            | Pop top value on stack.                     |
| `DropN`      | `0x07` | u8: N   | `v1,...vN -> .`     | Pop top N values on stack.                  |
| `Dup`        | `0x08` | 0       | `a -> a,a`          | Duplicate top value on stack.               |
| `DupN`       | `0x09` | u8: N   | `a -> a,...,a`      | Duplicate top value on stack for N times.   |
| `LdNil`      | `0x0a` | 0       | `. -> v`            | Push nil.                                   |
| `LdBool`     | `0x0b` | u8: V   | `. -> v`            | Push true or false.                         |
| `LdInt`      | `0x0c` | i8: V   | `. -> v`            | Push integer (int8).                        |
| `LdIntW`     | `0x0d` | i16: V  | `. -> v`            | Push integer (int16).                       |
| `LdFlt`      | `0x0e` | i8: V   | `. -> v`            | Push floating-point.                        |
| `_0f`        | `0x0f` | 0       |                     | *(reserved)*                                |
| `Add`        | `0x10` | 0       | `lhs,rhs -> res`    | `res = lhs + rhs`.                          |
| `Sub`        | `0x11` | 0       | `lhs,rhs -> res`    | `res = lhs - rhs`.                          |
| `Mul`        | `0x12` | 0       | `lhs,rhs -> res`    | `res = lhs * rhs`.                          |
| `Div`        | `0x13` | 0       | `lhs,rhs -> res`    | `res = lhs / rhs`.                          |
| `Rem`        | `0x14` | 0       | `lhs,rhs -> res`    | `res = lhs % rhs`.                          |
| `Shl`        | `0x15` | 0       | `lhs,rhs -> res`    | `res = lhs << rhs`.                         |
| `Shr`        | `0x16` | 0       | `lhs,rhs -> res`    | `res = lhs >> rhs`.                         |
| `And`        | `0x17` | 0       | `lhs,rhs -> res`    | `res = lhs & rhs`.                          |
| `Or`         | `0x18` | 0       | `lhs,rhs -> res`    | `res = lhs bit_or rhs`.                     |
| `Xor`        | `0x19` | 0       | `lhs,rhs -> res`    | `res = lhs ^ rhs`.                          |
| `Neg`        | `0x1a` | 0       | `val -> res`        | `res = -val`.                               |
| `Inv`        | `0x1b` | 0       | `val -> res`        | `res = ~val`.                               |
| `Not`        | `0x1c` | 0       | `val -> res`        | `res = !val`.                               |
| `Test`       | `0x1d` | 0       | `val -> res`        | Convert value to bool.                      |
| `_1e`        | `0x1e` | 0       |                     | *(reserved)*                                |
| `_1f`        | `0x1f` | 0       |                     | *(reserved)*                                |
| `Is`         | `0x20` | 0       | `lhs,rhs -> res`    | Check whether `lhs` and `rhs` are the same. |
| `Cmp`        | `0x21` | 0       | `lhs,rhs -> res`    | Compare `lhs` and `rhs`.                    |
| `CmpLt`      | `0x22` | 0       | `lhs,rhs -> res`    | `res = lhs < rhs`.                          |
| `CmpLe`      | `0x23` | 0       | `lhs,rhs -> res`    | `res = lhs <= rhs`.                         |
| `CmpGt`      | `0x24` | 0       | `lhs,rhs -> res`    | `res = lhs > rhs`.                          |
| `CmpGe`      | `0x25` | 0       | `lhs,rhs -> res`    | `res = lhs >= rhs`.                         |
| `CmpEq`      | `0x26` | 0       | `lhs,rhs -> res`    | `res = lhs == rhs`.                         |
| `CmpNe`      | `0x27` | 0       | `lhs,rhs -> res`    | `res = lhs != rhs`.                         |
| `LdCnst`     | `0x28` | u8: CI  | `. -> v`            | Load constant.                              |
| `LdCnstW`    | `0x29` | u16: CI | `. -> v`            | Load constant.                              |
| `LdSym`      | `0x2a` | u8: YI  | `. -> v`            | Load symbol constant.                       |
| `LdSymW`     | `0x2b` | u16: YI | `. -> v`            | Load symbol constant.                       |
| `LdArg`      | `0x2c` | u8: I   | `. -> v`            | Load argument.                              |
| `StArg`      | `0x2d` | u8: I   | `v -> .`            | Store argument.                             |
| `LdLoc`      | `0x2e` | u8: I   | `. -> v`            | Load local variable.                        |
| `LdLocW`     | `0x2f` | u16: I  | `. -> v`            | Load local variable.                        |
| `StLoc`      | `0x30` | u8: I   | `v -> .`            | Store local variable.                       |
| `StLocW`     | `0x31` | u16: I  | `v -> .`            | Store local variable.                       |
| `LdGlob`     | `0x32` | u8: I   | `. -> v`            | Load global variable by index.              |
| `LdGlobW`    | `0x33` | u16: I  | `. -> v`            | Load global variable by index.              |
| `StGlob`     | `0x34` | u8: I   | `v -> .`            | Store global variable by index.             |
| `StGlobW`    | `0x35` | u16: I  | `v -> .`            | Store global variable by index.             |
| `LdGlobY`    | `0x36` | u8: YI  | `. -> v`            | Load global variable by symbol.             |
| `LdGlobYW`   | `0x37` | u16: YI | `. -> v`            | Load global variable by symbol.             |
| `StGlobY`    | `0x38` | u8: YI  | `v -> .`            | Store global variable by symbol.            |
| `StGlobYW`   | `0x39` | u16: YI | `v -> .`            | Store global variable by symbol.            |
| `LdAttrY`    | `0x3a` | u8: YI  | `obj -> attr`       | Load object attribute by symbol.            |
| `LdAttrYW`   | `0x3b` | u16: YI | `obj -> attr`       | Load object attribute by symbol.            |
| `StAttrY`    | `0x3c` | u8: YI  | `attr,obj -> .`     | Store object attribute by symbol.           |
| `StAttrYW`   | `0x3d` | u16: YI | `attr,obj -> .`     | Store object attribute by symbol.           |
| `LdElem`     | `0x3e` | 0       | `obj,idx -> elem`   | Load object element (subscript).            |
| `StElem`     | `0x3f` | 0       | `elem,obj,idx -> .` | Store object element (subscript).           |
| `Jmp`        | `0x40` | i8: O   |                     | Unconditional jump.                         |
| `JmpW`       | `0x41` | i16: O  |                     | Unconditional jump.                         |
| `JmpWhen`    | `0x42` | i8: O   | `cond -> .`         | Jump if top value is true.                  |
| `JmpWhenW`   | `0x43` | i16: O  | `cond -> .`         | Jump if top value is true.                  |
| `JmpUnls`    | `0x44` | i8: O   | `cond -> .`         | Jump if top value is false.                 |
| `JmpUnlsW`   | `0x45` | i16: O  | `cond -> .`         | Jump if top value is false.                 |
| `LdMod`      | `0x46` | u16: YI | `. -> mod`          | import module by name                       |
| `_47`        | `0x47` | 0       |                     | *(reserved)*                                |
| `Ret`        | `0x48` | 0       |                     | Return nil.                                 |
| `RetLoc`     | `0x49` | u8: I   | `v -> .` / `. -> .` | Return local variable or top value (I=255). |
| `Call`       | `0x4a` | u8: C   | `fn,a... -> [ret]`  | Call a function.                            |
| `_4b`        | `0x4b` | 0       |                     | *(reserved)*                                |
| `_4c`        | `0x4c` | 0       |                     | *(reserved)*                                |
| `_4d`        | `0x4d` | 0       |                     | *(reserved)*                                |
| `PrepMethY`  | `0x4e` | u8: I   | `obj -> meth,obj`   | Load method by symbol and push object.      |
| `PrepMethYW` | `0x4f` | u16: I  | `obj -> meth,obj`   | Load method by symbol and push object.      |
| `MkArr`      | `0x50` | u8: N   | `e1,e2,... -> arr`  | Make an array.                              |
| `MkArrW`     | `0x51` | u16: N  | `e1,e2,... -> arr`  | Make an array.                              |
| `MkTup`      | `0x52` | u8: N   | `e1,e2,... -> tup`  | Make a tuple.                               |
| `MkTupW`     | `0x53` | u16: N  | `e1,e2,... -> tup`  | Make a tuple.                               |
| `MkSet`      | `0x54` | u8: N   | `e1,e2,... -> set`  | Make a set.                                 |
| `MkSetW`     | `0x55` | u16: N  | `e1,e2,... -> set`  | Make a set.                                 |
| `MkMap`      | `0x56` | u8: N   | `k1,v1,... -> map`  | Make a map.                                 |
| `MkMapW`     | `0x57` | u16: N  | `k1,v2,... -> map`  | Make a map.                                 |

Meaning of operand column:

- width
  - `0`: no operand
  - `i8`/`u8`: signed/unsigned 8-bit integer
  - `i16`/`u16`: signed/unsigned 16-bit integer
- type
  - `N`: number
  - `V`: immediate value
  - `I`: index
    - `CI`: constant table index
    - `YI`: symbol table index
  - `O`: offset
  - `C`: calling info: `C[7]` is discard_ret_val flag; `C[6:0]` is number of arguments

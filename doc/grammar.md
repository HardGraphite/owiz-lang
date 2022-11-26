# Ow Grammar

## Comments

- line comment: `# ... <LF>`
- block comment: `#= ... =#`

## Literals

- integer
  - dec: `[0-9_']+`
  - hex: `0[xX][0-9a-fA-F_']+`
  - bin: `0[bB][01_']+`
  - oct: `0[oO]?[0-7_']+`
- floating-point
  - fixed: `[:integer/dec:]\.[:integer/dec:]`
- string
  - simple: `[:quote:]([^\[:quote:]]|[:esc_seq:])*[:quote:]`
    - `[:quote:]` is `"` or `'`
    - `[:esc_seq:]`: see the table below for details
- identifier: `[^\W\d]\w*`
- symbol: ` ``[:identifier:] `

Escape sequences in string literal is listed below:

|    Sequence    | Character    |
|----------------|--------------|
| `\a`           | U+0007       |
| `\b`           | U+0008       |
| `\e`           | U+001B       |
| `\f`           | U+000C       |
| `\n`           | U+000A       |
| `\r`           | U+000D       |
| `\t`           | U+0009       |
| `\v`           | U+000B       |
| `\x`*nn*       | U+00*nn*     |
| `\u`*nnnn*     | U+*nnnn*     |
| `\U`*nnnnnnnn* | U+*nnnnnnnn* |

Examples:

```
# Ingeters
123
0xFe   # => 254
0777   # => 511
0o0    # => 0
0b1010 # => 10
0b0100_0000
0b1011'1111
# Floating points
3.14
# Strings
"It said, \"Hello, world!\"."
'\u4f60\u597d\x2c\x20\u4e16\u754c\U0001f600'
# Symbol
`a_symbol
# Identifier
an_identifier
AnIdentifier
Identifier1
```

## Expressions

### Literal

A literal value itself is an expression.

### Collection type literal expressions

Syntax:

```
tuple_expr
  = "(" ")"
  | "(" expr "," ")"
  | "(" expr { "," expr } [ "," ] ")"
  ;

array_expr
  = "[" "]"
  | "[" expr [{ "," expr }] [ "," ] "]"
  ;

set_expr
  = "{" "," "}"
  | "{" expr [{ "," expr }] [ "," ] "}"
  ;

map_expr
  = "{" "}"
  | "{" map_expr_elem [{ "," map_expr_elem }] [ "," ] "}"
  ;
map_expr_elem
  = expr "=>" expr
  ;
```

Examples:

```
# Tuples
()
(1,)
(1, 2)
# Arrays
[]
[1]
[1,2,]
# Sets
{,}
{1}
# Tables
{}
{1 => "1"}
{1=>"1",2=>"2",}
```

### Operator expressions

Syntax:

```
operator_expr
  = UN_OP expr
  | expr BIN_OP expr
  ;
```

where `UN_OP` is a unary operator, and `BIN_OP` is a binary operator.

Operators and their precedences:

| Operator                         | Precedence | Associativity |
|----------------------------------|:----------:|:-------------:|
| `.`, `:`                         |     1      |       L       |
| `a(b)`, `a[b]`                   |     2      |       L       |
| `+x`, `-x`, `!x`, `~x`           |     3      |       R       |
| `*`, `/`, `%`                    |     4      |       L       |
| `+`, `-`                         |     5      |       L       |
| `<<`, `>>`                       |     6      |       L       |
| `<`, `<=`, `>`, `>=`             |     8      |       L       |
| `==`, `!=`                       |     9      |       L       |
| `&`                              |     10     |       L       |
| `^`                              |     11     |       L       |
| <code>&#124;</code>              |     12     |       L       |
| `&&`                             |     13     |       L       |
| <code>&#124;&#124;</code>        |     14     |       L       |
| `=`                              |     15     |       R       |
| `*=`, `/=`, `%=`                 |     15     |       R       |
| `+=`, `-=`                       |     15     |       R       |
| `<<=`, `>>=`                     |     15     |       R       |
| `&=`, `^=`, <code>&#124;=</code> |     15     |       R       |

Examples:

```
ans = (1 + 2) * 3
0x80 & (1 << 7)
```

### Invocation expression

Syntax:

```
call_expr
  = expr "(" call_expr_arglist ")"
  ;
call_expr_arglist
  = (* empty *)
  | expr [{ "," expr }] [ "," ]
  ;
```

### Subscript expression

Syntax:

```
subscript_expr
  = expr "[" subscript_expr_indexlist "]"
  ;
subscript_expr_indexlist
  = expr [{ "," expr }] [ "," ]
  ;
```

Examples:

```
list[0]
matrix[3,2]
```

## Statements

### Expression statement

Syntax:

```
expr_stmt
  = expr END_LINE
  ;
```

where `END_LINE` is character `U+000A` (LF) or semicolon (`;`).

### Block statement

Syntax:

```
block_stmt
  = [{ stmt }]
  ;
```

### End statement

It is not a standalone statement.
It is usually used to mark the end of a block of code.

Syntax:

```
end_stmt
  = "end" END_LINE
  ;
```

### Return statement

To return from a function.

Syntax:

```
return_stmt
  = "return" [ expr ] END_LINE
  ;
```

### If-else statement

Syntax:

```
if_else_stmt
  = if_else_stmt_if_branch
    [{ if_else_stmt_elif_branch }]
    [ if_else_stmt_else_branch ]
    end_stmt
  ;
if_else_stmt_if_branch
  = "if" expr END_LINE
    block_stmt
  ;
if_else_stmt_elif_branch
  = "elif" expr END_LINE
    block_stmt
  ;
if_else_stmt_else_branch
  = "else" END_LINE
    block_stmt
  ;
```

### For statement

Syntax:

```
for_stmt
  = "for" IDENTIFIER "<-" expr END_LINE
    block_stmt
    end_stmt
  ;
```

### While statement

Syntax:

```
while_stmt
  = "while" expr END_LINE
    block_stmt
    end_stmt
  ;
```

### Function defining statement

Syntax:

```
func_stmt
  = "func" IDENTIFIER "(" func_stmt_arglist ")" END_LINE
    block_stmt
    end_stmt
  ;
func_stmt_arglist
  = [ expr { "," expr } [ "," ] ]
  ;
```

Example:

```
func foo(arg1, arg2)
  # do something
end
```

## Module

A module is the code in a file (or some file-like object).
It consists of a set of statements, or, in another word, a block-statement.
The stem part of the file name is the name of the module,
which must be a legal identifier.

Specially, a module file can be a shared library (or dynamic-link library)
with specified exported functions and variables.
This kind of module file is written in native code.

A module can contain a function called `main`,
and the function will be called automatically if the module is the entry of a program
(aka the top-level environment).

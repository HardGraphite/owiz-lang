# Ow Grammar

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

Escape sequences in string literal is listed below:

|    Sequence    |    Character   |
|----------------|----------------|
| `\a`           | U+0007         |
| `\b`           | U+0008         |
| `\e`           | U+001B         |
| `\f`           | U+000C         |
| `\n`           | U+000A         |
| `\r`           | U+000D         |
| `\t`           | U+0009         |
| `\v`           | U+000B         |
| `\x`*XX*       | U+00*XX*       |
| `\u`*XXXX*     | U+00*XXXX*     |
| `\U`*XXXXXXXX* | U+00*XXXXXXXX* |

## Comments

- line comment: `# ...`
- block comment: `#= ... =#`

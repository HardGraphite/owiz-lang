#!/bin/env python3

"""
*.c -> "module_list.h"
"""

import functools
import pathlib
import re
from typing import Generator, Iterable


def scan_file(file) -> str | None:
    with open(file) as f:
        for line in f:
            if not line.startswith('OW_BIMOD_MODULE_DEF'):
                continue
            m = re.match(r'^OW_BIMOD_MODULE_DEF\((\w+)\).*', line)
            if not m:
                continue
            return m[1]


def scan_dir(path) -> Generator[str, None, None]:
    return filter(
        lambda name: name is not None,
        map(
            scan_file,
            filter(
                lambda file: file.match('*.c'),
                pathlib.Path(path).iterdir()
            )
        )
    )


def write_list(names: Iterable[str], out_file):
    with open(out_file, 'w', newline='\n') as f:
        puts = functools.partial(print, file=f)

        puts(f'// Generated automatically. DO NOT edit.')
        puts()

        puts('#define OW_BIMOD_LIST \\')
        for name in names:
            text = f'ELEM({name})'
            puts(f'\t{text: <18}\\')
        puts('// ^^^ OW_BIMOD_LIST ^^^')


def main():
    modules_dir = pathlib.Path(__file__).parent
    list_file =  pathlib.Path(__file__).with_suffix('.h')

    write_list(scan_dir(modules_dir), list_file)


if __name__ == '__main__':
    main()

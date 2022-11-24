#!/bin/env python3

import argparse
import dataclasses
import functools
import re


@dataclasses.dataclass
class Version:
    major: int
    minor: int
    patch: int
    state: str | None = None
    extra: list[str] | None = None

    def __str__(self) -> str:
        text = f'{self.major}.{self.minor}.{self.patch}'
        if self.state:
            text += '-' + self.state
        if self.extra:
            text += '+' + '+'.join(self.extra)
        return text


def load_version(version_file) -> Version:
    with open(version_file) as f:
        data = f.read(256)
    m = re.match(r'(\d+)\.(\d+)\.(\d+)((?:-\w+)?)((?:\+\w+)*)', data)
    if not m:
        raise RuntimeError('invalid version string: ' + data)
    version = Version(int(m[1]), int(m[2]), int(m[3]))
    if m[4]:
        version.state = m[4][1:]
    if m[5]:
        version.extra = m[5][1:].split('+')
    return version


def generate_header(version: Version, out_file):
    with open(out_file, 'w', newline='\n') as f:
        puts = functools.partial(print, file=f)

        puts('#pragma once')
        puts()
        puts('#define', 'OW_VERSION_MAJOR ', version.major)
        puts('#define', 'OW_VERSION_MINOR ', version.minor)
        puts('#define', 'OW_VERSION_PATCH ', version.patch)
        puts('#define', 'OW_VERSION_STRING', '"' + str(version) + '"')


def main():
    arg_parser = argparse.ArgumentParser()
    arg_parser.description = 'Verify and query project version.'
    arg_parser.add_argument('-p', '--print', action='store_true',
        help='print current version')
    arg_parser.add_argument('-P', '--print-stem', action='store_true',
        help='print current version (major.minor.patch)')
    arg_parser.add_argument('-H', '--make-header', help='generate C header file')
    arg_parser.add_argument('VERSION_FILE', nargs='?',
        help='path to the VERSION.txt file')
    args = arg_parser.parse_args()
    del arg_parser

    version = load_version(args.VERSION_FILE if args.VERSION_FILE else 'VERSION.txt')

    if args.print:
        print(version)
    if args.print_stem:
        print(version.major, version.minor, version.patch, sep='.', end='')
    if args.make_header:
        generate_header(version, args.make_header)


if __name__ == '__main__':
    main()

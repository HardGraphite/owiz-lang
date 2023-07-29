#!/bin/env python3

"""
(modules) -> "modules_conf.cmake"
"""

import configparser
import dataclasses
import functools
import pathlib
import re
from typing import Generator, Iterable


C_CODE_INDENT = ' ' * 4


@dataclasses.dataclass
class ModuleConfig:
    required: bool = False
    must_embed: bool = False
    prefer_dyn: bool = False
    depend_mods: None | list[str] = None
    sources: None | str = None
    link_libs: None | str = None


def load_config(file) -> dict[str, ModuleConfig]:
    conf_file = configparser.ConfigParser(default_section='*')
    conf_file.read(file)
    result: dict[str, ModuleConfig] = {}
    for name in ('*', *conf_file.sections()):
        sect = conf_file[name]
        mod_conf = ModuleConfig()
        mod_conf.required = sect.getboolean('required')
        mod_conf.must_embed = sect.getboolean('must-embed')
        mod_conf.prefer_dyn = sect.getboolean('prefer-dyn')
        depend_mods = sect['depend-mods']
        if depend_mods:
            mod_conf.depend_mods = depend_mods.split(',')
        sources = sect['sources']
        if sources:
            mod_conf.sources = sources.replace(':', ';')
        link_libs = sect['link-libs']
        if link_libs:
            mod_conf.link_libs = link_libs.replace(':', ';')
        result[name] = mod_conf
    return result


def scan_file(file: pathlib.Path) -> str | None:
    with open(file) as f:
        for line in f:
            if not line.startswith('OW_BIMOD_MODULE_DEF'):
                continue
            m = re.match(r'^OW_BIMOD_MODULE_DEF\((\w+)\).*', line)
            if not m:
                continue
            name = m[1]
            if name != file.stem:
                raise RuntimeError(
                    f'Module name "{name}" does not match file name {file}.')
            return name


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


def write_cmake_conf(
        names: Iterable[str], conf: dict[str, ModuleConfig], out_file):
    names = sorted(set(names))

    default_conf = conf['*']
    assert not default_conf.required
    assert not default_conf.must_embed
    assert not default_conf.depend_mods
    assert not default_conf.link_libs

    with open(out_file, 'w', newline='\n') as f:
        puts = functools.partial(print, file=f)

        puts(f'# Generated automatically. DO NOT edit.')
        puts()

        puts('set(OW_AVAILABLE_MODULES')
        for mod_name in names:
            puts(C_CODE_INDENT + mod_name)
        puts(')')
        puts()

        puts('set(OW_REQUIRED_MODULES')
        for mod_name, mod_conf in conf.items():
            if mod_conf.required:
                puts(C_CODE_INDENT + mod_name)
        puts(')')
        puts()

        puts('set(OW_PRFDYN_MODULES')
        for mod_name, mod_conf in conf.items():
            if mod_conf.prefer_dyn:
                puts(C_CODE_INDENT + mod_name)
        puts(')')
        puts()

        for mod_name, mod_conf in conf.items():
            var_name_prefix = f'OW_MOD_{mod_name}_'
            if mod_conf.must_embed:
                puts(f'set({var_name_prefix}MUST_EMBED TRUE)')
            if mod_conf.depend_mods:
                val = ' '.join(mod_conf.depend_mods)
                puts(f'set({var_name_prefix}DEP_MODS {val})')
            if mod_conf.sources:
                puts(f'set({var_name_prefix}SOURCES {mod_conf.sources})')
            if mod_conf.link_libs:
                puts(f'set({var_name_prefix}LINK_LIBS {mod_conf.link_libs})')


def main():
    modules_dir = pathlib.Path(__file__).parent
    conf_file = modules_dir / 'modules_conf.ini'
    result_file = modules_dir / 'modules_conf.cmake'

    conf = load_config(conf_file)
    write_cmake_conf(scan_dir(modules_dir), conf, result_file)


if __name__ == '__main__':
    main()

#!/bin/env python3

"""
"ast_node_def.ini" -> "ast_node_list.h", "ast_node_funcs.h", "ast_node_structs.h"
"""

import ast
import configparser
import dataclasses
import functools
import pathlib
import re


C_CODE_INDENT = ' ' * 4


@dataclasses.dataclass
class NodeDef:
    name: str
    base: str | None = None
    attr: list[str] | None = None
    ctor: str | None = None
    dtor: str | None = None


def load_defs(file) -> list[NodeDef]:
    """Load node definitions from config file."""
    node_defs: list[NodeDef] = []

    def read_code(raw: str) -> str:
        if not raw[0] == '"':
            return raw
        res = ast.literal_eval(raw)
        if not isinstance(res, str):
            raise RuntimeError('invalid code string: ' + raw)
        return res

    parser = configparser.ConfigParser()
    parser.read(file)
    for node_name in parser.sections():
        node_def = NodeDef(name=node_name)
        section = parser[node_name]
        if 'base' in section:
            node_def.base = section['base']
        if 'attr' in section:
            node_def.attr = list(map(str.strip, section['attr'].split(',')))
        if 'ctor' in section:
            node_def.ctor = read_code(section['ctor'])
        if 'dtor' in section:
            node_def.dtor = read_code(section['dtor'])
        node_defs.append(node_def)

    return node_defs


def write_list(node_defs: list[NodeDef], conf_file: pathlib.Path, out_file):
    with open(out_file, 'w', newline='\n') as f:
        puts = functools.partial(print, file=f)

        puts(f'// Generated from "{conf_file.name}". DO NOT edit.')
        puts()

        puts('#define OW_AST_NODE_LIST \\')
        for node in node_defs:
            if node.name[0] == '.':
                continue
            text = f'ELEM({node.name})'
            puts(C_CODE_INDENT + f'{text: <22}\\')
        puts('// ^^^ OW_AST_NODE_LIST ^^^')


def write_structs(node_defs: list[NodeDef], conf_file: pathlib.Path, out_file):
    node_defs_dict = {node.name: node for node in node_defs}

    with open(out_file, 'w', newline='\n') as f:
        puts = functools.partial(print, file=f)

        def put_members(node: NodeDef):
            if node.base:
                base_node = node_defs_dict[node.base]
                put_members(base_node)
            if node.attr:
                for attr in node.attr:
                    puts(C_CODE_INDENT, attr, ';', sep='')

        puts(f'// Generated from "{conf_file.name}". DO NOT edit.')

        for node in node_defs:
            name = node.name[1:] if node.name[0] == '.' else node.name
            puts()
            puts('struct ow_ast_', name, ' {', sep='')
            puts(C_CODE_INDENT + 'OW_AST_NODE_HEAD')
            put_members(node)
            puts('};')


def write_funcs(node_defs: list[NodeDef], conf_file: pathlib.Path, out_file):
    node_param = 'node'
    node_defs_dict = {node.name: node for node in node_defs}

    def conv_code(code: str) -> str:
        return re.sub(r'\$(\w+)', node_param + r'->\1', code)

    with open(out_file, 'w', newline='\n') as f:
        puts = functools.partial(print, file=f)

        puts(f'// Generated from "{conf_file.name}". DO NOT edit.')

        for node in node_defs:
            name = node.name[1:] if node.name[0] == '.' else node.name
            node_type = 'struct ow_ast_' + name
            func_prefix = 'ow_ast_' + name + '_'
            if node.base:
                base_node = node_defs_dict[node.base]
                base_node_name = \
                    base_node.name[1:] if base_node.name[0] == '.' else base_node.name
                base_node_type = 'struct ow_ast_' + base_node_name
                base_func_prefix = 'ow_ast_' + base_node_name + '_'

            puts()
            puts('static void ', func_prefix, 'init(',
                node_type , ' *', node_param, ') {', sep='')
            has_code = False
            if node.base:
                puts(C_CODE_INDENT, base_func_prefix,
                    'init((', base_node_type, ' *)', node_param, ');', sep='')
                has_code = True
            if node.ctor:
                for s in map(str.strip, conv_code(node.ctor).split(';')):
                    if s:
                        puts(C_CODE_INDENT, s, ';', sep='')
                has_code = True
            if not has_code:
                puts(C_CODE_INDENT, 'ow_unused_var(', node_param, ');', sep='')
            puts('}')

            puts()
            puts('static void ', func_prefix, 'fini(',
                node_type , ' *', node_param, ') {', sep='')
            has_code = False
            if node.dtor:
                for s in map(str.strip, conv_code(node.dtor).split(';')):
                    if s:
                        puts(C_CODE_INDENT, s, ';', sep='')
                has_code = True
            if node.base:
                puts(C_CODE_INDENT, base_func_prefix,
                    'fini((', base_node_type, ' *)', node_param, ');', sep='')
                has_code = True
            if not has_code:
                puts(C_CODE_INDENT, 'ow_unused_var(', node_param, ');', sep='')
            puts('}')


def main():
    conf_file = pathlib.Path(__file__).with_suffix('.ini')
    list_file =  pathlib.Path(__file__).parent / 'ast_node_list.h'
    structs_file = pathlib.Path(__file__).parent / 'ast_node_structs.h'
    funcs_file = pathlib.Path(__file__).parent / 'ast_node_funcs.h'
    if not conf_file.exists():
        raise RuntimeError(f'config file {conf_file} does not exist')

    node_defs = load_defs(conf_file)
    write_list(node_defs, conf_file, list_file)
    write_structs(node_defs, conf_file, structs_file)
    write_funcs(node_defs, conf_file, funcs_file)


if __name__ == '__main__':
    main()

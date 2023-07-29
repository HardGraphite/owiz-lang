#!/bin/env python3

import argparse
import dataclasses
import enum
import json
import sys
from typing import Mapping, TextIO
import xml.sax


class DotWriter:
    def __init__(self, output_stream: TextIO):
        self.out = output_stream
        self.indent_level = 0

    def _print_indent(self):
        self.out.write('\t' * self.indent_level)

    def _print_attrs_and_lf(self, attrs):
        if attrs:
            attr_list = (name + '=' + value for name, value in attrs.items())
            self.out.write(' [' + ' '.join(attr_list) + ']\n')
        else:
            self.out.write('\n')

    def start_graph(self, name: str):
        self._print_indent()
        self.indent_level += 1
        self.out.write(f'digraph {name} {{\n')

    def end_graph(self):
        self.indent_level -= 1
        self._print_indent()
        self.out.write('}')

    def declare_node(self, name: str, **attrs):
        self._print_indent()
        self.out.write(name)
        self._print_attrs_and_lf(attrs)

    def put_edge(self, from_: str, to: str, **attrs):
        self._print_indent()
        self.out.write(from_)
        self.out.write(' -> ')
        self.out.write(to)
        self._print_attrs_and_lf(attrs)


class AstDataStreamFilter:
    def __init__(self, text_stream: TextIO) -> None:
        self.input_stream = text_stream
        self.state = 0

    def close(self):
        self.input_stream.close()
        self.state = -1

    def read(self, length: int = -1) -> str:
        if self.state == 0:
            while True:
                if self.input_stream.readline().endswith('AST vvv'):
                    self.state = 1
                    return self.read(length)
        elif self.state == 1:
            res = self.input_stream.read(length)
            pos = res.find('AST ^^^')
            if pos == -1:
                return res
            self.state = 2
            pos1 = res.find('\n', max(pos - 80, 0), pos)
            return res[:pos if pos1 == -1 else pos1]
        else:
            return ''


@enum.unique
class XmlElemType(enum.Enum):
    Node = 0
    Attr = 1


@dataclasses.dataclass
class XmlElemInfo:
    type: XmlElemType
    name: str


class AstConvertHandler(xml.sax.ContentHandler, DotWriter):
    @staticmethod
    def quote_string(s: str) -> str:
        assert isinstance(s, str)
        s = json.dumps(s)
        assert isinstance(s, str)
        return s

    def __init__(self, output_stream: TextIO):
        xml.sax.ContentHandler.__init__(self)
        DotWriter.__init__(self, output_stream)
        self.elem_stack: list[XmlElemInfo] = []
        self.elem_count = 0
        self.contents: list[str] = []

    def _allocate_elem_id(self, type: XmlElemType) -> str:
        res = type.name[0] + str(self.elem_count)
        self.elem_count += 1
        return res

    def startDocument(self):
        self.elem_stack.clear()
        self.elem_stack.append(XmlElemInfo(XmlElemType.Node, 'AST'))

        self.start_graph('AST')
        self.declare_node('AST', shape='doubleoctagon')

    def endDocument(self):
        assert len(self.elem_stack) == 1 and self.elem_stack[0].name == 'AST'
        self.elem_stack.pop()

        self.end_graph()

    def startElement(self, tag: str, attrs: Mapping[str, str]):
        parent_elem = self.elem_stack[-1]
        elem_type = XmlElemType.Node if tag[0].isupper() else XmlElemType.Attr
        elem_name = self._allocate_elem_id(elem_type)
        self.elem_stack.append(XmlElemInfo(elem_type, elem_name))

        if 'location' in attrs:
            label = '"' + tag + '\\n(' + attrs['location'] + ')"'
        else:
            label = tag
        if elem_type is XmlElemType.Node:
            shape = 'box'
        else:
            shape = 'none'
        self.declare_node(elem_name, label=label, shape=shape)
        self.put_edge(parent_elem.name, elem_name)

    def endElement(self, tag: str):
        this_elem = self.elem_stack[-1]
        if self.contents:
            elem_name = self._allocate_elem_id(XmlElemType.Node)
            text = AstConvertHandler.quote_string(' '.join(self.contents))
            self.declare_node(elem_name, label=text, shape='underline')
            self.put_edge(this_elem.name, elem_name)
            self.contents.clear()

        self.elem_stack.pop()

    def characters(self, content: str):
        content = content.strip()
        if content:
            self.contents.append(content)


def main():
    arg_parser = argparse.ArgumentParser()
    arg_parser.description = 'Convert AST (from parse debug output) to Graphviz DOT'
    arg_parser.add_argument('-p', '--plain', action='store_true',
        help='the inputs contains only XML, without other message from parse debug output')
    arg_parser.add_argument('-o', '--output', required=False,
        help='path to output file; if not specified, output to stdout')
    arg_parser.add_argument('INPUT', default='-',
        help='path to AST data file, or "-" to read from stdin')
    args = arg_parser.parse_args()
    del arg_parser

    input_stream, output_stream = None, None
    try:
        input_stream = sys.stdin if args.INPUT == '-' else open(args.INPUT, 'r')
        output_stream = sys.stdout if not args.output else open(args.output, 'w')
        if not args.plain:
            input_stream = AstDataStreamFilter(input_stream)
        xml.sax.parse(input_stream, AstConvertHandler(output_stream))
    finally:
        if input_stream is not None and input_stream is not sys.stdin:
            input_stream.close()
        if output_stream is not None and output_stream is not sys.stdout:
            output_stream.close()


if __name__ == '__main__':
    main()

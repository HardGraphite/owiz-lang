#!/bin/env python3

import argparse
import dataclasses
import pathlib
import re
import time
from typing import Generator, Iterable


@dataclasses.dataclass
class FileCodeInfo:
    file_size: int
    total_lines: int
    blank_lines: int
    comment_lines: int
    code_lines: int

    def __iadd__(self, other):
        assert isinstance(other, FileCodeInfo)
        self.file_size += other.file_size
        self.total_lines += other.total_lines
        self.blank_lines += other.blank_lines
        self.comment_lines += other.comment_lines
        self.code_lines += other.code_lines
        return self


@dataclasses.dataclass
class LineCounterMeta:
    line_comment: str | None
    block_comment: tuple[str, str] | None
    string_pattern: re.Pattern | None


def scan_code(file_path: pathlib.Path, meta: LineCounterMeta) -> FileCodeInfo:
    code_info = FileCodeInfo(file_path.stat().st_size, 0, 0, 0, 0)
    with open(file_path, 'r') as file:
        in_block_comment = False
        for line in file:
            code_info.total_lines += 1
            line = line.strip()
            if in_block_comment:
                pos1 = line.find(meta.block_comment[1])
                if pos1 == -1:
                    code_info.comment_lines += 1
                    continue
                in_block_comment = False
                line = line[pos1 + len(meta.block_comment[1]):]
                if not line:
                    code_info.comment_lines += 1
                    continue
                else:
                    code_info.code_lines += 1
            else:
                if not line:
                    code_info.blank_lines += 1
                    continue
                if (meta.line_comment and line.startswith(meta.line_comment)) \
                        or (meta.block_comment
                            and line.startswith(meta.block_comment[0])
                            and line.endswith(meta.block_comment[1])):
                    code_info.comment_lines += 1
                    continue
                code_info.code_lines += 1
            if meta.string_pattern is not None:
                line = meta.string_pattern.sub('', line)
            pos = 0
            while True:
                pos1 = line.find(meta.line_comment, pos) \
                    if meta.line_comment else -1
                pos2 = line.find(meta.block_comment[0], pos) \
                    if meta.block_comment else -1
                if pos1 >= 0 and (pos1 < pos2 or pos2 < 0):
                    break
                if pos2 < 0:
                    assert pos1 == -1 and pos2 == -1
                    break
                assert pos2 >= 0
                pos3 = line.find(
                    meta.block_comment[1], pos2 + len(meta.block_comment[0]))
                if pos3 >= 0:
                    pos = pos3 + len(meta.block_comment[1])
                    continue
                else:
                    in_block_comment = True
                    break
        if in_block_comment:
            print(
                '*** Warning: cannot find the end of a comment block in file ',
                file_path
            )
    return code_info


FILE_NAME_GLOB = [
    ('C', '*.c'),
    ('C++', '*.cc'),
    ('C/C++ header', '*.h'),
    ('Python', '*.py'),
    ('CMake', '*.cmake'),
    ('CMake', 'CMakeLists.txt'),
    ('INI', '*.ini'),
    ('Owiz', '*.ow'),
]


def file_type_from_name(file_path: pathlib.Path) -> str | None:
    for name, glob in FILE_NAME_GLOB:
        if file_path.match(glob):
            return name


__c_lc_meta = LineCounterMeta(
    '//', ('/*', '*/'), re.compile(r'"(?:\\.|[^"\\])*"'))
__py_lc_meta = LineCounterMeta(
    '#', None, re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''))
__ow_lc_meta = LineCounterMeta(
    '#', None, re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''))

LINE_COUNTER_META = {
    'C': __c_lc_meta,
    'C++': __c_lc_meta,
    'C/C++ header': __c_lc_meta,
    'Python': __py_lc_meta,
    'CMake': LineCounterMeta('#', None, None),
    'INI': LineCounterMeta(';', None, None),
    'Owiz': __ow_lc_meta,
}


@dataclasses.dataclass
class FileInfo:
    file_type: str
    code_info: FileCodeInfo


def scan_file(file_path: pathlib.Path) -> FileInfo | None:
    if file_type := file_type_from_name(file_path):
        return FileInfo(
            file_type,
            scan_code(file_path, LINE_COUNTER_META[file_type]),
        )


def scan_dir(
        dir_path: pathlib.Path,
        include_glob: list[str], exclude_glob: list[str]) \
            -> Generator[FileInfo, None, None]:
    for sub_path in dir_path.iterdir():
        if sub_path.is_file():
            sub_path_match_fn = sub_path.match
            if include_glob and not any(map(sub_path_match_fn, include_glob)):
                continue
            if exclude_glob and any(map(sub_path_match_fn, exclude_glob)):
                continue
            if (fi := scan_file(sub_path)) is not None:
                yield fi
        elif sub_path.is_dir():
            for fi in scan_dir(sub_path, include_glob, exclude_glob):
                yield fi


def print_stats(fis: Iterable[FileInfo]):
    @dataclasses.dataclass
    class FilesStatsInfo:
        file_count: int
        code_info: FileCodeInfo

    si_by_ft: dict[str, FilesStatsInfo] = {}
    si_sum = FilesStatsInfo(0, FileCodeInfo(0, 0, 0, 0, 0))
    for fi in fis:
        if fi.file_type not in si_by_ft:
            si = FilesStatsInfo(0, FileCodeInfo(0, 0, 0, 0, 0))
            si_by_ft[fi.file_type] = si
        else:
            si = si_by_ft[fi.file_type]
        si.file_count += 1
        si.code_info += fi.code_info
        si_sum.file_count += 1
        si_sum.code_info += fi.code_info

    def put_bar():
        print('-' * 13 + '+' + '-' * 66)

    def put_line(
            file_type, file_cnt, file_size, file_size_percent,
            total, blank, comment, code):
        if isinstance(file_size, int):
            if file_size >= 10 * 1024 * 1024:
                file_size = f'{file_size / (1024 * 1024):.0f} MiB'
            elif file_size >= 10240:
                file_size = f'{file_size / 1024:.0f} KiB'
            else:
                file_size = str(file_size) + ' B'
        print(f'{file_type: <12} | {file_cnt: >5} '
            + f'{file_size: >10} {file_size_percent: >3}% '
            + f'{total: >10} {blank: >10} {comment: >10} {code: >10}')

    put_bar()
    put_line(
        'Language', 'Files', 'Size', '',
        'Total ln', 'Blank ln', 'Comment ln', 'Code ln')
    put_bar()
    for ft, si in sorted(
            si_by_ft.items(), key=lambda x: x[1].code_info.file_size,
            reverse=True):
        fc, ci = si.file_count, si.code_info
        percentage = round(ci.file_size / si_sum.code_info.file_size * 100)
        put_line(
            ft, fc, ci.file_size, percentage,
            ci.total_lines, ci.blank_lines, ci.comment_lines, ci.code_lines)
    put_bar()
    put_line(
        '[SUM]', si_sum.file_count,
        si_sum.code_info.file_size, 100,
        si_sum.code_info.total_lines, si_sum.code_info.blank_lines,
        si_sum.code_info.comment_lines, si_sum.code_info.code_lines)
    put_bar()


def main():
    arg_parser = argparse.ArgumentParser()
    arg_parser.description = 'Print line counts and file sizes of code.'
    arg_parser.add_argument(
        '-i', '--include', action='append', help='included path glob')
    arg_parser.add_argument(
        '-x', '--exclude', action='append', help='excluded path glob')
    arg_parser.add_argument(
        'ROOT_DIR', nargs='?', default=pathlib.Path(__file__).parent.parent,
        type=pathlib.Path, help='root directory to search for code files')
    args = arg_parser.parse_args()

    root_dir = args.ROOT_DIR
    if not (root_dir.exists() and root_dir.is_dir()):
        print(f'*** No such directory: {root_dir}')
        exit(1)
    include_list = [] if args.include is None else args.include
    exclude_list = [] if args.exclude is None else args.exclude

    t0 = time.time_ns()
    print('** Root directory:', root_dir)
    print_stats(scan_dir(root_dir, include_list, exclude_list))
    t1 = time.time_ns()
    print('** Time cost:', (t1 - t0) / 1e9, 's')


if __name__ == '__main__':
    main()

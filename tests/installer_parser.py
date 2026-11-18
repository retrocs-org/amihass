#!/usr/bin/env python3
"""Static checks for the Install-NovaDock Installer script.

The Commodore Installer cannot run on the build host, so this parses the
script and checks what would otherwise only fail on an Amiga: balanced
parentheses, terminated strings, known function names (Installer 42, OS 3.1,
has no procedure arguments), prompt/help/choices on every question,
variables set before use, and that every package file the script copies or
runs is really in the package.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'Install-AmiHASS'

FUNCTIONS = {
    # control flow and data
    'set', 'if', 'while', 'until', 'select', 'procedure', 'foreach', 'trap',
    'onerror', 'cat', 'tackon', 'pathonly', 'fileonly', 'expandpath',
    'exists', 'getsize', 'getdiskspace', 'getenv', 'getversion', 'earlier',
    'substr', 'strlen', 'patmatch', 'symbolset', 'symbolval', 'database',
    '=', '<>', '>', '<', '>=', '<=', '+', '-', '*', '/', 'and', 'or', 'not',
    'xor', 'bitand', 'bitor', 'bitxor', 'bitnot', 'shiftleft', 'shiftright',
    'in', 'user', 'debug', 'transcript',
    # user interface
    'welcome', 'message', 'working', 'complete', 'exit', 'abort', 'askbool',
    'askchoice', 'askoptions', 'askdir', 'askfile', 'asknumber', 'askstring',
    'askdisk',
    # actions
    'copyfiles', 'copylib', 'makedir', 'makeassign', 'delete', 'rename',
    'protect', 'run', 'execute', 'rexx', 'startup', 'textfile', 'tooltype',
    'iconinfo',
}
PARAMETERS = {
    'prompt', 'help', 'choices', 'default', 'source', 'dest', 'all',
    'pattern', 'files', 'infos', 'noposition', 'optional', 'confirm', 'safe',
    'quiet', 'noreq', 'append', 'include', 'command', 'setdefaulttool',
    'settooltype', 'newname', 'disk', 'assigns', 'override', 'nogauge',
    'delopts', 'back', 'fonts', 'swapcolors', 'range', 'newpath', 'resident',
}
QUESTIONS = {'askbool', 'askchoice', 'askoptions'}
# Built at packaging time and guarded by (exists ...) in the script.
OPTIONAL = {'Catalogs'}
SOURCE_ONLY = '--source-only' in sys.argv
BUILT_FILES = {'NovaDock', 'NovaDockPrefs', 'SaveConfig', 'MakeIcon', 'StartInstaller', 'SetupStartup'}


def tokenize(text):
    tokens, i, line = [], 0, 1
    while i < len(text):
        c = text[i]
        if c == '\n':
            line += 1
        if c in ' \t\r\n':
            i += 1
        elif c == ';':
            while i < len(text) and text[i] != '\n':
                i += 1
        elif c in '()':
            tokens.append((c, line))
            i += 1
        elif c == '"':
            j, value = i + 1, ''
            while True:
                assert j < len(text) and text[j] != '\n', \
                    f'line {line}: unterminated string'
                if text[j] == '\\':
                    value += text[j:j + 2]
                    j += 2
                    continue
                if text[j] == '"':
                    break
                value += text[j]
                j += 1
            tokens.append(('str', line, value))
            i = j + 1
        else:
            j = i
            while j < len(text) and text[j] not in ' \t\r\n();"':
                j += 1
            tokens.append(('sym', line, text[i:j]))
            i = j
    return tokens


def parse(tokens):
    stack, root = [], []
    for tok in tokens:
        if tok[0] == '(':
            node = {'line': tok[1], 'items': []}
            (stack[-1]['items'] if stack else root).append(node)
            stack.append(node)
        elif tok[0] == ')':
            assert stack, f'line {tok[1]}: unbalanced ")"'
            stack.pop()
        else:
            assert stack, f'line {tok[1]}: value outside an expression'
            stack[-1]['items'].append(tok)
    assert not stack, f'line {stack[-1]["line"]}: unclosed "("'
    return root


def walk(nodes):
    for node in nodes:
        if isinstance(node, dict):
            yield node
            yield from walk(node['items'])


def head(node):
    first = node['items'][0] if node['items'] else None
    return first[2].lower() if isinstance(first, tuple) and first[0] == 'sym' else None



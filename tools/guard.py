#!/usr/bin/env python3
"""Source/package policy checks. Static checks support review; they are not a sandbox."""
import argparse
import ast
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import zipfile

EXTENSIONS = {'.rpx','.rpl','.wud','.wux','.wua','.szs','.pack','.bfres','.sarc','.arc','.bfsar','.bfstm','.msbt','.bmg','.bti','.bdl','.bmd','.gcm','.iso','.sav'}
BINARIES = {'.elf','.o','.a','.so','.dll','.dylib','.exe','.zip','.gz','.xz','.7z','.rar','.png','.jpg','.jpeg','.gif','.webp','.bin','.wasm','.pyc'}
TEXT = {'.c','.h','.cpp','.hpp','.mm','.m','.py','.md','.txt','.json','.yml','.yaml','.patch','.gitignore'}
MAGIC = (b'Yaz0',b'SARC',b'RARC',b'FRES',b'FSAR',b'FSTM')
BINARY_MAGIC = (b'\x7fELF',b'MZ',b'PK\x03\x04',b'\x1f\x8b',b'7z\xbc\xaf\x27\x1c',b'\xfe\xed\xfa\xce',b'\xce\xfa\xed\xfe',b'\xfe\xed\xfa\xcf',b'\xcf\xfa\xed\xfe',b'\xca\xfe\xba\xbe')
FORBIDDEN = (b'NOT FOR '+b'PUBLICATION', b'gabi::'+b'call<')
PATTERNS = [re.compile(rb'WWHD_'+rb'FUNC\s*\(\s*0x'),re.compile(rb'VER'+rb'IFY\s*\(\s*0x')]
MAX_FILE = 2 * 1024 * 1024
MAX_TOTAL = 32 * 1024 * 1024
SETUP_MODULES = {'argparse','struct','zlib','math','json','hashlib','binascii','collections','itertools','functools','re','safe_io'}
DANGEROUS = {'exec','eval','compile','__import__','getattr','setattr','delattr','globals','locals','vars','open','breakpoint','input','help','memoryview'}


def load_release_guard(sdk):
    spec = importlib.util.spec_from_file_location('port_guard', sdk / 'tools/release/guard.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def check(name, data, release, source=False):
    problems=[]
    release.check_entry(name,data,problems)
    suffix=Path(name).suffix.lower()
    if suffix in EXTENSIONS: problems.append(name+': game payload extension')
    if data.startswith(MAGIC): problems.append(name+': game archive/resource magic')
    if any(marker in data for marker in FORBIDDEN) or any(pattern.search(data) for pattern in PATTERNS): problems.append(name+': non-public or recompiled material marker')
    if source:
        parts=Path(name).parts
        if any(part.startswith('.') and not (i==0 and part=='.github') for i,part in enumerate(parts)) and name!='.gitignore':
            problems.append(name+': hidden source path refused')
        if suffix in BINARIES or data.startswith(BINARY_MAGIC): problems.append(name+': compiled/archive/binary source refused')
        if suffix not in TEXT and Path(name).name not in {'LICENSE','COPYING','CODEOWNERS','.gitignore'}: problems.append(name+': unrecognised source type')
        if len(data)>MAX_FILE: problems.append(name+': source exceeds 2 MiB')
        try: text=data.decode('utf-8')
        except UnicodeDecodeError: problems.append(name+': source must be UTF-8 text')
        else:
            if any(ord(c)<32 and c not in '\t\n\r' for c in text): problems.append(name+': control byte in text source')
            # Long encoded payloads and minified lines need a readable source representation.
            if re.search(r'[A-Za-z0-9+/]{1024,}={0,2}',text) or re.search(r'(?:\\x[0-9a-fA-F]{2}){128,}',text): problems.append(name+': encoded blob refused')
            if re.search(r'(?:0x[0-9a-fA-F]{2}\s*,\s*){128,}',text): problems.append(name+': encoded byte array refused')
            if any(len(line)>4096 for line in text.splitlines()): problems.append(name+': minified/oversized source line')
    return problems


def setup_policy(name, data, helper=False):
    problems=[]
    try: tree=ast.parse(data,filename=name)
    except (SyntaxError,ValueError) as error: return [name+': invalid setup Python: '+str(error)]
    modules=SETUP_MODULES | ({'os','pathlib','sys','stat','tempfile'} if helper else set())
    for node in ast.walk(tree):
        if isinstance(node,(ast.Import,ast.ImportFrom)):
            names=[alias.name for alias in node.names] if isinstance(node,ast.Import) else [node.module or '']
            if isinstance(node,ast.ImportFrom) and node.level: problems.append(name+': relative setup imports refused')
            for module in names:
                if module.split('.')[0] not in modules: problems.append(name+': setup import refused: '+module)
        if isinstance(node,ast.Name) and (node.id in (DANGEROUS - ({'open'} if helper else set())) or (node.id.startswith('__') and node.id!='__name__')):
            problems.append(name+': dynamic or unsafe setup name: '+node.id)
        if isinstance(node,ast.Attribute):
            if node.attr.startswith('__'): problems.append(name+': dunder setup access refused')
            if node.attr in {'system','popen','spawn','execv','execve','fork','connect','urlopen'}: problems.append(name+': process/network effect refused: '+node.attr)
            if not helper and node.attr in {'open','write','write_bytes','write_text','mkdir','unlink','rename','replace','rmdir','remove','system','popen','load','loads','decompressobj','read_bytes','read_text','FileType'}:
                # json.loads and zlib decompression are ordinary parsing, not execution.
                if node.attr not in {'loads','decompressobj'}: problems.append(name+': direct setup effect refused: '+node.attr)
        if isinstance(node,(ast.Global,ast.Nonlocal)): problems.append(name+': setup global rebinding refused')
    return sorted(set(problems))


def setup_files(folder,manifest):
    result=set()
    for step in manifest.get('setup',[]):
        if step.get('type')!='run_tool': continue
        tool=step.get('tool',step.get('path',''))
        if not isinstance(tool,str) or not tool.endswith('.py') or '\\' in tool or ':' in tool or tool.startswith('/') or any(p in {'','..','.'} for p in tool.split('/')):
            raise ValueError('Unsafe setup tool path')
        result.add(tool)
    # All packaged setup helpers are checked, even if not reachable by today's imports.
    tools=folder/'tools'
    if tools.exists(): result.update(p.relative_to(folder).as_posix() for p in tools.rglob('*.py'))
    return sorted(result)


def scan(root,sdk,packages=False):
    release=load_release_guard(sdk)
    problems=[]
    if packages: paths=sorted(root.glob('*.zip'))+[root/'index.json']
    else:
        names=subprocess.check_output(['git','-C',str(root),'ls-files','-z','--cached','--others','--exclude-standard'])
        paths=[root/name.decode() for name in names.split(b'\0') if name]
    total=0
    for path in paths:
        name=path.relative_to(root).as_posix()
        if path.is_symlink(): problems.append(name+': symlink refused'); continue
        if not path.is_file(): problems.append(name+': non-regular file refused'); continue
        size=path.stat().st_size
        total+=size
        if not packages and size>MAX_FILE: problems.append(name+': source exceeds 2 MiB');continue
        if packages and size>256*1024*1024: problems.append(name+': package artifact exceeds 256 MiB');continue
        data=path.read_bytes()
        problems.extend(check(name,data,release,source=not packages))
        if packages and path.suffix.lower()=='.zip':
            with zipfile.ZipFile(path) as archive:
                if len(archive.infolist())>4096 or sum(info.file_size for info in archive.infolist())>512*1024*1024:
                    problems.append(name+': expanded package inventory exceeds limits');continue
                for info in archive.infolist():
                    if info.is_dir(): continue
                    if info.file_size>128*1024*1024: problems.append(info.filename+': oversized package file');continue
                    if (info.external_attr>>16)&0o170000==0o120000: problems.append(info.filename+': package symlink refused')
                    problems.extend(check(name+'/'+info.filename,archive.read(info),release))
    if not packages:
        if total>MAX_TOTAL: problems.append('source inventory exceeds 32 MiB')
        for manifest_path in sorted(root.glob('*/manifest.json')):
            folder=manifest_path.parent
            manifest=json.loads(manifest_path.read_text())
            for required in ('README.md','LICENSE'):
                if not (folder/required).is_file(): problems.append(folder.name+': missing '+required)
            try:
                for name in setup_files(folder,manifest):
                    path=folder/name
                    if path.is_symlink() or not path.is_file(): problems.append(str(path)+': missing/symlink setup tool');continue
                    problems.extend(setup_policy(folder.name+'/'+name,path.read_text(),helper=folder.name=='gc-minimap' and name=='tools/safe_io.py'))
            except ValueError as error: problems.append(folder.name+': '+str(error))
    return problems


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root',type=Path)
    parser.add_argument('--sdk',type=Path,required=True)
    parser.add_argument('--packages',action='store_true')
    parser.add_argument('--pr-base', help='Trusted base revision; protected generated/policy files may not change in PRs')
    args=parser.parse_args()
    problems=scan(args.root.resolve(),args.sdk.resolve(),args.packages)
    if args.pr_base:
        changed=subprocess.check_output(['git','-C',str(args.root),'diff','--name-only',args.pr_base,'HEAD'],text=True).splitlines()
        protected={'index.json','sdk.json','gc-minimap/tools/safe_io.py'}
        for name in changed:
            if name in protected or name.startswith('.github/workflows/') or name in {'tools/guard.py','.github/CODEOWNERS'}:
                problems.append(name+': protected policy/generated file; maintainer-reviewed integration required')
    for problem in problems: print(problem)
    raise SystemExit(bool(problems))

#!/usr/bin/env python3
# 무한대전 데이터 파일(32 비트 리눅스에서 구조체를 그대로 저장한 것)을
# 64 비트에서 읽고 쓰기 위한 변환 코드를 만든다.
#
#   python3 gen_disk32.py ../game/src/mstruct.h ../game/src
#   -> disk32.h, disk32.c
#
# - 디스크 쪽 구조체(d_*)는 원래 구조체와 필드 순서가 같고
#   long -> int32_t, 포인터 -> uint32_t 로 바꾼 것 (i386 과 같은 배치가 된다)
# - 읽을 때 포인터 필드는 NULL, 쓸 때는 0 (원래 코드도 읽은 뒤 포인터를 모두 0 으로 되돌린다)
import re, sys

SRC = sys.argv[1]
OUT = sys.argv[2]
STRUCTS = ['lasttime', 'daily', 'exit_', 'object', 'room', 'creature']

text = open(SRC, 'rb').read().decode('cp949', 'replace')
text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)


def body_of(name):
	m = re.search(r'typedef\s+struct\s+' + re.escape(name) + r'\s*\{(.*?)\}\s*' + re.escape(name) + r'\s*;', text, re.S)
	if not m:
		raise SystemExit('struct not found: ' + name)
	return m.group(1)


def parse(name):
	fields = []
	for line in body_of(name).split('\n'):
		line = line.strip()
		if not line or line.startswith('#'):
			continue
		for decl in line.rstrip(';').split(';'):
			decl = decl.strip()
			if not decl:
				continue
			m = re.match(r'^(unsigned\s+char|struct\s+\w+|\w+)\s*(\*?)\s*(\w+)((?:\[\w+\])*)$', decl)
			if not m:
				raise SystemExit('cannot parse in %s: %r' % (name, decl))
			typ, ptr, fname, dims = m.groups()
			typ = re.sub(r'\s+', ' ', typ)
			dims = re.findall(r'\[(\w+)\]', dims)
			fields.append((typ, ptr == '*' or typ in ('otag', 'ctag', 'xtag', 'etag', 'ttag') and ptr == '*', fname, dims))
	return fields


def kind(typ, is_ptr):
	if is_ptr:
		return 'ptr'
	if typ in ('char', 'unsigned char', 'short'):
		return 'raw'
	if typ in ('long', 'int'):
		return 'long' if typ == 'long' else 'raw'
	if typ.startswith('struct '):
		sub = typ.split()[1]
		if sub in STRUCTS:
			return 'sub:' + sub
	raise SystemExit('unknown type %s' % typ)


def disk_type(typ, k):
	if k == 'ptr':
		return 'uint32_t'
	if k == 'long':
		return 'int32_t'
	if k.startswith('sub:'):
		return 'd_' + k[4:]
	return typ


h = ['/* 자동 생성: tools/gen_disk32.py (손으로 고치지 말 것) */',
     '#ifndef DISK32_H', '#define DISK32_H', '#include <stdint.h>', '/* mstruct.h 다음에 포함할 것 (mstruct.h 에는 중복 포함 방지가 없다) */', '']
c = ['/* 자동 생성: tools/gen_disk32.py (손으로 고치지 말 것) */',
     '#include <string.h>', '#include <unistd.h>', '#include "mstruct.h"', '#include "disk32.h"', '']
proto = []

for s in STRUCTS:
	fields = parse(s)
	h.append('typedef struct d_%s {' % s)
	for typ, is_ptr, fname, dims in fields:
		k = kind(typ, is_ptr)
		h.append('\t%s %s%s;' % (disk_type(typ, k), fname, ''.join('[%s]' % d for d in dims)))
	h.append('} d_%s;' % s)
	h.append('')

	for direction in ('from', 'to'):
		if direction == 'from':
			sig = 'void %s_from_disk(%s *m, const d_%s *d)' % (s, s, s)
		else:
			sig = 'void %s_to_disk(d_%s *d, const %s *m)' % (s, s, s)
		proto.append(sig + ';')
		c.append(sig)
		c.append('{')
		c.append('\tint i, j;' if any(f[3] for f in fields) else '')
		for typ, is_ptr, fname, dims in fields:
			k = kind(typ, is_ptr)
			if k == 'raw':
				c.append('\tmemcpy(%s->%s, %s->%s, sizeof(d->%s));' % (
					('m', fname, 'd', fname, fname) if direction == 'from' else ('d', fname, 'm', fname, fname))
					if dims else
					('\tm->%s = d->%s;' % (fname, fname) if direction == 'from' else '\td->%s = m->%s;' % (fname, fname)))
				continue
			# 원소 하나에 대한 문장
			def one(idx):
				if k == 'ptr':
					return 'm->%s%s = 0;' % (fname, idx) if direction == 'from' else 'd->%s%s = 0;' % (fname, idx)
				if k == 'long':
					return ('m->%s%s = (long)d->%s%s;' % (fname, idx, fname, idx) if direction == 'from'
					        else 'd->%s%s = (int32_t)m->%s%s;' % (fname, idx, fname, idx))
				sub = k[4:]
				return ('%s_from_disk(&m->%s%s, &d->%s%s);' % (sub, fname, idx, fname, idx) if direction == 'from'
				        else '%s_to_disk(&d->%s%s, &m->%s%s);' % (sub, fname, idx, fname, idx))
			if not dims:
				c.append('\t' + one(''))
			elif len(dims) == 1:
				c.append('\tfor (i = 0; i < %s; i++) %s' % (dims[0], one('[i]')))
			else:
				c.append('\tfor (i = 0; i < %s; i++) for (j = 0; j < %s; j++) %s' % (dims[0], dims[1], one('[i][j]')))
		c.append('}')
		c.append('')

# 파일/메모리 읽기 쓰기 도우미
for s in ('object', 'creature', 'room', 'exit_'):
	sig_r = 'int disk_read_%s(int fd, %s *m)' % (s, s)
	sig_w = 'int disk_write_%s(int fd, const %s *m)' % (s, s)
	sig_mr = 'int mem_read_%s(const char *buf, %s *m)' % (s, s)
	sig_mw = 'int mem_write_%s(char *buf, const %s *m)' % (s, s)
	proto += [sig_r + ';', sig_w + ';', sig_mr + ';', sig_mw + ';']
	c += [
		'/* 파일에서 하나 읽기: 성공하면 1 */',
		sig_r, '{', '\td_%s d;' % s,
		'\tif (read(fd, &d, sizeof(d)) != (ssize_t)sizeof(d)) { memset(m, 0, sizeof(*m)); return 0; }',
		'\t%s_from_disk(m, &d);' % s, '\treturn 1;', '}', '',
		'/* 파일에 하나 쓰기: 성공하면 1 */',
		sig_w, '{', '\td_%s d;' % s, '\tmemset(&d, 0, sizeof(d));',
		'\t%s_to_disk(&d, m);' % s,
		'\treturn write(fd, &d, sizeof(d)) == (ssize_t)sizeof(d);', '}', '',
		'/* 메모리에서 하나 읽기: 읽은 바이트 수 */',
		sig_mr, '{', '\td_%s d;' % s, '\tmemcpy(&d, buf, sizeof(d));',
		'\t%s_from_disk(m, &d);' % s, '\treturn sizeof(d);', '}', '',
		'/* 메모리에 하나 쓰기: 쓴 바이트 수 */',
		sig_mw, '{', '\td_%s d;' % s, '\tmemset(&d, 0, sizeof(d));',
		'\t%s_to_disk(&d, m);' % s, '\tmemcpy(buf, &d, sizeof(d));', '\treturn sizeof(d);', '}', '',
	]

# 배치 검사 프로그램: -m32 로 원래 구조체, 64 비트로 d_ 구조체를 컴파일해 출력을 비교한다
chk = ['#include <stdio.h>', '#include <stddef.h>', '#include "mstruct.h"',
       '#ifdef DISK', '#include "disk32.h"', '#define T(s) d_##s', '#else', '#define T(s) s', '#endif',
       '#define F(s, f) printf(#s "." #f " %u %u\\n", (unsigned)offsetof(T(s), f), (unsigned)sizeof(((T(s) *)0)->f))',
       'int main(void)', '{']
for s in STRUCTS:
	chk.append('\tprintf("%s %%u\\n", (unsigned)sizeof(T(%s)));' % (s, s))
	for typ, is_ptr, fname, dims in parse(s):
		chk.append('\tF(%s, %s);' % (s, fname))
chk += ['\treturn 0;', '}', '']
open(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), 'layout_check.c'), 'w', encoding='cp949', newline='\n').write('\n'.join(chk))

h += proto
h += ['', '#endif', '']
open(OUT + '/disk32.h', 'w', encoding='cp949', newline='\n').write('\n'.join(h))
open(OUT + '/disk32.c', 'w', encoding='cp949', newline='\n').write('\n'.join(x for x in c if x is not None))
print('ok: disk32.h, disk32.c')

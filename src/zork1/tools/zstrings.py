# Z-machine v3 게임 파일에서 화면에 나오는 문장을 모두 뽑는다 (번역표 만들기용)
#   python3 zstrings.py zork1.z3 > strings.txt
# 루틴은 갈 수 있는 길(분기, 점프)을 따라 푼다. 시작점은 첫 루틴과 call 대상, 그리고 메모리 안의
# 루틴 주소처럼 보이는 값(물건 속성의 ACTION 등). 코드가 끝난 뒤는 문장 모음이다.
import sys, struct

mem = open(sys.argv[1], 'rb').read()
w = lambda a: struct.unpack('>H', mem[a:a+2])[0]
assert mem[0] == 3
HIGH = w(4); PC0 = w(6); OBJ = w(0x0A); ABBR = w(0x18); LEN = w(0x1A) * 2

A0 = 'abcdefghijklmnopqrstuvwxyz'
A1 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
A2 = ' \n0123456789.,!?_#\'"/\\-:()'

def zdecode(a, abbr_ok=True):
	zs = []
	while True:
		if a + 1 >= LEN: raise ValueError('eof')
		x = w(a); a += 2
		zs += [(x >> 10) & 31, (x >> 5) & 31, x & 31]
		if x & 0x8000: break
	out = []; alpha = 0; i = 0
	while i < len(zs):
		c = zs[i]; i += 1
		if c == 0: out.append(' '); alpha = 0; continue
		if c in (1, 2, 3):
			if not abbr_ok: raise ValueError('nested abbr')
			if i < len(zs):
				idx = 32 * (c - 1) + zs[i]; i += 1
				out.append(zdecode(w(ABBR + idx * 2) * 2, False)[0])
			alpha = 0; continue
		if c == 4: alpha = 1; continue
		if c == 5: alpha = 2; continue
		if alpha == 2 and c == 6:
			if i + 1 < len(zs):
				out.append(chr((zs[i] << 5) | zs[i+1])); i += 2
			alpha = 0; continue
		if alpha == 2 and c == 7: out.append('\n'); alpha = 0; continue
		out.append((A0, A1, A2)[alpha][c - 6]); alpha = 0
	return ''.join(out), a

# 물건 이름
names = {}
o = 1
first_pt = LEN
while True:
	e = OBJ + 62 + (o - 1) * 9
	if e >= first_pt or o > 255: break
	pt = w(e + 7)
	first_pt = min(first_pt, pt)
	names[o] = zdecode(pt + 1)[0] if mem[pt] else ''
	o += 1

OP2 = {1:'b',2:'b',3:'b',4:'b',5:'b',6:'b',7:'b',8:'s',9:'s',10:'b',11:'',12:'',13:'',14:'',15:'s',16:'s',17:'s',18:'s',19:'s',20:'s',21:'s',22:'s',23:'s',24:'s'}
OP1 = {0:'b',1:'sb',2:'sb',3:'s',4:'s',5:'',6:'',7:'',9:'',10:'',11:'R',12:'J',13:'',14:'s',15:'s'}
OP0 = {0:'R',1:'R',2:'t',3:'tR',4:'',5:'b',6:'b',7:'R',8:'R',9:'',10:'R',11:'',12:'',13:'b'}
OPV = {0:'s',1:'',2:'',3:'',4:'',5:'',6:'',7:'s',8:'',9:'',10:'',11:'',19:'',20:'',21:''}

def decode(a):
	"""명령 하나: (다음 주소, 분기 대상들, 끝인지, 문장, call 대상, print_paddr 대상)"""
	b = mem[a]; a += 1
	if b < 0x80:
		op = b & 31; kind = OP2.get(op); types = [2 if b & 0x40 else 1, 2 if b & 0x20 else 1]; form = '2'
	elif b < 0xC0:
		t = (b >> 4) & 3; op = b & 15
		if t == 3: kind = OP0.get(op); types = []; form = '0'
		else: kind = OP1.get(op); types = [t]; form = '1'
	else:
		op = b & 31; form = 'V' if b & 0x20 else '2'
		kind = (OPV if form == 'V' else OP2).get(op)
		tb = mem[a]; a += 1; types = []
		for sh in (6, 4, 2, 0):
			t = (tb >> sh) & 3
			if t == 3: break
			types.append(t)
	if kind is None: raise ValueError('bad opcode %x at %x' % (b, a - 1))
	vals = []
	for t in types:
		if t == 0: vals.append(('c', w(a))); a += 2
		elif t == 1: vals.append(('c', mem[a])); a += 1
		else: vals.append(('v', mem[a])); a += 1
	targets = []; end = False; text = None; call = None; pstr = None
	if 's' in kind: a += 1
	if 'b' in kind:
		bb = mem[a]; a += 1
		if bb & 0x40: off = bb & 0x3f
		else:
			off = ((bb & 0x3f) << 8) | mem[a]; a += 1
			if off & 0x2000: off -= 0x4000
		if off not in (0, 1): targets.append(a + off - 2)
	if 't' in kind: text, a = zdecode(a)
	if kind == 'J':
		off = vals[0][1] if vals[0][0] == 'c' else 0
		if off & 0x8000: off -= 0x10000
		targets.append(a + off - 2); end = True
	if 'R' in kind: end = True
	if form == 'V' and op == 0 and vals and vals[0][0] == 'c' and vals[0][1]: call = vals[0][1] * 2
	if form == '1' and op == 13 and vals[0][0] == 'c': pstr = vals[0][1] * 2
	consts = [x[1] * 2 for x in vals if x[0] == 'c' and x[1] >= 0x100]
	return a, targets, end, text, call, pstr, consts

routines = {}     # 시작 -> 끝
maybe = set()     # 상수 피연산자로 나온 루틴 주소 후보 (QUEUE 로 예약하는 루틴 등)
texts = []
pstrs = set()

def do_routine(r, strict=False):
	"""r 의 루틴을 갈 수 있는 길을 따라 푼다. 잘 풀리면 (끝, 문장들, call 들, paddr 들)"""
	if r in routines: return True
	nl = mem[r]
	if nl > 15: return None
	start = r + 1 + nl * 2
	todo = [start]; seen = set(); ts = []; calls = []; ps = []; hi = start
	while todo:
		a = todo.pop()
		while a not in seen:
			if not (HIGH <= a < LEN): return None
			seen.add(a)
			try: n, tg, end, text, call, pstr, consts = decode(a)
			except Exception: return None
			if text is not None: ts.append((a, text))
			if call: calls.append(call)
			for c in consts:
				if HIGH <= c < LEN: maybe.add(c)
			if pstr: ps.append(pstr)
			todo += tg
			hi = max(hi, n)
			if end: break
			a = n
	return hi, ts, calls, ps

def add(r, strict):
	res = do_routine(r)
	if res is None or res is True: return res is True
	hi, ts, calls, ps = res
	routines[r] = hi; texts.extend(ts); pstrs.update(ps)
	for c in calls:
		add(c, True)
	return True

add(PC0 - 1, True)
# 메모리 안에서 루틴 주소처럼 보이는 값 (속성의 ACTION, 표의 루틴들)
code_lo = min(routines); code_hi = max(routines.values())
changed = True
while changed:
	changed = False
	# 루틴 주소는 자료 영역(코드 앞: 물건 속성, 동사 표 등)에만 들어 있다
	for i in range(0, code_lo - 1):
		p = w(i) * 2
		if code_lo - 0x2000 <= p < code_hi + 0x2000 and HIGH <= p < LEN and p not in routines and mem[p] <= 15:
			before = len(routines)
			res = do_routine(p)
			if res and res is not True:
				# 다른 루틴 한가운데가 아닌지: 이미 아는 루틴 범위 안이면 버린다
				inside = any(s < p < e for s, e in routines.items())
				if not inside:
					add(p, False)
					if len(routines) > before: changed = True
	code_lo = min(routines); code_hi = max(routines.values())

# 코드 끝: 루틴 사이가 크게 벌어지는 곳 (그 뒤에 루틴처럼 보인 것은 문장 속을 잘못 짚은 것)
rs = sorted(routines.items())
code_end = rs[0][1]
for st, en in rs:
	if st - code_end > 0x400: break
	code_end = max(code_end, en)
for r in [r for r in routines if r >= code_end]:
	del routines[r]
# 상수 피연산자로 나온 루틴 후보 (다른 루틴 한가운데가 아니고 코드 안이면)
for _ in range(5):
	before = len(routines)
	for c in sorted(maybe):
		if c in routines or not (min(routines) <= c < code_end) or mem[c] > 15: continue
		if any(s0 < c < e0 for s0, e0 in routines.items()): continue
		add(c, False)
	for r in [r for r in routines if r >= code_end]:
		del routines[r]
	if len(routines) == before: break
# 루틴 사이의 빈 곳: 찾지 못한 루틴 (변수로 부르는 루틴 등). 차례로 풀어 본다
for _ in range(3):
	rs = sorted(routines.items())
	for (s1, e1), (s2, e2) in zip(rs, rs[1:]):
		a = e1 + (e1 & 1)
		while a < s2:
			res = do_routine(a)
			if not res or res is True: break
			hi, ts, calls, ps = res
			if hi > s2: break
			routines[a] = hi; texts.extend(ts)
			a = hi + (hi & 1)
texts = [(x, t) for (x, t) in texts if x < code_end]
code_end += code_end & 1
# 코드 뒤는 문장 모음
a = code_end
nstr = 0
while a + 1 < LEN:
	try: s, e = zdecode(a)
	except Exception: break
	texts.append((a, s)); nstr += 1
	a = e + (e & 1)

def esc(s): return s.replace('\\', '\\\\').replace('\n', '\\n')
seen = set(); out = []
for o in sorted(names):
	if names[o] and names[o] not in seen: seen.add(names[o]); out.append(names[o])
for addr, s in sorted(texts):
	if s not in seen: seen.add(s); out.append(s)
sys.stderr.write('routines %d (%x-%x), strings after code %d, objects %d, unique %d, chars %d\n' %
	(len(routines), min(routines), code_end, nstr, len(names), len(out), sum(len(s) for s in out)))
for s in out:
	print('@@ ' + esc(s))
	print('== ')

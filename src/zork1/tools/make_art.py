# 조크 I 장면 그림 만들기: 색 글자 도트 그림 -> 이야기 안시 (완성형)
#   한 점 = 2 칸 (배경색 칸 두 개). 색: . 바탕(파랑, BBS 기본) k 검정 b 파랑 g 초록 c 청록 r 빨강 m 자주 y 갈색 w 회색
#   배경색은 0~7 만 쓴다 (밝은 배경은 이야기에서 깜빡임으로 보일 수 있음). 끝나면 ESC[=1G 로 BBS 바탕에 돌려놓는다.
#   python3 zk_art.py <출력 폴더>
import sys, os
OUT = sys.argv[1] if len(sys.argv) > 1 else '.'
COL = {'k': 0, 'b': 1, 'g': 2, 'c': 3, 'r': 4, 'm': 5, 'y': 6, 'w': 7, '.': 1}
W = 32

H_OUT, W_OUT = 7, 26		# 화면에 낼 크기 (줄, 점). 원래 도트 그림은 10 줄 x 32 점

def shrink(rows, h, w):
	"""옆 줄(칸)과 가장 비슷한 줄(칸)부터 빼서 h x w 로 줄인다 (그림 모양을 덜 망가뜨린다)"""
	rows = [(r + '.' * W)[:W] for r in rows]
	# 그림에 드물게 쓰인 색(눈, 불꽃 같은 작은 부분)이 달라지는 줄은 빼기 아깝게 무게를 준다
	allc = ''.join(rows)
	rare = dict((c, 1 + 6 * (1 - allc.count(c) / float(len(allc)))) for c in set(allc))
	diff = lambda a, b: sum(max(rare[x], rare[y]) for x, y in zip(a, b) if x != y)
	def cut(lines, n):
		lines = list(lines)
		while len(lines) > n:
			best = min(range(1, len(lines)), key=lambda i: (diff(lines[i - 1], lines[i]), -i))
			del lines[best]
		return lines
	rows = cut(rows, h)
	cols = cut([''.join(r[i] for r in rows) for i in range(W)], w)
	return [''.join(c[j] for c in cols) for j in range(len(rows))]

def render(rows, small=False):
	out = []
	if not small: rows = shrink(rows, H_OUT, W_OUT)
	for r in rows:
		line = ' ' * 13
		cur = None
		for ch in r:
			c = COL.get(ch, 1)
			if c != cur:
				line += '\x1b[=%dG' % c
				cur = c
			line += '  '
		line += '\x1b[=1G'
		out.append(line.rstrip() if False else line)
	return '\r\n'.join(out) + '\r\n\x1b[=15F\x1b[=1G'

ART = {}

ART['West of House'] = [
	'................................',
	'...........rrrrrrrrrr...........',
	'.........rrrrrrrrrrrrrr.........',
	'.......rrrrrrrrrrrrrrrrrr.......',
	'........wwwwwwwwwwwwwwww........',
	'........wwkykywwwwwkykyw........',
	'........wwykykwwwwwykykw...ww...',
	'........wwkykywwwwwwwwww...kk...',
	'ggggggggwwykykwwwwwwwwwwgggyggggg',
	'ggggggggggggggggggggggggggyggggg',
]
ART['Living Room'] = [
	'wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww',
	'wyyyyyw......wwwwwwwwwwwwwwwwwww',
	'wykkkyw..........wwwwwwwwwwwwwww',
	'wykkkyw.....yyyyyyyyyyyy.wwwwwww',
	'wykkkyw.....ycccccccccy..wwwwwww',
	'wykkkyw.....yccyccyccy...wwwwwww',
	'wykkkyw.....yyyyyyyyyyy..wwwwwww',
	'yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy',
	'yyyyrrmmrrmmrrmmrrmmrrmmrryyyyyy',
	'yyyyrrrrrrrrrrrrrrrrrrrrrryyyyyy',
]
ART['Cellar'] = [
	'kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk',
	'kkkkkkkkkkkkkkkkkkkkkkkkkkyyyyyk',
	'kkkkkkkkkkkkkkkkkkkkkkkyyykkkkkk',
	'kkkkkkkkkkkkkkkkkkkkyyykkkkkkkkk',
	'kwwkkkkkkkkkkkkkkyyykkkkkkkkkkkk',
	'kwwkkkkkkkkkkkyyykkkkkkkkkkkkkkk',
	'kwwkkkkkkkkyyykkkkkkkkkkkkkkkkkk',
	'kwwkkkkkyyykkkkkkkkkkkkkkkkkkkkk',
	'kwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwk',
	'kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk',
]
ART['The Troll Room'] = [
	'kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk',
	'kkkkkkkkkkkkggggggkkkkkkkwwwkkkk',
	'kkkkkkkkkkkggrggrggkkkkkwwwwwkkk',
	'kkkkkkkkkkkgggggggggkkkkkwyykkkk',
	'kkkkkkkkkkkkgwwwwgkkkkkkkkykkkkk',
	'kkkkkkkkkgggggggggggggggggykkkkk',
	'kkkkkkkkgggkgggggggkkkkkkkykkkkk',
	'kkkkkkkkggkkgggggggkkkkkkkykkkkk',
	'kkkkkkkkkkkkggkkkggkkkkkkkkkkkkk',
	'kkkkkkkkkkkgggkkkgggkkkkkkkkkkkk',
]
ART['Forest'] = [
	'................................',
	'....g.........g.........g.......',
	'...ggg.......ggg.......ggg....g.',
	'..ggggg.....ggggg.....ggggg..ggg',
	'.ggggggg...ggggggg...ggggggggggg',
	'ggggggggg.ggggggggg.ggggggggggg.',
	'....y.........y.........y....y..',
	'....y.........y.........y....y..',
	'gggggggggggggggggggggggggggggggg',
	'gggggggggggggggggggggggggggggggg',
]
ART['Forest Path'] = ART['Forest']
ART['Clearing'] = [
	'................................',
	'..g..........................g..',
	'.ggg.......................ggg..',
	'ggggg.....................ggggg.',
	'ggggggg.................ggggggg.',
	'..y.............................',
	'..y........yyyyyyyy.........y...',
	'gggggggggggykykykyyggggggggggggg',
	'gggggggggggyyyyyyyyggggggggggggg',
	'gggggggggggggggggggggggggggggggg',
]
ART['Up a Tree'] = [
	'....ggggggg.........ggggggg.....',
	'..ggggggggggg.....ggggggggggg...',
	'.ggggggggggggg...ggggggggggggg..',
	'ggggggggggggggggggggggggggggggg.',
	'yyyyyyyyyyyyyyyyyyyyyyyy........',
	'.........y.ywwwy.y..............',
	'.........yyycccyyy..............',
	'..........yyyyyyy...............',
	'..............yyyy..............',
	'..............yyyy..............',
]
ART['Dam'] = [
	'................................',
	'................................',
	'cccccccccccwwwwwwwwwwwwwwwwwwwww',
	'cccccccccccwwkwwkwwkwwkwwkwwkwww',
	'cccccccccccwwwwwwwwwwwwwwwwwwwww',
	'cccccccccccwwwwwwwwwwwwwwwwwwwww',
	'cccccccccccwwwwwwwwwwwwwwwwwwwww',
	'cccccccccccwwwwwwwwwwwwwwwwwwwww',
	'cccccccccccwwwwwwwwwwwwwwwwwwwww',
	'yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy',
]
ART['Aragain Falls'] = [
	'...........rrrrrrrrrr...........',
	'........rrryyyyyyyyyyrrr........',
	'......rryyygggggggggyyyrr.......',
	'.....ryygggccccccccgggyyr.......',
	'....rygccmm........mmccgyr......',
	'....rygcm............mcgyr......',
	'wwwwrygcm..cccc......mcgyrwwwwww',
	'wwwwwwwww..cccc......wwwwwwwwwww',
	'wwwwwwwww..cwcw.....wwwwwwwwwwww',
	'ccccccccccccwccccccccccccccccccc',
]
ART['End of Rainbow'] = ART['Aragain Falls']
ART['On the Rainbow'] = [
	'................................',
	'rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr',
	'yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy',
	'gggggggggggggggggggggggggggggggg',
	'cccccccccccccccccccccccccccccccc',
	'mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm',
	'................................',
	'.........cccc...................',
	'.........cwcc...................',
	'cccccccccccccccccccccccccccccccc',
]
ART['Cyclops Room'] = [
	'kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk',
	'kkkkkkkkkkyyyyyyyyyyyykkkkkkkkkk',
	'kkkkkkkkkyyyyyyyyyyyyyykkkkkkkkk',
	'kkkkkkkkyyyyywwwwwwyyyyykkkkkkkk',
	'kkkkkkkkyyyyywwkkwwyyyyykkkkkkkk',
	'kkkkkkkkyyyyywwwwwwyyyyykkkkkkkk',
	'kkkkkkkkyyyyyyyyyyyyyyyykkkkkkkk',
	'kkkkkkkkkyyyrrrrrrrryyykkkkkkkkk',
	'kkkkkkkkkkyywwkwwkwyyykkkkkkkkkk',
	'kkkkkkkkkkkyyyyyyyyyykkkkkkkkkkk',
]
ART['Altar'] = [
	'kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk',
	'kkkkkkkkkrkkkkkkkkkkkkrkkkkkkkkk',
	'kkkkkkkkkykkkkkkkkkkkkykkkkkkkkk',
	'kkkkkkkkkwkkkkkkkkkkkkwkkkkkkkkk',
	'kkkkkkkkkwkkkkkyykkkkkwkkkkkkkkk',
	'kkkkkkwwwwwwwwwwwwwwwwwwwwkkkkkk',
	'kkkkkkkwwwwwwwwwwwwwwwwwwkkkkkkk',
	'kkkkkkkkwwwkkkkkkkkkkwwwkkkkkkkk',
	'kkkkkkkkwwwkkkkkkkkkkwwwkkkkkkkk',
	'wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww',
]
ART['Temple'] = ART['Altar']
ART['Entrance to Hades'] = [
	'kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk',
	'kkkkkkwkkkwkkkwkkkwkkkwkkkwkkkkk',
	'kkkkkkwkkkwkkkwkkkwkkkwkkkwkkkkk',
	'kkkkkkwkwwwkkkwkwwkkkkwkkkwkkkkk',
	'kkkkkkwkwkwkkkwwkwwkkkwkkkwkkkkk',
	'kkkkkkwkwwwkkkwkwwkkkkwkwwwkkkkk',
	'kkkkkkwkkkwkkkwkkkwkkkwkwkwkkkkk',
	'kkkkkkwkkkwkkkwkkkwkkkwkkwwkkkkk',
	'kkkkkkwkkkwkkkwkkkwkkkwkkkwkkkkk',
	'rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr',
]
ART['Maze'] = [
	'wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww',
	'wkkkkkkwkkkkkkkkkkkwkkkkkkkkkkkw',
	'wkwwwwkwkwwwwwwwwwkwkwwwwwwwwwkw',
	'wkwkkkkkkwkkkkkkkwkkkwkkkkkkkwkw',
	'wkwkwwwwwwkwwwwwkwwwwwkwwwwwkwkw',
	'wkkkwkkkkkkwkkkwkkkkkkkwkkkkkkkw',
	'wwwkwkwwwwwwkwkwwwwwwwkwkwwwwwww',
	'wkkkkkwkkkkkkwkkkkkkkwkkkwkkkkkw',
	'wkwwwwwkwwwwwwwwwwwwkwwwkwwwwwkw',
	'wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww',
]
ART['Treasure Room'] = [
	'kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk',
	'kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk',
	'kkkkkkkkkkyyyyyyyyyyyykkkkkkkkkk',
	'kkkkkkkkkyykykykykykyyykkkkkkkkk',
	'kkkkkkkkkyrycygyrycygyykkkkkkkkk',
	'kkkkkkkkkyyyyyyyyyyyyyykkkkkkkkk',
	'kkkkkkkkkyyyyyykkyyyyyykkkkkkkkk',
	'kkkkkkkkkyyyyyyyyyyyyyykkkkkkkkk',
	'kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk',
	'wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww',
]
ART['Gallery'] = [
	'wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww',
	'wwwwwwwwyyyyyyyyyyyyyyyywwwwwwww',
	'wwwwwwwwy..............ywwwwwwww',
	'wwwwwwwwy..yy.....rr...ywwwwwwww',
	'wwwwwwwwy.yyyy...rrrr..ywwwwwwww',
	'wwwwwwwwyggggggggggggggywwwwwwww',
	'wwwwwwwwyggggggcgggggggywwwwwwww',
	'wwwwwwwwyyyyyyyyyyyyyyyywwwwwwww',
	'wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww',
	'yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy',
]
ART['Machine Room'] = [
	'kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk',
	'kkkkkkkkwwwwwwwwwwwwwwwwkkkkkkkk',
	'kkkkkkkwwwwwwwwwwwwwwwwwwkkkkkkk',
	'kkkkkkkwwkkkkkkkkkkkkkkwwkkkkkkk',
	'kkkkkkkwwkkkkkkkkkkkkkkwwkkkkkkk',
	'kkkkkkkwwwwwwwwwwwwwwwwwwkkkkkkk',
	'kkkkkkkwwwwrwwwwwwwwgwwwwkkkkkkk',
	'kkkkkkkwwwwwwwwwwwwwwwwwwkkkkkkk',
	'kkkkkkkkwwkkkkkkkkkkkkwwkkkkkkkk',
	'wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww',
]
ART['Egyptian Room'] = [
	'kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk',
	'kkkkkkkkkkkkkyyyyyykkkkkkkkkkkkk',
	'kkkkkkkkkkkyyyyyyyyyykkkkkkkkkkk',
	'kkkkkkkkkkkyykyyyykyykkkkkkkkkkk',
	'kkkkkkkkkkkyyyyrryyyykkkkkkkkkkk',
	'kkkkkkkkkkkycyccccycykkkkkkkkkkk',
	'kkkkkkkkkkkyyyyyyyyyykkkkkkkkkkk',
	'kkkkkkkkkkkycyccccycykkkkkkkkkkk',
	'kkkkkkkkkkkkyyyyyyyykkkkkkkkkkkk',
	'yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy',
]
ART['Atlantis Room'] = [
	'cccccccccccccccccccccccccccccccc',
	'cccccccccccccyycyycyyccccccccccc',
	'cccccccccccccyycyycyyccccccccccc',
	'cccccccccccccyyyyyyyyccccccccccc',
	'cccccccccccccccyyyyccccccccccccc',
	'cccccccccccccccyyyyccccccccccccc',
	'cccccccccccccccyyyyccccccccccccc',
	'cccccccccccccccyyyyccccccccccccc',
	'wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww',
	'yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy',
]
ART['Kitchen'] = [
	'wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww',
	'wwwwwwwwwwwwwwwwwwwwwww....wwwww',
	'wwwwwwwwwwwwwwwwwwwwwww....wwwww',
	'wwwwwwwwwwcwwwwwwwwwwwwwwwwwwwww',
	'wwwwwwwwwwcwwwyyyywwwwwwwwwwwwww',
	'wwwwwwwwwwcwwyyyyyywwwwwwwwwwwww',
	'wwwwwyyyyyyyyyyyyyyyyyyyywwwwwww',
	'wwwwwwywwwwwwwwwwwwwwwwywwwwwwww',
	'wwwwwwywwwwwwwwwwwwwwwwywwwwwwww',
	'yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy',
]

# 자동으로 줄이면 작은 부분(눈, 지붕)이 빠지는 그림은 작은 크기(7 줄 x 26 점)로 따로 그린다
SMALL = {}
SMALL['West of House'] = [
	'..........................',
	'........rrrrrrrrrr........',
	'......rrrrrrrrrrrrrr......',
	'.......wwwwwwwwwwww.......',
	'.......wykykwwwkykw...ww..',
	'gggggggwkykywwwwwwwgggkkgg',
	'ggggggggggggggggggggggyggg',
]
SMALL['The Troll Room'] = [
	'kkkkkkkkkkkkkkkkkkkkkkkkkk',
	'kkkkkkkkkggggggkkkkkkwwwkk',
	'kkkkkkkkggrggrggkkkkwwwwwk',
	'kkkkkkkkkgwwwwgkkkkkkykkkk',
	'kkkkkkggggggggggggggggykkk',
	'kkkkkggkkggggggkkkkkkykkkk',
	'kkkkkkkkkggkkggkkkkkkkkkkk',
]
SMALL['Cyclops Room'] = [
	'kkkkkkkkkkkkkkkkkkkkkkkkkk',
	'kkkkkkkkyyyyyyyyyykkkkkkkk',
	'kkkkkkkyyyywwwwyyyykkkkkkk',
	'kkkkkkkyyyywkkwyyyykkkkkkk',
	'kkkkkkkyyyyyyyyyyyykkkkkkk',
	'kkkkkkkkyyrrrrrryykkkkkkkk',
	'kkkkkkkkkywkwwkwykkkkkkkkk',
]

def slug(n):
	return ''.join(c.lower() if c.isalnum() else '_' for c in n)

# 작은 크기로 그린 나머지 방들: art_small.txt ("@@ 방 이름" 다음 7 줄, 줄마다 26 점)
small_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'art_small.txt')
if os.path.exists(small_file):
	name = None
	for l in open(small_file, encoding='utf-8'):
		l = l.rstrip('\r\n')
		if l.startswith('@@ '): name = l[3:].strip(); SMALL[name] = []
		elif name and l.strip(): SMALL[name].append(l.strip())
	for n, rows in SMALL.items():
		assert len(rows) == H_OUT and all(len(r) == W_OUT and set(r) <= set(COL) for r in rows), n

os.makedirs(OUT, exist_ok=True)
names = list(ART) + [n for n in SMALL if n not in ART]
for name in names:
	pic = render(SMALL[name], True) if name in SMALL else render(ART[name])
	open(os.path.join(OUT, slug(name) + ".ans"), "wb").write(pic.encode("cp949"))
print(len(names), 'pictures')

# 조크 I 장면 그림 만들기: 색 글자 도트 그림 -> 이야기 안시 (완성형)
#   한 점 = 2 칸 (배경색 칸 두 개). 색: . 바탕(파랑, BBS 기본) k 검정 b 파랑 g 초록 c 청록 r 빨강 m 자주 y 갈색 w 회색
#   배경색은 0~7 만 쓴다 (밝은 배경은 이야기에서 깜빡임으로 보일 수 있음). 끝나면 ESC[=1G 로 BBS 바탕에 돌려놓는다.
#   python3 zk_art.py <출력 폴더>
import sys, os
OUT = sys.argv[1] if len(sys.argv) > 1 else '.'
COL = {'k': 0, 'b': 1, 'g': 2, 'c': 3, 'r': 4, 'm': 5, 'y': 6, 'w': 7, '.': 1}
W = 32

def render(rows):
	out = []
	for r in rows:
		r = (r + '.' * W)[:W]
		line = ' ' * 7
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

def slug(n):
	return ''.join(c.lower() if c.isalnum() else '_' for c in n)

os.makedirs(OUT, exist_ok=True)
for name, rows in ART.items():
	open(os.path.join(OUT, slug(name) + '.ans'), 'wb').write(render(rows).encode('cp949'))
print(len(ART), 'pictures')

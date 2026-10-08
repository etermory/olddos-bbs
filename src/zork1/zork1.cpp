#include "../main.h"
#include <iconv.h>
#include <errno.h>
#include <sys/stat.h>
#include <map>
#include <set>

// ------------------------------------------------------------------
// 조크 I (Zork I, Infocom 1980) 한글판 : BBS 1 인용 문 게임
//   bin/zork1 <호스트이름> <아이디> <tty>
//
// 원작 게임 파일(src/zork1/zork1.z3, Z-machine 3 판)을 그대로 돌리는 작은 Z-machine 실행기.
// 명령은 원작 그대로 영어로 넣고, 화면에 나오는 문장만 번역표(src/zork1/ko.txt)로 바꿔 보여 준다.
// 원작 소스와 게임 파일: https://github.com/historicalsource/zork1 (MIT License, Copyright (c) 2025 Microsoft)
//
// 번역표 (UTF-8)
//   @@ 원문 (줄바꿈은 \n)
//   == 번역 (비우면 원문 그대로, {} 는 아무것도 내보내지 않음)
// 조사는 앞 글자의 받침에 맞춰 고른다: {은} {이} {을} {과} {으로} {아} {이라}  (예: "{이} 켜졌습니다.")
//
// 저장: data/zork1/<아이디>.sav  (/x 로 나갈 때, 접속이 끊길 때 저절로. 원작의 save/restore 도 같은 파일)
// 번역표에 없는 문장은 data/zork1/missing.txt, 조각으로 번역된 물건/숫자가 든 줄은 data/zork1/lines.txt 에 모은다
// (lines.txt 의 줄을 번역표에 문장 틀로 넣으면 한글 어순으로 나온다).
// ------------------------------------------------------------------

struct termio sys_term;
char title[1024] = "조크 I";
char tty[10];
char host_name[256];

typedef unsigned char u8;
typedef unsigned short u16;

static std::string home, user_id, save_path, data_dir, story_path;
static std::vector<u8> mem, story;
static u16 HIGH, PC0, DICT, OBJ, GLOB, STATIC_BASE, ABBR;
static unsigned int file_len;

struct frame {
	unsigned int ret_pc;
	int store;			// 돌려줄 값을 넣을 변수 (-1: 버림)
	int nlocals;
	u16 locals[15];
	unsigned int sp;	// 이 함수가 시작할 때의 스택 높이
};
static std::vector<u16> stack;
static std::vector<frame> frames;
static unsigned int pc;
static bool running = true;

// ------------------------------------------------------------------
// 메모리
// ------------------------------------------------------------------
static inline u16 rw(unsigned int a) { return (u16)((mem[a] << 8) | mem[a + 1]); }
static inline void ww(unsigned int a, u16 v) { mem[a] = (u8)(v >> 8); mem[a + 1] = (u8)v; }

static void fatal(const char *m)
{
	printf("\r\n[조크 실행기 오류] %s (pc=%05x)\r\n", m, pc);
	running = false;
}

// ------------------------------------------------------------------
// 번역과 출력
// ------------------------------------------------------------------
static std::map<std::string, std::string> ko;
static std::set<std::string> missing_seen;
static std::string out;			// 아직 화면에 내보내지 않은 글 (UTF-8)
static int lines_since_input = 0;

static std::string convert(const std::string &in, const char *to, const char *from)
{
	iconv_t cd = iconv_open(to, from);
	if ( cd == (iconv_t)-1 ) return in;
	std::string o;
	char *src = (char*)in.data();
	size_t left = in.size();
	char buf[4096];
	while ( left > 0 ) {
		char *dst = buf;
		size_t room = sizeof(buf);
		size_t r = iconv(cd, &src, &left, &dst, &room);
		o.append(buf, dst - buf);
		if ( r == (size_t)-1 && errno != E2BIG ) { src++; left--; }
	}
	iconv_close(cd);
	return o;
}

static std::string unescape(const std::string &s)
{
	std::string o;
	for ( size_t i = 0; i < s.size(); i++ ) {
		if ( s[i] == '\\' && i + 1 < s.size() ) {
			i++;
			if ( s[i] == 'n' ) o += '\n';
			else if ( s[i] == 't' ) o += '\t';
			else o += s[i];
		} else o += s[i];
	}
	return o;
}

static std::string escape(const std::string &s)
{
	std::string o;
	for ( size_t i = 0; i < s.size(); i++ ) {
		if ( s[i] == '\n' ) o += "\\n";
		else if ( s[i] == '\\' ) o += "\\\\";
		else o += s[i];
	}
	return o;
}

static void load_translation(const std::string &path)
{
	std::vector<std::string> lines = split_string(read_file(path.c_str()), '\n');
	std::string en;
	bool have = false;
	for ( unsigned int i = 0; i < lines.size(); i++ ) {
		std::string l = lines[i];
		if ( !l.empty() && l[l.size() - 1] == '\r' ) l.erase(l.size() - 1);
		if ( l.compare(0, 3, "@@ ") == 0 ) { en = unescape(l.substr(3)); have = true; }
		else if ( l == "@@" ) { en = ""; have = true; }
		else if ( have && (l.compare(0, 3, "== ") == 0 || l == "==") ) {
			std::string k = l.size() > 3 ? unescape(l.substr(3)) : "";
			if ( k == "{}" ) ko[en] = "";		// 한글에서는 없어지는 조각 (예: "The ")
			else if ( !k.empty() ) ko[en] = k;
			have = false;
		}
	}
}

static void note_missing(const std::string &en)
{
	if ( en.empty() || missing_seen.count(en) ) return;
	missing_seen.insert(en);
	// 글자가 하나도 없는 조각(공백, 문장 부호)은 번역할 것이 없다
	bool letters = false;
	for ( size_t i = 0; i < en.size(); i++ ) if ( isalpha((unsigned char)en[i]) ) letters = true;
	if ( !letters ) return;
	std::string path = data_dir + "/missing.txt";
	std::string all = read_file(path.c_str());
	std::string line = "@@ " + escape(en) + "\n";
	if ( all.find(line) != std::string::npos ) return;
	FILE *fp = fopen(path.c_str(), "a");
	if ( fp ) { fputs(line.c_str(), fp); fputs("== \n", fp); fclose(fp); }
}

// 조각 하나 번역 (없으면 원문)
static std::string tr(const std::string &en)
{
	std::map<std::string, std::string>::const_iterator it = ko.find(en);
	if ( it != ko.end() ) return it->second;
	note_missing(en);
	return en;
}

// ------------------------------------------------------------------
// 줄 단위 문장 틀
//   게임은 "There is nothing behind the " + 물건 이름 + "." 처럼 조각으로 찍는다.
//   한 줄의 조각들을 모아 두었다가 "There is nothing behind the %o." 처럼 틀째로 번역표에 있으면
//   한글 어순대로 다시 짠다 (%o 물건 이름, %n 숫자. 번역에서 %1 %2 ... 로 순서를 바꿀 수 있다).
//   틀이 없으면 조각마다 번역한다.
// ------------------------------------------------------------------
struct piece {
	int kind;			// 0 글, 1 물건 이름, 2 숫자, 3 그대로 (입력 글자 등)
	std::string s;		// 원문
	bool has_k;			// 여러 줄 문장을 통째로 번역해 둔 조각이면 k 가 번역
	std::string k;
};
static std::vector<piece> line;

static void add_piece(int kind, const std::string &s)
{
	if ( !line.empty() && line.back().kind == 3 && kind == 3 ) { line.back().s += s; return; }
	piece p; p.kind = kind; p.s = s; p.has_k = false;
	line.push_back(p);
}

static void note_line(const std::string &key)
{
	std::string path = data_dir + "/lines.txt";
	if ( missing_seen.count("\x01" + key) ) return;
	missing_seen.insert("\x01" + key);
	std::string l = "@@ " + escape(key) + "\n";
	if ( read_file(path.c_str()).find(l) != std::string::npos ) return;
	FILE *fp = fopen(path.c_str(), "a");
	if ( fp ) { fputs(l.c_str(), fp); fputs("== \n", fp); fclose(fp); }
}

static void end_line(bool newline)
{
	if ( !line.empty() ) {
		std::string key;
		std::vector<std::string> args;
		bool placeholder = false;
		for ( unsigned int i = 0; i < line.size(); i++ ) {
			if ( line[i].kind == 0 || line[i].kind == 3 ) key += line[i].s;
			else if ( line[i].kind == 1 ) { key += "%o"; args.push_back(tr(line[i].s)); placeholder = true; }
			else { key += "%n"; args.push_back(line[i].s); placeholder = true; }
		}
		std::map<std::string, std::string>::const_iterator it = line.size() > 1 ? ko.find(key) : ko.end();
		if ( it != ko.end() ) {
			const std::string &t = it->second;
			unsigned int next = 0;
			for ( size_t i = 0; i < t.size(); i++ ) {
				if ( t[i] == '%' && i + 1 < t.size() ) {
					char c = t[i + 1];
					if ( (c == 'o' || c == 'n') && next < args.size() ) { out += args[next++]; i++; continue; }
					if ( c >= '1' && c <= '9' && (unsigned int)(c - '1') < args.size() ) { out += args[c - '1']; i++; continue; }
				}
				out += t[i];
			}
		} else {
			if ( placeholder ) note_line(key);
			for ( unsigned int i = 0; i < line.size(); i++ ) {
				if ( line[i].has_k ) out += line[i].k;
				else if ( line[i].kind == 0 || line[i].kind == 1 ) out += tr(line[i].s);
				else out += line[i].s;
			}
		}
		line.clear();
	}
	if ( newline ) out += "\n";
}

// 글 조각 (안의 줄바꿈에서 줄을 끝낸다)
static void emit(const std::string &en)
{
	// 줄바꿈이 든 문장을 통째로 번역해 두었으면 원문과 번역을 같은 줄바꿈에서 나눠 넣는다
	std::map<std::string, std::string>::const_iterator it;
	if ( en.find('\n') != std::string::npos && (it = ko.find(en)) != ko.end() ) {
		// 끝에 표시 글자를 붙여 나눈다 (끝의 빈 줄도 하나로 세도록)
		std::vector<std::string> e = split_string(en + "\x01", '\n'), k = split_string(it->second + "\x01", '\n');
		if ( e.size() == k.size() ) {
			for ( unsigned int i = 0; i < e.size(); i++ ) {
				std::string es = e[i], ks = k[i];
				if ( i + 1 == e.size() ) { es.erase(es.size() - 1); ks.erase(ks.size() - 1); }
				if ( !es.empty() || !ks.empty() ) {
					piece p; p.kind = 0; p.s = es; p.has_k = true; p.k = ks;
					line.push_back(p);
				}
				if ( i + 1 < e.size() ) end_line(true);
			}
			return;
		}
	}
	size_t a = 0;
	while ( 1 ) {
		size_t nl = en.find('\n', a);
		if ( nl == std::string::npos ) { if ( a < en.size() ) add_piece(0, en.substr(a)); break; }
		if ( nl > a ) add_piece(0, en.substr(a, nl - a));
		end_line(true);
		a = nl + 1;
	}
}

// UTF-8 한 글자를 끝에서 읽는다 (조사 고르기). 없으면 0
static unsigned long last_char(const std::string &s)
{
	int i = (int)s.size() - 1;
	// 닫는 따옴표, 괄호는 건너뛴다
	while ( i >= 0 && (s[i] == '"' || s[i] == '\'' || s[i] == ')' || s[i] == ']') ) i--;
	if ( i < 0 ) return 0;
	int start = i;
	while ( start > 0 && ((unsigned char)s[start] & 0xC0) == 0x80 ) start--;
	unsigned char c = (unsigned char)s[start];
	if ( c < 0x80 ) return c;
	if ( (c & 0xE0) == 0xC0 ) return ((c & 0x1F) << 6) | (s[start + 1] & 0x3F);
	if ( (c & 0xF0) == 0xE0 ) return ((c & 0x0F) << 12) | ((s[start + 1] & 0x3F) << 6) | (s[start + 2] & 0x3F);
	return 0;
}

// 받침: 0 없음, 1 있음, 2 ㄹ 받침
static int batchim(unsigned long c)
{
	if ( c >= 0xAC00 && c <= 0xD7A3 ) {
		int jong = (int)((c - 0xAC00) % 28);
		return jong == 0 ? 0 : jong == 8 ? 2 : 1;
	}
	if ( c >= '0' && c <= '9' ) {
		static const int d[10] = { 1, 2, 0, 1, 0, 0, 1, 2, 2, 0 };	// 영 일 이 삼 사 오 육 칠 팔 구
		return d[c - '0'];
	}
	if ( c == 'l' || c == 'L' ) return 2;
	if ( c == 'm' || c == 'n' || c == 'M' || c == 'N' ) return 1;
	return 0;
}

static std::string resolve_particles(const std::string &s)
{
	static const char *tok[][3] = {
		{ "{은}", "은", "는" }, { "{이}", "이", "가" }, { "{을}", "을", "를" }, { "{과}", "과", "와" },
		{ "{아}", "아", "야" }, { "{이라}", "이라", "라" }, { "{으로}", "으로", "로" }, { NULL, NULL, NULL } };
	// 이 소스는 완성형(EUC-KR)이고 번역은 UTF-8 이므로 한 번 바꿔 둔다
	static std::vector<std::string> u[3];
	if ( u[0].empty() ) {
		for ( int k = 0; tok[k][0]; k++ )
			for ( int j = 0; j < 3; j++ ) u[j].push_back(convert(tok[k][j], "UTF-8", "CP949"));
	}
	std::string o;
	for ( size_t i = 0; i < s.size(); ) {
		bool done = false;
		if ( s[i] == '{' ) {
			for ( unsigned int k = 0; k < u[0].size(); k++ ) {
				size_t n = u[0][k].size();
				if ( s.compare(i, n, u[0][k]) == 0 ) {
					int b = batchim(last_char(o));
					// 으로: ㄹ 받침 뒤에는 '로'
					bool first = (k == 6) ? (b == 1) : (b != 0);
					o += first ? u[1][k] : u[2][k];
					i += n;
					done = true;
					break;
				}
			}
		}
		if ( !done ) o += s[i++];
	}
	return o;
}

// 쌓인 글을 화면으로: 조사 고르기, 완성형으로, 78 칸에서 줄 바꾸기, 화면이 차면 잠깐 멈춤
static void flush_out(void)
{
	end_line(false);
	if ( out.empty() ) return;
	std::string text = convert(resolve_particles(out), "CP949//TRANSLIT", "UTF-8");
	out.clear();
	std::vector<std::string> lines = split_string(text, '\n');
	bool trailing = !text.empty() && text[text.size() - 1] == '\n';
	if ( trailing && !lines.empty() && lines.back().empty() ) lines.pop_back();
	for ( unsigned int i = 0; i < lines.size(); i++ ) {
		std::vector<std::string> parts = wrap_words(lines[i], 78);
		for ( unsigned int k = 0; k < parts.size(); k++ ) {
			printf("%s", parts[k].c_str());
			bool nl = (k + 1 < parts.size()) || (i + 1 < lines.size()) || trailing;
			if ( nl ) {
				printf("\r\n");
				if ( ++lines_since_input >= 22 ) {
					printf("\033[=7F[계속: Enter]\033[=15F");
					fflush(stdout);
					press_enter();
					printf("\r\033[K");
					lines_since_input = 0;
				}
			}
		}
	}
	fflush(stdout);
}

// ------------------------------------------------------------------
// Z 문자열
// ------------------------------------------------------------------
static const char *A2 = " \n0123456789.,!?_#'\"/\\-:()";

static unsigned int zdecode(unsigned int a, std::string &s, bool abbr_ok = true)
{
	int alpha = 0, abbr = 0, esc = 0, hi = 0;
	while ( 1 ) {
		u16 x = rw(a); a += 2;
		int zs[3] = { (x >> 10) & 31, (x >> 5) & 31, x & 31 };
		for ( int k = 0; k < 3; k++ ) {
			int c = zs[k];
			if ( abbr ) {
				std::string t;
				if ( abbr_ok ) zdecode(rw(ABBR + (32 * (abbr - 1) + c) * 2) * 2, t, false);
				s += t; abbr = 0; continue;
			}
			if ( esc == 1 ) { hi = c; esc = 2; continue; }
			if ( esc == 2 ) { s += (char)((hi << 5) | c); esc = 0; continue; }
			if ( c == 0 ) { s += ' '; alpha = 0; continue; }
			if ( c >= 1 && c <= 3 ) { abbr = c; alpha = 0; continue; }
			if ( c == 4 ) { alpha = 1; continue; }
			if ( c == 5 ) { alpha = 2; continue; }
			if ( alpha == 2 && c == 6 ) { esc = 1; alpha = 0; continue; }
			if ( alpha == 0 ) s += (char)('a' + c - 6);
			else if ( alpha == 1 ) s += (char)('A' + c - 6);
			else s += A2[c - 6];
			alpha = 0;
		}
		if ( x & 0x8000 ) break;
	}
	return a;
}

// 단어를 사전 형식(z 문자 6 개, 4 바이트)으로
static void zencode(const std::string &w, u16 out2[2])
{
	int zs[9];
	int n = 0;
	for ( size_t i = 0; i < w.size() && n < 6; i++ ) {
		char c = w[i];
		if ( c >= 'a' && c <= 'z' ) { zs[n++] = c - 'a' + 6; continue; }
		const char *p = strchr(A2 + 2, c);
		if ( c && p ) { zs[n++] = 5; if ( n < 6 ) zs[n++] = (int)(p - A2) + 6; continue; }
		zs[n++] = 5; if ( n < 6 ) zs[n++] = 6;
		if ( n < 6 ) zs[n++] = ((unsigned char)c >> 5) & 31;
		if ( n < 6 ) zs[n++] = c & 31;
	}
	while ( n < 6 ) zs[n++] = 5;
	out2[0] = (u16)((zs[0] << 10) | (zs[1] << 5) | zs[2]);
	out2[1] = (u16)((zs[3] << 10) | (zs[4] << 5) | zs[5] | 0x8000);
}

// ------------------------------------------------------------------
// 물건
// ------------------------------------------------------------------
static inline unsigned int obj_addr(int o) { return OBJ + 62 + (o - 1) * 9; }
static inline int parent(int o) { return o ? mem[obj_addr(o) + 4] : 0; }
static inline int sibling(int o) { return o ? mem[obj_addr(o) + 5] : 0; }
static inline int child(int o) { return o ? mem[obj_addr(o) + 6] : 0; }
static inline unsigned int prop_table(int o) { return rw(obj_addr(o) + 7); }

static void remove_obj(int o)
{
	if ( !o ) return;
	int p = parent(o);
	if ( !p ) return;
	unsigned int oa = obj_addr(o);
	if ( child(p) == o ) mem[obj_addr(p) + 6] = (u8)sibling(o);
	else {
		int c = child(p);
		while ( c && sibling(c) != o ) c = sibling(c);
		if ( c ) mem[obj_addr(c) + 5] = (u8)sibling(o);
	}
	mem[oa + 4] = 0; mem[oa + 5] = 0;
}

static void insert_obj(int o, int dest)
{
	if ( !o || !dest ) return;
	remove_obj(o);
	unsigned int oa = obj_addr(o), da = obj_addr(dest);
	mem[oa + 5] = mem[da + 6];
	mem[oa + 4] = (u8)dest;
	mem[da + 6] = (u8)o;
}

static unsigned int first_prop(int o)
{
	unsigned int p = prop_table(o);
	return p + 1 + mem[p] * 2;
}

// 속성 데이터 주소 (없으면 0)
static unsigned int prop_addr(int o, int num)
{
	if ( !o ) return 0;
	unsigned int p = first_prop(o);
	while ( mem[p] ) {
		int n = mem[p] & 31, len = (mem[p] >> 5) + 1;
		if ( n == num ) return p + 1;
		if ( n < num ) return 0;
		p += 1 + len;
	}
	return 0;
}

static std::string obj_name(int o)
{
	std::string s;
	if ( o && mem[prop_table(o)] ) zdecode(prop_table(o) + 1, s);
	return s;
}

// ------------------------------------------------------------------
// 변수, 스택, 함수
// ------------------------------------------------------------------
static inline void push(u16 v) { stack.push_back(v); }
static inline u16 pop(void)
{
	if ( stack.size() <= frames.back().sp ) { fatal("스택이 비었습니다"); return 0; }
	u16 v = stack.back(); stack.pop_back(); return v;
}

static u16 get_var(int v)
{
	if ( v == 0 ) return pop();
	if ( v < 16 ) return frames.back().locals[v - 1];
	return rw(GLOB + 2 * (v - 16));
}
static void set_var(int v, u16 x)
{
	if ( v == 0 ) push(x);
	else if ( v < 16 ) frames.back().locals[v - 1] = x;
	else ww(GLOB + 2 * (v - 16), x);
}
// inc, dec, store, load, pull 처럼 변수 번호로 다룰 때: 0 은 스택 맨 위 (넣고 빼지 않음)
static u16 peek_var(int v)
{
	if ( v == 0 ) { if ( stack.size() <= frames.back().sp ) return 0; return stack.back(); }
	return get_var(v);
}
static void poke_var(int v, u16 x)
{
	if ( v == 0 ) { if ( stack.size() > frames.back().sp ) stack.back() = x; else push(x); }
	else set_var(v, x);
}

static void do_return(u16 v)
{
	frame f = frames.back();
	frames.pop_back();
	stack.resize(f.sp);
	pc = f.ret_pc;
	if ( f.store >= 0 ) set_var(f.store, v);
}

static void call(unsigned int packed, const std::vector<u16> &args, int store)
{
	if ( packed == 0 ) { if ( store >= 0 ) set_var(store, 0); return; }
	unsigned int a = packed * 2;
	frame f;
	f.ret_pc = pc;
	f.store = store;
	f.nlocals = mem[a++];
	f.sp = (unsigned int)stack.size();
	for ( int i = 0; i < f.nlocals; i++ ) { f.locals[i] = rw(a); a += 2; }
	for ( unsigned int i = 1; i < args.size() && (int)i <= f.nlocals; i++ ) f.locals[i - 1] = args[i];
	frames.push_back(f);
	pc = a;
}

// ------------------------------------------------------------------
// 저장 (data/zork1/<아이디>.sav)
//   "ZK1S" 판(1) 종류(1: 원작 save 명령, 2: 입력을 기다리던 곳) pc  동적 메모리  스택  함수들
// ------------------------------------------------------------------
static void put32(std::string &s, unsigned int v) { for ( int i = 3; i >= 0; i-- ) s += (char)((v >> (i * 8)) & 255); }
static unsigned int get32(const std::string &s, size_t &i)
{
	if ( i + 4 > s.size() ) { i = s.size() + 1; return 0; }
	unsigned int v = 0;
	for ( int k = 0; k < 4; k++ ) v = (v << 8) | (u8)s[i++];
	return v;
}

static bool save_state(int kind, unsigned int at)
{
	std::string s = "ZK1S";
	s += (char)1; s += (char)kind;
	put32(s, at);
	put32(s, STATIC_BASE);
	s.append((const char*)&mem[0], STATIC_BASE);
	put32(s, (unsigned int)stack.size());
	for ( unsigned int i = 0; i < stack.size(); i++ ) { s += (char)(stack[i] >> 8); s += (char)stack[i]; }
	put32(s, (unsigned int)frames.size());
	for ( unsigned int i = 0; i < frames.size(); i++ ) {
		const frame &f = frames[i];
		put32(s, f.ret_pc); put32(s, (unsigned int)(f.store + 1)); put32(s, f.nlocals); put32(s, f.sp);
		for ( int k = 0; k < 15; k++ ) { s += (char)(f.locals[k] >> 8); s += (char)f.locals[k]; }
	}
	std::string tmp = save_path + ".tmp";
	FILE *fp = fopen(tmp.c_str(), "wb");
	if ( !fp ) return false;
	bool ok = fwrite(s.data(), 1, s.size(), fp) == s.size();
	ok = (fclose(fp) == 0) && ok;
	if ( !ok ) { unlink(tmp.c_str()); return false; }
	return rename(tmp.c_str(), save_path.c_str()) == 0;
}

// 불러오기. 성공하면 kind 를 돌려준다 (0: 실패)
static int load_state(void)
{
	std::string s = read_file(save_path.c_str());
	if ( s.size() < 14 || s.compare(0, 4, "ZK1S") != 0 || s[4] != 1 ) return 0;
	int kind = s[5];
	size_t i = 6;
	unsigned int at = get32(s, i);
	unsigned int dyn = get32(s, i);
	if ( dyn != STATIC_BASE || i + dyn > s.size() ) return 0;
	std::vector<u8> m2(mem);
	memcpy(&m2[0], s.data() + i, dyn); i += dyn;
	// 머리말의 Flags 2 (글꼴/출력 설정)는 지금 것을 둔다
	m2[0x10] = mem[0x10]; m2[0x11] = mem[0x11];
	unsigned int n = get32(s, i);
	if ( i + n * 2 > s.size() ) return 0;
	std::vector<u16> st2;
	for ( unsigned int k = 0; k < n; k++ ) { st2.push_back((u16)(((u8)s[i] << 8) | (u8)s[i + 1])); i += 2; }
	unsigned int nf = get32(s, i);
	std::vector<frame> fr2;
	for ( unsigned int k = 0; k < nf && i <= s.size(); k++ ) {
		frame f;
		f.ret_pc = get32(s, i); f.store = (int)get32(s, i) - 1; f.nlocals = (int)get32(s, i); f.sp = get32(s, i);
		if ( i + 30 > s.size() ) return 0;
		for ( int q = 0; q < 15; q++ ) { f.locals[q] = (u16)(((u8)s[i] << 8) | (u8)s[i + 1]); i += 2; }
		fr2.push_back(f);
	}
	if ( i > s.size() || fr2.empty() ) return 0;
	mem = m2; stack = st2; frames = fr2; pc = at;
	return kind;
}

// ------------------------------------------------------------------
// 입력
// ------------------------------------------------------------------
static std::string pending_input;	// 다음 입력으로 미리 넣어 둘 명령 (불러온 뒤 look)
static unsigned int sread_pc;		// 지금 sread 명령의 시작 (/x 로 저장할 곳)

static void bbs_exit(bool save)
{
	flush_out();
	if ( save ) {
		if ( save_state(2, sread_pc) ) printf("\r\n\033[=10F저장했습니다. 다음에 들어오면 이어서 할 수 있습니다.\033[=15F\r\n");
		else printf("\r\n\033[=12F저장하지 못했습니다.\033[=15F\r\n");
	}
	printf("\r\n [Enter] 를 누르세요.");
	fflush(stdout);
	press_enter();
	ioctl(0, TCSETAF, &sys_term);
	exit(0);
}

static void tokenise(unsigned int text, unsigned int parse)
{
	unsigned int ns = mem[DICT];
	std::string seps((const char*)&mem[DICT + 1], ns);
	unsigned int elen = mem[DICT + 1 + ns];
	int count = (int)rw(DICT + 2 + ns);
	unsigned int entries = DICT + 4 + ns;
	int maxw = mem[parse];
	int nw = 0;
	unsigned int p = text + 1;
	while ( mem[p] && nw < maxw ) {
		if ( mem[p] == ' ' ) { p++; continue; }
		unsigned int start = p;
		std::string w;
		if ( seps.find((char)mem[p]) != std::string::npos ) { w = (char)mem[p]; p++; }
		else {
			while ( mem[p] && mem[p] != ' ' && seps.find((char)mem[p]) == std::string::npos ) w += (char)mem[p++];
		}
		u16 code[2];
		zencode(w, code);
		unsigned int found = 0;
		int lo = 0, hi = count - 1;
		while ( lo <= hi ) {
			int mid = (lo + hi) / 2;
			unsigned int e = entries + mid * elen;
			u16 a = rw(e), b = rw(e + 2);
			if ( a == code[0] && b == code[1] ) { found = e; break; }
			if ( a < code[0] || (a == code[0] && b < code[1]) ) lo = mid + 1;
			else hi = mid - 1;
		}
		unsigned int ent = parse + 2 + nw * 4;
		ww(ent, (u16)found);
		mem[ent + 2] = (u8)w.size();
		mem[ent + 3] = (u8)(start - text);
		nw++;
	}
	mem[parse + 1] = (u8)nw;
}

static void sread(unsigned int text, unsigned int parse)
{
	flush_out();
	std::string line;
	if ( !pending_input.empty() ) {
		line = pending_input;
		pending_input.clear();
		printf("%s\r\n", line.c_str());
	} else {
		char buf[128];
		printf(ESC_ENG);
		fflush(stdout);
		line_input(buf, 76);
		line = buf;
		printf("\r\n");
	}
	lines_since_input = 0;
	std::string t = trim(line);
	if ( t == "/x" || t == "/q" || t == "/p" ) bbs_exit(true);
	if ( t == "/?" ) {
		printf("\033[=7F명령은 영어로 넣습니다: n s e w ne nw se sw up down, look, inventory(i), take <물건>, drop <물건>,\r\n");
		printf("open <물건>, examine <물건>, score, save, restore, restart, quit.   그만하기(저장): /x\033[=15F\r\n");
		t = "look";
	}
	unsigned int maxlen = mem[text];
	std::string low;
	for ( size_t i = 0; i < t.size() && low.size() < maxlen; i++ ) {
		unsigned char c = (unsigned char)t[i];
		if ( c >= 0x80 ) continue;		// 한글 명령은 받지 않는다 (원작 사전은 영어)
		low += (char)tolower(c);
	}
	for ( size_t i = 0; i < low.size(); i++ ) mem[text + 1 + i] = (u8)low[i];
	mem[text + 1 + low.size()] = 0;
	tokenise(text, parse);
}

// ------------------------------------------------------------------
// 명령 실행
// ------------------------------------------------------------------
static unsigned int rng_state = 1;
static int zrandom(int n)
{
	if ( n <= 0 ) { rng_state = n ? (unsigned int)(-n) : (unsigned int)time(NULL); return 0; }
	rng_state = rng_state * 1103515245u + 12345u;
	return (int)((rng_state >> 16) % (unsigned int)n) + 1;
}

static void branch(bool cond)
{
	u8 b = mem[pc++];
	int off;
	if ( b & 0x40 ) off = b & 0x3f;
	else {
		off = ((b & 0x3f) << 8) | mem[pc++];
		if ( off & 0x2000 ) off -= 0x4000;
	}
	if ( ((b & 0x80) != 0) != cond ) return;
	if ( off == 0 ) do_return(0);
	else if ( off == 1 ) do_return(1);
	else pc = pc + off - 2;
}

static inline int store_var(void) { return mem[pc++]; }

static void restart(void)
{
	u8 f2a = mem[0x10], f2b = mem[0x11];
	mem = story;
	mem[0x10] = f2a; mem[0x11] = f2b;
	stack.clear();
	frames.clear();
	frame f; f.ret_pc = 0; f.store = -1; f.nlocals = 0; f.sp = 0;
	frames.push_back(f);
	pc = PC0;
}

static void step(void)
{
	unsigned int op_pc = pc;
	u8 b = mem[pc++];
	int form, op;
	std::vector<u16> v;
	int types[4], nt = 0;
	if ( b < 0x80 ) {
		form = 2; op = b & 31;
		types[0] = (b & 0x40) ? 2 : 1; types[1] = (b & 0x20) ? 2 : 1; nt = 2;
	} else if ( b < 0xC0 ) {
		op = b & 15;
		int t = (b >> 4) & 3;
		if ( t == 3 ) { form = 0; nt = 0; }
		else { form = 1; types[0] = t; nt = 1; }
	} else {
		op = b & 31;
		form = (b & 0x20) ? 3 : 2;
		u8 tb = mem[pc++];
		for ( int sh = 6; sh >= 0; sh -= 2 ) {
			int t = (tb >> sh) & 3;
			if ( t == 3 ) break;
			types[nt++] = t;
		}
	}
	for ( int i = 0; i < nt; i++ ) {
		if ( types[i] == 0 ) { v.push_back(rw(pc)); pc += 2; }
		else if ( types[i] == 1 ) v.push_back(mem[pc++]);
		else v.push_back(get_var(mem[pc++]));
	}
	short a = v.size() > 0 ? (short)v[0] : 0, bb = v.size() > 1 ? (short)v[1] : 0;

	if ( form == 2 ) {
		switch ( op ) {
		case 1: { bool c = false; for ( unsigned int i = 1; i < v.size(); i++ ) if ( v[0] == v[i] ) c = true; branch(c); break; }
		case 2: branch(a < bb); break;
		case 3: branch(a > bb); break;
		case 4: { short x = (short)peek_var(v[0]) - 1; poke_var(v[0], (u16)x); branch(x < bb); break; }
		case 5: { short x = (short)peek_var(v[0]) + 1; poke_var(v[0], (u16)x); branch(x > bb); break; }
		case 6: branch(parent(v[0]) == v[1]); break;
		case 7: branch((v[0] & v[1]) == v[1]); break;
		case 8: set_var(store_var(), v[0] | v[1]); break;
		case 9: set_var(store_var(), v[0] & v[1]); break;
		case 10: branch(v[0] && (mem[obj_addr(v[0]) + v[1] / 8] & (0x80 >> (v[1] % 8))) != 0); break;
		case 11: if ( v[0] ) mem[obj_addr(v[0]) + v[1] / 8] |= (u8)(0x80 >> (v[1] % 8)); break;
		case 12: if ( v[0] ) mem[obj_addr(v[0]) + v[1] / 8] &= (u8)~(0x80 >> (v[1] % 8)); break;
		case 13: poke_var(v[0], v[1]); break;
		case 14: insert_obj(v[0], v[1]); break;
		case 15: set_var(store_var(), rw((u16)(v[0] + 2 * v[1]))); break;
		case 16: set_var(store_var(), mem[(u16)(v[0] + v[1])]); break;
		case 17: {
			int sv = store_var();
			unsigned int p = prop_addr(v[0], v[1]);
			if ( !p ) set_var(sv, rw(OBJ + (v[1] - 1) * 2));
			else if ( (mem[p - 1] >> 5) == 0 ) set_var(sv, mem[p]);
			else set_var(sv, rw(p));
			break;
		}
		case 18: set_var(store_var(), (u16)prop_addr(v[0], v[1])); break;
		case 19: {
			int sv = store_var();
			unsigned int p;
			if ( v[1] == 0 ) p = first_prop(v[0]);
			else { p = prop_addr(v[0], v[1]); if ( p ) p += (mem[p - 1] >> 5) + 1; }
			set_var(sv, p ? (u16)(mem[p] & 31) : 0);
			break;
		}
		case 20: set_var(store_var(), (u16)(a + bb)); break;
		case 21: set_var(store_var(), (u16)(a - bb)); break;
		case 22: set_var(store_var(), (u16)(a * bb)); break;
		case 23: { int sv = store_var(); if ( !bb ) { fatal("0 으로 나눔"); return; } set_var(sv, (u16)(a / bb)); break; }
		case 24: { int sv = store_var(); if ( !bb ) { fatal("0 으로 나눔"); return; } set_var(sv, (u16)(a % bb)); break; }
		default: fatal("모르는 2OP 명령"); return;
		}
	} else if ( form == 1 ) {
		switch ( op ) {
		case 0: branch(v[0] == 0); break;
		case 1: { int sv = store_var(); int s = sibling(v[0]); set_var(sv, (u16)s); branch(s != 0); break; }
		case 2: { int sv = store_var(); int c = child(v[0]); set_var(sv, (u16)c); branch(c != 0); break; }
		case 3: set_var(store_var(), (u16)parent(v[0])); break;
		case 4: set_var(store_var(), v[0] ? (u16)((mem[v[0] - 1] >> 5) + 1) : 0); break;
		case 5: poke_var(v[0], (u16)(peek_var(v[0]) + 1)); break;
		case 6: poke_var(v[0], (u16)(peek_var(v[0]) - 1)); break;
		case 7: { std::string s; zdecode(v[0], s); emit(s); break; }
		case 9: remove_obj(v[0]); break;
		case 10: add_piece(1, obj_name(v[0])); break;
		case 11: do_return(v[0]); break;
		case 12: pc = pc + (short)v[0] - 2; break;
		case 13: { std::string s; zdecode(v[0] * 2, s); emit(s); break; }
		case 14: set_var(store_var(), peek_var(v[0])); break;
		case 15: set_var(store_var(), (u16)~v[0]); break;
		default: fatal("모르는 1OP 명령"); return;
		}
	} else if ( form == 0 ) {
		switch ( op ) {
		case 0: do_return(1); break;
		case 1: do_return(0); break;
		case 2: { std::string s; pc = zdecode(pc, s); emit(s); break; }
		case 3: { std::string s; pc = zdecode(pc, s); emit(s); end_line(true); do_return(1); break; }
		case 4: break;
		case 5: {		// save: 분기 정보 앞에서 저장하고, 불러오면 그곳에서 '성공' 으로 분기
			bool ok = save_state(1, pc);
			branch(ok);
			break;
		}
		case 6: {
			unsigned int keep = pc;
			int kind = load_state();
			if ( kind == 1 ) branch(true);
			else if ( kind == 2 ) { pending_input = "look"; }	// 입력을 기다리던 곳에서 이어서
			else { pc = keep; branch(false); }
			break;
		}
		case 7: restart(); break;
		case 8: do_return(pop()); break;
		case 9: pop(); break;
		case 10: flush_out(); running = false; break;
		case 11: end_line(true); break;
		case 12: break;		// show_status: 상태 줄은 쓰지 않는다
		case 13: branch(true); break;
		default: fatal("모르는 0OP 명령"); return;
		}
	} else {
		switch ( op ) {
		case 0: call(v[0], v, store_var()); break;
		case 1: ww((u16)(v[0] + 2 * v[1]), v[2]); break;
		case 2: mem[(u16)(v[0] + v[1])] = (u8)v[2]; break;
		case 3: {
			unsigned int p = prop_addr(v[0], v[1]);
			if ( !p ) { fatal("없는 속성에 씀"); return; }
			if ( (mem[p - 1] >> 5) == 0 ) mem[p] = (u8)v[2]; else ww(p, v[2]);
			break;
		}
		case 4: sread_pc = op_pc; sread(v[0], v[1]); break;
		case 5: if ( v[0] == 13 ) end_line(true); else add_piece(3, std::string(1, (char)v[0])); break;
		case 6: { char nb[16]; snprintf(nb, sizeof(nb), "%d", (short)v[0]); add_piece(2, nb); break; }
		case 7: set_var(store_var(), (u16)zrandom((short)v[0])); break;
		case 8: push(v[0]); break;
		case 9: { u16 x = pop(); poke_var(v[0], x); break; }
		case 10: case 11: case 19: case 20: case 21: break;	// 창 나누기, 출력 흐름, 소리: 쓰지 않음
		default: fatal("모르는 VAR 명령"); return;
		}
	}
}

// ------------------------------------------------------------------
// BBS
// ------------------------------------------------------------------
void raw_mode(void)
{
    struct termio tbuf;
    ioctl(0, TCGETA, &tbuf);
    tbuf.c_cc[4] = 1;
    tbuf.c_cc[5] = 0;
    tbuf.c_iflag = 0;
    tbuf.c_iflag |= IXON;
    tbuf.c_iflag |= IXANY;
    tbuf.c_oflag = 0;
    tbuf.c_oflag &= ~OPOST;
    tbuf.c_lflag &= ~(ICANON | ISIG | ECHO);
    tbuf.c_cflag &= ~PARENB;
    tbuf.c_cflag &= ~CSIZE;
    tbuf.c_cflag |= CS8;
    ioctl(0, TCSETAF, &tbuf);
}

// 접속이 끊기면 입력을 기다리던 곳을 저장하고 끝낸다
int host_close(void)
{
	if ( sread_pc ) save_state(2, sread_pc);
    ioctl(0, TCSETAF, &sys_term);
    exit(1);
}

static bool valid_id(const std::string &id)
{
	if ( id.empty() || id.size() > 40 ) return false;
	for ( size_t i = 0; i < id.size(); i++ ) {
		unsigned char c = (unsigned char)id[i];
		if ( !isalnum(c) && c != '_' && c != '*' ) return false;
	}
	return true;
}

int main(int argc, char **argv)
{
	if ( argc > 1 ) snprintf(host_name, sizeof(host_name), "%s", argv[1]);
	user_id = argc > 2 ? argv[2] : "";
	home = getenv("HANULSO") ? getenv("HANULSO") : ".";
	story_path = home + "/src/zork1/zork1.z3";
	data_dir = home + "/data/zork1";

    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGHUP, (__sighandler_t)host_close);
    signal(SIGSEGV, (__sighandler_t)host_close);
    signal(SIGBUS, (__sighandler_t)host_close);

    ioctl(0, TCGETA, &sys_term);
	raw_mode();
    umask(0022);

	mkdir((home + "/data").c_str(), 0755);
	mkdir(data_dir.c_str(), 0755);
	if ( !valid_id(user_id) ) { printf("\r\n아이디가 잘못되었습니다.\r\n"); ioctl(0, TCSETAF, &sys_term); return 1; }
	// '*' 가 든 아이디(카페에서 가져온 회원)는 파일 이름에서 바꾼다
	std::string fid = user_id;
	for ( size_t i = 0; i < fid.size(); i++ ) if ( fid[i] == '*' ) fid[i] = '_';
	save_path = data_dir + "/" + fid + ".sav";

	std::string s = read_file(story_path.c_str());
	if ( s.size() < 64 || s[0] != 3 ) {
		printf("\r\n게임 파일이 없습니다: %s\r\n", story_path.c_str());
		printf("\r\n [Enter] 를 누르세요.");
		press_enter();
		ioctl(0, TCSETAF, &sys_term);
		return 1;
	}
	story.assign(s.begin(), s.end());
	mem = story;
	HIGH = rw(4); PC0 = rw(6); DICT = rw(8); OBJ = rw(0x0A); GLOB = rw(0x0C); STATIC_BASE = rw(0x0E); ABBR = rw(0x18);
	file_len = rw(0x1A) * 2;
	if ( mem.size() < file_len ) mem.resize(file_len + 64, 0);
	story = mem;
	// 상태 줄 없음, 화면 나누기 없음
	mem[1] |= 0x10;
	mem[1] &= (u8)~0x20;
	story[1] = mem[1];
	zrandom(-(int)(time(NULL) ^ getpid()));

	load_translation(home + "/src/zork1/ko.txt");

	printf(ESC_CLEAR);
	printf("\033[=14F  조크 I : 위대한 지하 제국\033[=15F   \033[=7F(Zork I, Infocom 1980 / 한글판)\033[=15F\r\n");
	printf("\033[=7F  명령은 영어로 넣습니다 (n, look, take lamp, open mailbox ...).  도움말 /?   그만하기(저장) /x\033[=15F\r\n");
	printf("\033[=8F  Zork I source code (c) 2025 Microsoft, MIT License\033[=15F\r\n\r\n");

	restart();
	if ( access(save_path.c_str(), F_OK) == 0 ) {
		printf("  저장한 게임이 있습니다. 이어서 할까요? (Y/n) ");
		fflush(stdout);
		if ( yesno(YES) == YES ) {
			int kind = load_state();
			if ( kind == 2 ) { pending_input = "look"; printf("\r\n"); }
			else if ( kind == 1 ) { branch(true); printf("\r\n"); }
			else { restart(); printf("\r\n  \033[=12F저장한 게임을 읽지 못해 처음부터 합니다.\033[=15F\r\n"); }
		} else {
			printf("\r\n  처음부터 합니다. (저장한 게임은 /x 로 나갈 때 새로 덮어씁니다)\r\n\r\n");
		}
	}

	while ( running ) step();
	flush_out();
	printf("\r\n [Enter] 를 누르세요.");
	fflush(stdout);
	press_enter();
	ioctl(0, TCSETAF, &sys_term);
	return 0;
}

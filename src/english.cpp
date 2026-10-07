#include "main.h"
#include <dirent.h>

// ------------------------------------------------------------------
// 오늘의 영어 한 문장 : 날마다 인터넷에서 새 문장을 받아 온다
//   bin/english <호스트이름> <아이디> <tty>
//   bin/english --line : 오늘의 문장 한 줄 "문장<TAB>뜻" (화면 파일 태그용)
//
// 문장은 ZenQuotes (zenquotes.io, 오늘의 명언), 뜻은 MyMemory (기계 번역).
// 하루에 한 번만 받아 $HANULSO/data/english/YYYY-MM-DD.txt 에 둔다
// (영어 / 뜻 / 말한 사람, 완성형). 그날 다른 사람은 이 파일을 읽는다.
// 이전/다음은 그동안 모아 둔 날들을 오간다.
// ------------------------------------------------------------------

struct termio sys_term;

char title[1024] = "오늘의 영어 한 문장";

char tty[10];

char host_name[256];

#define E_W		"\033[=15F"
#define E_Y		"\033[=14F"
#define E_C		"\033[=11F"
#define E_G		"\033[=7F"
#define E_R		"\033[=12F"

struct sentence { std::string day, en, ko, who; };

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

/* 프로그램 종료 루틴 */
int host_close (void)
{
    ioctl(0, TCSETAF, &sys_term);
    exit(1);
}

void print_header(const char *head_title)
{
	printf(ESC_CLEAR);
    printf("\033[1;1H");
	printf("\033[=9F\033[=1G%s\033[=15F\033[=1G", repeat("─", 40).c_str());
    printf("\033[1;1H");
	printf("\033[1A\033[7m%s\033[0m", host_name);
	int center = (80-strlen(strip_ansi_codes(head_title)))/2;
	if ( center < 0 ) center = 0;
    printf("\033[2;1H");
	printf("\r\033[%dC%s", center, head_title);
    printf("\033[3;1H");
	printf("\033[=0F\033[=1G%s\033[=15F\033[=1G", repeat("━", 40).c_str());
    printf("\033[4;1H");
}

// ------------------------------------------------------------------
// 받아 오기
// ------------------------------------------------------------------
static std::string data_dir(void)
{
	return std::string(getenv("HANULSO") ? getenv("HANULSO") : ".") + "/data/english";
}

static std::string today_str(void)
{
	time_t t = time(NULL);
	struct tm tm;
	localtime_r(&t, &tm);
	char b[16];
	snprintf(b, sizeof(b), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	return b;
}

static void put_utf8(std::string &out, unsigned int cp)
{
	if ( cp < 0x80 ) out += (char)cp;
	else if ( cp < 0x800 ) { out += (char)(0xC0 | cp >> 6); out += (char)(0x80 | (cp & 0x3F)); }
	else if ( cp < 0x10000 ) { out += (char)(0xE0 | cp >> 12); out += (char)(0x80 | (cp >> 6 & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
	else { out += (char)(0xF0 | cp >> 18); out += (char)(0x80 | (cp >> 12 & 0x3F)); out += (char)(0x80 | (cp >> 6 & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
}

// JSON 의 "key":"..." 값 (\" \\ \/ \n \uXXXX 를 풀어 UTF-8 로). from 부터 찾는다
static std::string json_string_value(const std::string &j, const char *key, std::string::size_type from = 0)
{
	std::string k = std::string("\"") + key + "\":\"";
	std::string::size_type p = j.find(k, from);
	if ( p == std::string::npos ) return "";
	p += k.size();
	std::string out;
	while ( p < j.size() && j[p] != '"' ) {
		char c = j[p++];
		if ( c != '\\' || p >= j.size() ) { out += c; continue; }
		char e = j[p++];
		if ( e == 'n' || e == 't' || e == 'r' ) out += ' ';
		else if ( e == 'u' && p + 4 <= j.size() ) {
			unsigned int cp = strtoul(j.substr(p, 4).c_str(), NULL, 16);
			p += 4;
			// 서로게이트 짝
			if ( cp >= 0xD800 && cp <= 0xDBFF && p + 6 <= j.size() && j[p] == '\\' && j[p + 1] == 'u' ) {
				unsigned int lo = strtoul(j.substr(p + 2, 4).c_str(), NULL, 16);
				p += 6;
				cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
			}
			put_utf8(out, cp);
		}
		else out += e;
	}
	return out;
}

// 영어 문장의 꾸밈 따옴표, 줄표 따위를 ASCII 로 (완성형으로 바꾸면 두 칸짜리 글자가 된다)
static std::string ascii_punct(const std::string &s)
{
	static const char *from[] = { "\xE2\x80\x98", "\xE2\x80\x99", "\xE2\x80\x9C", "\xE2\x80\x9D", "\xE2\x80\x93",
		"\xE2\x80\x94", "\xE2\x80\xA6", "\xC2\xA0", NULL };
	static const char *to[] = { "'", "'", "\"", "\"", "-", " - ", "...", " " };
	std::string r = s;
	for ( int i = 0; from[i]; i++ ) r = replace_all(r, from[i], to[i]);
	return r;
}

static std::string url_encode(const std::string &s)
{
	std::string r;
	for ( unsigned int i = 0; i < s.size(); i++ ) {
		unsigned char c = s[i];
		if ( isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' ) r += c;
		else { char b[4]; snprintf(b, sizeof(b), "%%%02X", c); r += b; }
	}
	return r;
}

static bool load_day(const std::string &day, sentence &s)
{
	std::string text = read_file((data_dir() + "/" + day + ".txt").c_str());
	std::vector<std::string> l = split_string(text, '\n');
	if ( l.size() < 3 || trim(l[0]).empty() ) return false;
	s.day = day;
	s.en = trim(l[0]);
	s.ko = trim(l[1]);
	s.who = trim(l[2]);
	return true;
}

static void save_day(const sentence &s)
{
	mkdir((std::string(getenv("HANULSO") ? getenv("HANULSO") : ".") + "/data").c_str(), 0755);
	mkdir(data_dir().c_str(), 0755);
	std::string path = data_dir() + "/" + s.day + ".txt";
	std::string tmp = path + "." + TO_STRING(getpid());
	FILE *fp = fopen(tmp.c_str(), "w");
	if ( fp == NULL ) return;
	fprintf(fp, "%s\n%s\n%s\n", s.en.c_str(), s.ko.c_str(), s.who.c_str());
	fclose(fp);
	chmod(tmp.c_str(), 0644);
	rename(tmp.c_str(), path.c_str());		// 여럿이 같이 받아도 반쪽 파일이 보이지 않게
}

// 한국어 뜻 (MyMemory 기계 번역). 못 받으면 ""
static std::string translate(const std::string &en_utf8)
{
	std::string json;
	if ( !download_text("https://api.mymemory.translated.net/get?langpair=en%7Cko&q=" + url_encode(en_utf8), json) ) return "";
	if ( json.find("\"responseStatus\":200") == std::string::npos ) return "";
	std::string ko = json_string_value(json, "translatedText");
	if ( ko.empty() || ko.find("MYMEMORY WARNING") != std::string::npos ) return "";
	return utf8_to_cp949(ascii_punct(ko));
}

// 오늘 문장: 저장해 둔 것이 있으면 그것, 없으면 받아 온다. 못 받으면 false
static bool fetch_today(sentence &s)
{
	std::string day = today_str();
	if ( load_day(day, s) ) {
		if ( !s.ko.empty() ) return true;
		// 지난번에 뜻을 못 받았으면 다시
	} else {
		std::string json;
		if ( !download_text("https://zenquotes.io/api/today", json) ) return false;
		std::string q = json_string_value(json, "q");
		if ( q.empty() || q.find("Too many requests") != std::string::npos ) return false;
		s.day = day;
		s.en = utf8_to_cp949(ascii_punct(q));
		s.who = utf8_to_cp949(ascii_punct(json_string_value(json, "a")));
		s.ko = "";
	}
	// 뜻은 원래 (UTF-8) 문장으로 번역한다. 저장한 것은 완성형이므로 되돌려 쓴다
	std::string en_utf8 = s.en;
	{
		char *u = cp949_to_utf8((char*)s.en.c_str());
		if ( u ) { en_utf8 = u; free(u); }
	}
	s.ko = translate(en_utf8);
	save_day(s);
	return true;
}

// 모아 둔 날들 (오래된 것부터)
static std::vector<std::string> saved_days(void)
{
	std::vector<std::string> days;
	DIR *d = opendir(data_dir().c_str());
	if ( d == NULL ) return days;
	struct dirent *e;
	while ( (e = readdir(d)) != NULL ) {
		std::string n = e->d_name;
		if ( n.size() == 14 && n.compare(10, 4, ".txt") == 0 ) days.push_back(n.substr(0, 10));
	}
	closedir(d);
	std::sort(days.begin(), days.end());
	return days;
}

// ------------------------------------------------------------------
// 화면
// ------------------------------------------------------------------
// 낱말 사이에서 width 칸에 맞춰 나누고 가운데 맞춰 찍는다 (완성형 한글은 두 칸)
static void center_wrapped(const char *color, const std::string &s, int width)
{
	std::vector<std::string> lines = wrap_words(s, width);
	for ( unsigned int i = 0; i < lines.size(); i++ ) {
		std::string l = trim(lines[i]);
		int pad = (80 - (int)l.size()) / 2;
		if ( pad < 1 ) pad = 1;
		printf("%s%s%s" E_W "\r\n", std::string(pad, ' ').c_str(), color, l.c_str());
	}
}

static const char *weekday_name(const std::string &day)
{
	static const char *w[] = { "일", "월", "화", "수", "목", "금", "토" };
	struct tm tm;
	memset(&tm, 0, sizeof(tm));
	sscanf(day.c_str(), "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday);
	tm.tm_year -= 1900;
	tm.tm_mon -= 1;
	tm.tm_hour = 12;
	mktime(&tm);
	return w[tm.tm_wday];
}

int main(int argc, char **argv)
{
	if ( argc > 1 && !strcmp(argv[1], "--line") ) {
		sentence s;
		if ( fetch_today(s) ) printf("%s\t%s\n", s.en.c_str(), s.ko.c_str());
		return 0;
	}
	if ( argc > 1 ) {
		snprintf(host_name, sizeof(host_name), "%s", argv[1]);
	}

    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGHUP, (__sighandler_t)host_close);
    signal(SIGSEGV, (__sighandler_t)host_close);
    signal(SIGBUS, (__sighandler_t)host_close);

    ioctl(0,TCGETA, &sys_term);
	raw_mode();
    umask(0022);

	print_header(title);
	printf("\r\n\r\n   " E_G "오늘의 문장을 받아 오는 중입니다..." E_W);
	fflush(stdout);
	sentence today;
	bool got = fetch_today(today);

	std::vector<std::string> days = saved_days();
	if ( days.empty() ) {
		print_header(title);
		printf("\r\n\r\n   " E_R "문장을 받아 오지 못했습니다. 잠시 뒤에 다시 들어와 주세요." E_W "\r\n");
		printf("\r\n [Enter] 를 누르세요.");
		press_enter();
		host_close();
	}
	int idx = days.size() - 1;		// 맨 마지막 = 오늘 (못 받았으면 가장 최근)

	while ( 1 ) {
		sentence s;
		if ( !load_day(days[idx], s) ) s.en = "(읽지 못했습니다)";
		bool is_today = s.day == today_str();

		print_header(title);
		int y, m, d;
		sscanf(s.day.c_str(), "%d-%d-%d", &y, &m, &d);
		char head[96];
		snprintf(head, sizeof(head), "%d년 %d월 %d일 (%s)%s", y, m, d, weekday_name(s.day),
				is_today ? " 오늘의 문장" : "");
		printf("\r\n\r\n");
		center_wrapped(E_G, head, 76);
		if ( !got && idx == (int)days.size() - 1 ) {
			center_wrapped(E_R, "(오늘 문장을 받아 오지 못해 가장 최근 문장을 보여 줍니다)", 76);
		}
		printf("\r\n\r\n");
		center_wrapped(E_Y, s.en, 70);
		printf("\r\n");
		center_wrapped(E_C, s.ko.empty() ? "(뜻을 받아 오지 못했습니다)" : s.ko, 70);
		printf("\r\n");
		if ( !s.who.empty() ) center_wrapped(E_G, "─ " + s.who + " ─", 76);
		printf("\r\n\r\n");
		printf("  " E_G "%d / %d   문장: zenquotes.io   뜻: MyMemory 기계 번역" E_W "\r\n", idx + 1, (int)days.size());

		char cmd[16];
		printf(ESC_ENG);
		printf("\r\n이전(B) 다음(N) 오늘(T)  상위메뉴(P) 종료(X) >> ");
		line_input(cmd, 4);
		std::string c = trim(cmd);
		if ( !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "x") || !strcasecmp(c.c_str(), "q") ) break;
		if ( !strcasecmp(c.c_str(), "b") ) { if ( idx > 0 ) idx--; }
		else if ( !strcasecmp(c.c_str(), "n") || c.empty() ) { if ( idx + 1 < (int)days.size() ) idx++; }
		else if ( !strcasecmp(c.c_str(), "t") ) idx = days.size() - 1;
	}

	host_close();
	return 0;
}

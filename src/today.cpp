#include "main.h"

// 오늘은 무슨 날 : 오늘의 명언, 기념일, 역사 속 오늘 (한국어 위키백과 날짜 문서)

struct termio sys_term;

char title[1024] = "오늘은 무슨 날";

char tty[10];

char host_name[256];

// ansi.h 의 표준 ANSI 색 대신 이야기 터미널 글자색 (ESC[=nF) 을 쓴다
#undef C_WHITE
#define C_WHITE		"\033[=15F"
#undef C_YELLOW
#define C_YELLOW	"\033[=14F"
#undef C_CYAN
#define C_CYAN		"\033[=11F"
#undef C_GREEN
#define C_GREEN		"\033[=10F"
#undef C_GRAY
#define C_GRAY		"\033[=7F"
#undef C_MAGENTA
#define C_MAGENTA	"\033[=13F"

#define EVENTS_PER_PAGE	12

struct event {
	int year;
	std::string text;
};

// 오늘의 명언 (날짜마다 하나)
static const char *quotes[][2] = {
	{ "천 리 길도 한 걸음부터.", "노자" },
	{ "아는 것을 안다 하고 모르는 것을 모른다 하는 것, 이것이 아는 것이다.", "공자" },
	{ "배우고 때때로 익히면 또한 기쁘지 아니한가.", "공자" },
	{ "하루라도 책을 읽지 않으면 입안에 가시가 돋는다.", "안중근" },
	{ "진리는 반드시 따르는 자가 있고 정의는 반드시 이루는 날이 있다.", "도산 안창호" },
	{ "백성은 나라의 근본이니, 근본이 튼튼해야 나라가 평안하다.", "서경" },
	{ "실패는 성공의 어머니이다.", "속담" },
	{ "시작이 반이다.", "속담" },
	{ "티끌 모아 태산.", "속담" },
	{ "가는 말이 고와야 오는 말이 곱다.", "속담" },
	{ "윗물이 맑아야 아랫물이 맑다.", "속담" },
	{ "낮말은 새가 듣고 밤말은 쥐가 듣는다.", "속담" },
	{ "웃는 얼굴에 침 못 뱉는다.", "속담" },
	{ "백지장도 맞들면 낫다.", "속담" },
	{ "될성부른 나무는 떡잎부터 알아본다.", "속담" },
	{ "고생 끝에 낙이 온다.", "속담" },
	{ "아는 길도 물어 가라.", "속담" },
	{ "우물을 파도 한 우물을 파라.", "속담" },
	{ "돌다리도 두들겨 보고 건너라.", "속담" },
	{ "지혜로운 자는 기회를 만든다.", "프랜시스 베이컨" },
	{ "아는 것이 힘이다.", "프랜시스 베이컨" },
	{ "나는 생각한다, 고로 존재한다.", "데카르트" },
	{ "너 자신을 알라.", "소크라테스" },
	{ "인생은 짧고 예술은 길다.", "히포크라테스" },
	{ "오늘 할 수 있는 일을 내일로 미루지 마라.", "벤저민 프랭클린" },
	{ "시간은 돈이다.", "벤저민 프랭클린" },
	{ "천재는 1%의 영감과 99%의 노력으로 이루어진다.", "에디슨" },
	{ "가장 큰 영광은 한 번도 실패하지 않음이 아니라 실패할 때마다 다시 일어서는 데 있다.", "공자" },
	{ "행복은 습관이다. 그것을 몸에 지니라.", "허버드" },
	{ "길을 아는 것과 그 길을 걷는 것은 다르다.", "속담" },
	{ "느리게 가는 것을 두려워하지 말고, 멈추는 것을 두려워하라.", "중국 속담" },
	{ "물은 흘러야 썩지 않는다.", "속담" },
	{ "가장 어두운 시간은 해 뜨기 직전이다.", "서양 속담" },
	{ "말 한마디로 천 냥 빚을 갚는다.", "속담" },
	{ "배움에는 왕도가 없다.", "유클리드" },
	{ "어제와 똑같이 살면서 다른 미래를 기대하는 것은 어리석다.", "격언" },
	{ "상상력은 지식보다 중요하다.", "아인슈타인" },
	{ "삶이 있는 한 희망은 있다.", "키케로" },
	{ "작은 기회로부터 종종 위대한 업적이 시작된다.", "데모스테네스" },
	{ "남을 아는 것은 지혜, 자신을 아는 것은 깨달음이다.", "노자" },
	{ "지금 잠을 자면 꿈을 꾸지만, 지금 공부하면 꿈을 이룬다.", "격언" },
	{ "즐기는 사람을 이길 수 없다.", "공자" },
	{ "세 사람이 길을 가면 그 가운데 반드시 나의 스승이 있다.", "공자" },
	{ "높이 나는 새가 멀리 본다.", "리처드 바크" },
	{ "인내는 쓰고 그 열매는 달다.", "루소" },
	{ "오늘이라는 날은 두 번 다시 오지 않는다.", "단테" },
	{ "웃음은 마음의 조깅이다.", "노먼 커즌스" },
	{ "가장 좋은 복수는 잘 사는 것이다.", "조지 허버트" },
	{ "마음이 있으면 길이 있다.", "속담" },
};

static const int quote_count = sizeof(quotes) / sizeof(quotes[0]);

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

	return;
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

// 위키 문법을 걷어 낸다
std::string clean_wiki(std::string s)
{
	std::string r;
	unsigned int i = 0;

	// <ref ...>...</ref>, <ref .../>, 그 밖의 태그
	while ( i < s.size() ) {
		if ( s.compare(i, 4, "<ref") == 0 ) {
			std::size_t close = s.find('>', i);
			if ( close == std::string::npos ) break;
			if ( s[close - 1] == '/' ) { i = close + 1; continue; }
			std::size_t end = s.find("</ref>", close);
			if ( end == std::string::npos ) break;
			i = end + 6;
			continue;
		}
		if ( s[i] == '<' ) {
			std::size_t close = s.find('>', i);
			if ( close == std::string::npos ) break;
			i = close + 1;
			continue;
		}
		r += s[i++];
	}
	s = r;

	// {{...}} 틀
	r = "";
	int depth = 0;
	for ( i = 0; i < s.size(); i++ ) {
		if ( s.compare(i, 2, "{{") == 0 ) { depth++; i++; continue; }
		if ( depth > 0 && s.compare(i, 2, "}}") == 0 ) { depth--; i++; continue; }
		if ( depth == 0 ) r += s[i];
	}
	s = r;

	// [[대상|보이는 글]] -> 보이는 글, [[대상]] -> 대상
	r = "";
	i = 0;
	while ( i < s.size() ) {
		if ( s.compare(i, 2, "[[") == 0 ) {
			std::size_t end = s.find("]]", i);
			if ( end == std::string::npos ) break;
			std::string in = s.substr(i + 2, end - i - 2);
			std::size_t bar = in.rfind('|');
			r += (bar == std::string::npos) ? in : in.substr(bar + 1);
			i = end + 2;
			continue;
		}
		// [http://... 보이는 글]
		if ( s[i] == '[' && s.compare(i, 5, "[http") == 0 ) {
			std::size_t end = s.find(']', i);
			std::size_t sp = s.find(' ', i);
			if ( end != std::string::npos ) {
				if ( sp != std::string::npos && sp < end ) r += s.substr(sp + 1, end - sp - 1);
				i = end + 1;
				continue;
			}
		}
		r += s[i++];
	}
	s = r;

	s = replace_all(s, "'''", "");
	s = replace_all(s, "''", "");
	s = replace_all(s, "&nbsp;", " ");
	return trim(s);
}

// 한국어 위키백과 "M월 D일" 문서에서 사건과 기념일
bool fetch_day(int m, int d, std::vector<event> &events, std::vector<std::string> &days)
{
	// 문서 이름은 UTF-8 로 퍼센트 인코딩 ("월" EC 9B 94, "일" EC 9D BC)
	char url[512];
	snprintf(url, sizeof(url), "https://ko.wikipedia.org/w/index.php?title=%d%%EC%%9B%%94_%d%%EC%%9D%%BC&action=raw", m, d);

	// 임시 파일 없이 받는다 (받는 중에 끊겨도 tmp 에 남지 않게)
	std::string utf;
	if ( !download_text(url, utf) ) return false;
	std::string text = utf8_to_cp949(utf);
	if ( text.empty() ) return false;

	std::vector<std::string> lines = split_string(text, '\n');
	std::string section;
	int cur_year = 0;
	for ( unsigned int i = 0; i < lines.size(); i++ ) {
		std::string line = trim(lines[i]);
		if ( line.compare(0, 2, "==") == 0 ) {
			section = trim(replace_all(line, "=", ""));
			continue;
		}
		if ( line.empty() || line[0] != '*' ) continue;

		if ( section == "사건" ) {
			if ( line.compare(0, 2, "**") == 0 ) {
				// 연도 아래에 여러 사건
				std::string t = clean_wiki(line.substr(2));
				if ( cur_year && !t.empty() ) {
					event e; e.year = cur_year; e.text = t;
					events.push_back(e);
				}
				continue;
			}
			std::string body = trim(line.substr(1));
			int y = 0;
			if ( sscanf(clean_wiki(body).c_str(), "%d", &y) != 1 ) continue;
			cur_year = y;
			std::size_t dash = body.find(" - ");
			if ( dash == std::string::npos ) continue;	// 다음 줄들(**)에 사건이 있음
			std::string t = clean_wiki(body.substr(dash + 3));
			if ( !t.empty() ) {
				event e; e.year = y; e.text = t;
				events.push_back(e);
			}
		} else if ( section == "기념일" ) {
			std::string t = clean_wiki(line.substr(line.find_first_not_of('*')));
			if ( !t.empty() ) days.push_back(t);
		}
	}
	return events.size() > 0 || days.size() > 0;
}

static bool newer(const event &a, const event &b)
{
	return a.year > b.year;
}

void show_day(int m, int d)
{
	char head[128];
	snprintf(head, sizeof(head), "%s - %d월 %d일", title, m, d);

	print_header(head);
	printf("\r\n  자료를 받아오는 중입니다...");
	fflush(stdout);

	std::vector<event> events;
	std::vector<std::string> days;
	bool ok = fetch_day(m, d, events, days);
	std::stable_sort(events.begin(), events.end(), newer);

	// 명언은 날짜마다 하나
	int q = (m * 31 + d) % quote_count;

	int page = 0;
	int pages = (events.size() + EVENTS_PER_PAGE - 1) / EVENTS_PER_PAGE;
	if ( pages < 1 ) pages = 1;

	while (1) {
		print_header(head);

		// 한 줄(78 칸)에 들어가도록 저자 길이만큼 명언을 줄인다
		int avail = 78 - 15 - (int)strlen(quotes[q][1]) - 3;
		printf("  " C_MAGENTA "오늘의 명언" C_WHITE "  %s " C_GRAY "- %s" C_WHITE "\r\n",
				string_truncate(quotes[q][0], avail, "..").c_str(), quotes[q][1]);

		if ( !ok ) {
			printf("\r\n  " C_GRAY "위키백과에서 자료를 받아오지 못했습니다." C_WHITE "\r\n");
		} else {
			if ( days.size() > 0 ) {
				std::string s;
				for ( unsigned int i = 0; i < days.size(); i++ ) {
					if ( i ) s += ", ";
					s += days[i];
				}
				printf("  " C_GREEN "기념일" C_WHITE "       %s\r\n", string_truncate(s, 62, "..").c_str());
			}
			printf(C_GRAY " %s" C_WHITE "\r\n", repeat("─", 39).c_str());
			printf("  " C_CYAN "역사 속 오늘" C_WHITE "  (%d/%d 쪽)\r\n", page + 1, pages);

			for ( int i = page * EVENTS_PER_PAGE; i < (int)events.size() && i < (page + 1) * EVENTS_PER_PAGE; i++ ) {
				printf("  " C_YELLOW "%6d년" C_WHITE " %s\r\n", events[i].year,
						string_truncate(events[i].text, 66, "..").c_str());
			}
		}

		printf(C_GRAY " %s" C_WHITE "\r\n", repeat("─", 39).c_str());
		printf(C_GRAY "  자료: 한국어 위키백과 (CC BY-SA)" C_WHITE "\r\n");

		char cmd[64];
		printf(ESC_ENG);
		printf("다음(N,엔터) 이전(B) 다른 날(예: 3-1) 상위메뉴(P) >> ");
		line_input(cmd, 20);
		std::string c = trim(cmd);

		if ( !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "x") ) return;
		if ( c.empty() || !strcasecmp(c.c_str(), "n") ) {
			if ( page + 1 < pages ) page++;
			else return;
		} else if ( !strcasecmp(c.c_str(), "b") ) {
			if ( page > 0 ) page--;
		} else {
			int nm, nd;
			if ( sscanf(c.c_str(), "%d-%d", &nm, &nd) == 2 && nm >= 1 && nm <= 12 && nd >= 1 && nd <= 31 ) {
				show_day(nm, nd);
				return;
			}
		}
	}
}

int main(int argc, char **argv)
{
	// bin/today --quote : 오늘의 명언 한 줄 "명언 - 저자" (txt 파일의 [today_quote])
	if ( argc > 1 && !strcmp(argv[1], "--quote") ) {
		time_t t = time(NULL);
		struct tm *tm = localtime(&t);
		int q = ((tm->tm_mon + 1) * 31 + tm->tm_mday) % quote_count;
		printf("%s - %s\n", quotes[q][0], quotes[q][1]);
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
    umask(0111);

	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	show_day(tm->tm_mon + 1, tm->tm_mday);

	ioctl(0, TCSETAF, &sys_term);
	return 0;
}

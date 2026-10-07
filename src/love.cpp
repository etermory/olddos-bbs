#include "main.h"
#include "lunarcalc.h"

// ------------------------------------------------------------------
// 사랑의 별점 : 이름 궁합, 생일 궁합 (별자리 + 띠), 회원과 궁합
//   bin/love <호스트이름> <아이디> <tty>
// 이름 궁합은 그 시절 공책에 쓰던 획수 궁합: 두 이름의 글자를 번갈아 놓고
// 획수를 적은 뒤, 이웃한 두 수를 더한 끝자리를 아래로 적어 두 자리가 남을 때까지.
// (재미로 보는 것)
// ------------------------------------------------------------------

struct termio sys_term;

char tty[10];

static std::string user_id;
static std::string user_nick;
static std::string user_birthday;		// YYYY-MM-DD (양력)

#define L_WHITE		"\033[=15F"
#define L_YELLOW	"\033[=14F"
#define L_RED		"\033[=12F"
#define L_PINK		"\033[=13F"
#define L_CYAN		"\033[=11F"
#define L_GREEN		"\033[=10F"
#define L_GRAY		"\033[=7F"

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

int host_close(void)
{
	printf(L_WHITE);
	fflush(stdout);
	database::close();
    ioctl(0, TCSETAF, &sys_term);
    exit(1);
}

static void print_header(const char *head_title)
{
	printf(ESC_CLEAR);
    printf("\033[1;1H");
	printf("\033[=9F\033[=1G%s\033[=15F\033[=1G", repeat("─", 40).c_str());
    printf("\033[1;1H");
	printf("\033[1A\033[7m%s\033[0m", host_name);
	int center = (80 - strlen(strip_ansi_codes(head_title))) / 2;
	if ( center < 0 ) center = 0;
    printf("\033[2;1H");
	printf("\r\033[%dC%s", center, head_title);
    printf("\033[3;1H");
	printf("\033[=0F\033[=1G%s\033[=15F\033[=1G", repeat("━", 40).c_str());
    printf("\033[4;1H");
}

static std::string ask(const char *msg, int len, bool han)
{
	char buf[64];
	printf(han ? ESC_HAN : ESC_ENG);
	printf("%s", msg);
	line_input(buf, len);
	printf(ESC_ENG);
	return trim(buf);
}

static void wait_enter(void)
{
	printf("\r\n " L_GRAY "[Enter] 를 누르세요." L_WHITE);
	press_enter();
}

// ------------------------------------------------------------------
// 한글 낱자와 획수
// ------------------------------------------------------------------
// 완성형 두 바이트 -> 유니코드 (한글 음절이 아니면 0)
static int syllable_code(unsigned char a, unsigned char b)
{
	char in[3] = { (char)a, (char)b, 0 };
	char out[8];
	iconv_t cd = iconv_open("UCS-2BE", "CP949");
	if ( cd == (iconv_t)-1 ) return 0;
	char *ip = in, *op = out;
	size_t il = 2, ol = sizeof(out);
	int code = 0;
	if ( iconv(cd, &ip, &il, &op, &ol) != (size_t)-1 && ol == sizeof(out) - 2 ) {
		code = ((unsigned char)out[0] << 8) | (unsigned char)out[1];
	}
	iconv_close(cd);
	return (code >= 0xAC00 && code <= 0xD7A3) ? code : 0;
}

// 초성 19, 중성 21, 종성 28 (첫째는 받침 없음)
static const int cho_strokes[19] = { 2, 4, 2, 3, 6, 5, 4, 4, 8, 2, 4, 1, 3, 6, 4, 3, 4, 4, 3 };
static const int jung_strokes[21] = { 2, 3, 3, 4, 2, 3, 3, 4, 2, 4, 5, 3, 3, 2, 4, 5, 3, 3, 1, 2, 1 };
static const int jong_strokes[28] = { 0, 2, 4, 4, 2, 5, 5, 3, 5, 7, 9, 9, 7, 9, 9, 8, 4, 4, 6, 2, 4, 1, 3, 4, 3, 4, 4, 3 };

// 이름의 글자들 (한글 음절만) 과 획수
static bool name_strokes(const std::string &name, std::vector<std::string> &chars, std::vector<int> &strokes)
{
	chars.clear();
	strokes.clear();
	for ( unsigned int i = 0; i + 1 < name.size(); ) {
		unsigned char a = name[i], b = name[i + 1];
		if ( a < 0x80 ) { i++; continue; }
		int code = syllable_code(a, b);
		if ( code ) {
			int s = code - 0xAC00;
			chars.push_back(name.substr(i, 2));
			strokes.push_back(cho_strokes[s / 588] + jung_strokes[s % 588 / 28] + jong_strokes[s % 28]);
		}
		i += 2;
	}
	return !chars.empty() && chars.size() <= 6;
}

// 이름 궁합: a 를 앞에. 피라미드를 그리고 % 를 돌려준다
static int name_match(const std::string &a, const std::string &b, bool draw, int row)
{
	std::vector<std::string> ca, cb;
	std::vector<int> sa, sb;
	name_strokes(a, ca, sa);
	name_strokes(b, cb, sb);

	std::vector<std::string> chars;
	std::vector<int> nums;
	for ( unsigned int i = 0; i < ca.size() || i < cb.size(); i++ ) {
		if ( i < ca.size() ) { chars.push_back(ca[i]); nums.push_back(sa[i] % 10); }
		if ( i < cb.size() ) { chars.push_back(cb[i]); nums.push_back(sb[i] % 10); }
	}

	// 글자 한 칸은 4 칸 (한글 2 칸 + 사이)
	int width = chars.size() * 4;
	int left = (80 - width) / 2;
	if ( draw ) {
		printf("\033[%d;%dH", row, left + 1);
		for ( unsigned int i = 0; i < chars.size(); i++ ) {
			bool from_a = (i % 2 == 0 && i / 2 < ca.size()) || (i >= cb.size() * 2);
			printf("%s%s" L_WHITE "  ", from_a ? L_CYAN : L_PINK, chars[i].c_str());
		}
		row++;
	}
	int step = 0;
	while ( 1 ) {
		if ( draw ) {
			printf("\033[%d;%dH", row++, left + 1 + step * 2);
			for ( unsigned int i = 0; i < nums.size(); i++ ) printf("%s%d" L_WHITE "   ", nums.size() == 2 ? L_YELLOW : L_WHITE, nums[i]);
		}
		if ( nums.size() <= 2 ) break;
		std::vector<int> next;
		for ( unsigned int i = 0; i + 1 < nums.size(); i++ ) next.push_back((nums[i] + nums[i + 1]) % 10);
		nums = next;
		step++;
	}
	int pct = nums.size() == 2 ? nums[0] * 10 + nums[1] : nums[0] * 10;
	if ( pct == 0 ) pct = 100;		// 00 은 100%
	return pct;
}

static const char *pct_comment(int pct)
{
	if ( pct >= 90 ) return "하늘이 맺어 준 짝! 서로 말하지 않아도 마음이 통합니다.";
	if ( pct >= 80 ) return "아주 잘 어울려요. 함께 있으면 웃음이 끊이지 않겠네요.";
	if ( pct >= 70 ) return "좋은 궁합입니다. 작은 배려가 큰 사랑으로 자랍니다.";
	if ( pct >= 60 ) return "괜찮은 사이예요. 서로의 다른 점을 아껴 주세요.";
	if ( pct >= 50 ) return "반반입니다. 마음을 먼저 여는 쪽이 이깁니다.";
	if ( pct >= 40 ) return "티격태격하지만 정이 드는 사이. 다툰 날엔 먼저 전보를!";
	if ( pct >= 30 ) return "조금 노력이 필요해요. 공통 취미를 찾아보세요.";
	if ( pct >= 20 ) return "쉽지는 않지만, 궁합은 재미일 뿐! 마음이 더 중요합니다.";
	return "숫자는 숫자일 뿐. 진심이면 0% 도 100% 가 됩니다.";
}

static void hearts(int pct)
{
	int n = (pct + 10) / 20;
	if ( n > 5 ) n = 5;
	printf(L_RED);
	for ( int i = 0; i < 5; i++ ) printf("%s", i < n ? "♥" : "♡");
	printf(L_WHITE);
}

static void name_compat(std::string a, std::string b)
{
	std::vector<std::string> c; std::vector<int> s;
	if ( a.empty() ) a = ask("\r\n  첫째 이름 (한글, 1~6 자) >> ", 12, true);
	if ( a.empty() ) return;
	if ( b.empty() ) b = ask("\r\n  둘째 이름 (한글, 1~6 자) >> ", 12, true);
	if ( b.empty() ) return;
	if ( !name_strokes(a, c, s) || !name_strokes(b, c, s) ) {
		printf("\r\n  " L_RED "이름은 한글 1~6 글자로 넣어 주세요." L_WHITE "\r\n");
		wait_enter();
		return;
	}
	std::string na = display_text(a), nb = display_text(b);
	print_header(L_PINK "사랑의 별점" L_WHITE " - 이름 궁합");
	// 피라미드 하나는 글자 수만큼 줄을 쓴다. 둘 다 들어가면 둘 다, 아니면 앞의 것만
	std::vector<std::string> ca, cb; std::vector<int> sa, sb;
	name_strokes(a, ca, sa);
	name_strokes(b, cb, sb);
	int n = ca.size() + cb.size();
	bool both = 5 + n + 1 + n <= 20;
	int p1 = name_match(a, b, true, 5);
	int p2 = name_match(b, a, both, 5 + n + 1);
	printf("\033[20;1H");
	printf("  " L_CYAN "%s" L_WHITE " → " L_PINK "%s" L_WHITE " : " L_YELLOW "%3d%%" L_WHITE "  ", na.c_str(), nb.c_str(), p1);
	hearts(p1);
	printf("\r\n  " L_PINK "%s" L_WHITE " → " L_CYAN "%s" L_WHITE " : " L_YELLOW "%3d%%" L_WHITE "  ", nb.c_str(), na.c_str(), p2);
	hearts(p2);
	printf("\r\n  %s", pct_comment((p1 + p2) / 2));
	wait_enter();
}

// ------------------------------------------------------------------
// 생일 궁합 (별자리 + 띠)
// ------------------------------------------------------------------
struct star_sign { const char *name; int from_m, from_d; int element; };	// 0 불, 1 흙, 2 바람, 3 물
static const star_sign stars[] = {
	{ "물병자리", 1, 20, 2 }, { "물고기자리", 2, 19, 3 }, { "양자리", 3, 21, 0 }, { "황소자리", 4, 20, 1 },
	{ "쌍둥이자리", 5, 21, 2 }, { "게자리", 6, 22, 3 }, { "사자자리", 7, 23, 0 }, { "처녀자리", 8, 23, 1 },
	{ "천칭자리", 9, 24, 2 }, { "전갈자리", 10, 23, 3 }, { "사수자리", 11, 23, 0 }, { "염소자리", 12, 25, 1 },
};
static const char *element_name[] = { "불", "흙", "바람", "물" };

static int star_of(int m, int d)
{
	int md = m * 100 + d;
	for ( int i = 11; i >= 0; i-- ) if ( md >= stars[i].from_m * 100 + stars[i].from_d ) return i;
	return 11;
}

// 띠 (입춘 2/4 전이면 앞 해, 오늘의 운세와 같게)
static int ddi_of(int y, int m, int d)
{
	if ( m < 2 || (m == 2 && d < 4) ) y--;
	return ((y - 4) % 12 + 12) % 12;
}

static int star_score(int a, int b, const char **why)
{
	int ea = stars[a].element, eb = stars[b].element;
	if ( a == b ) { *why = "같은 별자리: 서로를 거울처럼 잘 압니다"; return 82; }
	if ( ea == eb ) { *why = "같은 기운: 생각하는 방식이 닮았어요"; return 92; }
	if ( (ea == 0 && eb == 2) || (ea == 2 && eb == 0) ) { *why = "불과 바람: 서로를 더 타오르게 합니다"; return 88; }
	if ( (ea == 1 && eb == 3) || (ea == 3 && eb == 1) ) { *why = "흙과 물: 서로를 단단하고 촉촉하게"; return 86; }
	if ( (ea == 0 && eb == 3) || (ea == 3 && eb == 0) ) { *why = "불과 물: 끌리지만 부딪히기 쉬워요"; return 48; }
	if ( (ea == 0 && eb == 1) || (ea == 1 && eb == 0) ) { *why = "불과 흙: 속도가 달라 기다림이 필요"; return 58; }
	if ( (ea == 2 && eb == 1) || (ea == 1 && eb == 2) ) { *why = "바람과 흙: 자유와 안정 사이 줄다리기"; return 55; }
	*why = "바람과 물: 말보다 마음을 먼저 보여 주세요";
	return 60;
}

static int ddi_score(int a, int b, const char **why)
{
	static const int yukhap[12] = { 1, 0, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2 };		// 자축 인해 묘술 진유 사신 오미
	static const int wonjin[12] = { 7, 6, 9, 8, 11, 10, 1, 0, 3, 2, 5, 4 };		// 자미 축오 인유 묘신 진해 사술
	int diff = ((a - b) % 12 + 12) % 12;
	if ( a == b ) { *why = "같은 띠: 친구 같은 연인"; return 75; }
	if ( diff == 4 || diff == 8 ) { *why = "삼합: 함께하면 힘이 세 배"; return 95; }
	if ( yukhap[a] == b ) { *why = "육합: 서로 모자란 곳을 채워 줍니다"; return 90; }
	if ( diff == 6 ) { *why = "충: 정반대라 부딪히지만 그만큼 끌려요"; return 42; }
	if ( wonjin[a] == b ) { *why = "원진: 사소한 일로 서운해지기 쉬워요"; return 48; }
	*why = "무난한 사이: 노력한 만큼 가까워집니다";
	return 68;
}

static bool parse_date(const std::string &s, int *y, int *m, int *d)
{
	if ( sscanf(s.c_str(), "%d-%d-%d", y, m, d) != 3 && sscanf(s.c_str(), "%4d%2d%2d", y, m, d) != 3 ) return false;
	return valid_solar(*y, *m, *d);
}

static void birth_compat(std::string na, std::string ba, std::string nb, std::string bb)
{
	int y1, m1, d1, y2, m2, d2;
	if ( ba.empty() ) ba = ask("\r\n  첫째 사람 생일 (양력, 예: 1995-3-14) >> ", 10, false);
	if ( !parse_date(ba, &y1, &m1, &d1) ) {
		if ( !ba.empty() ) { printf("\r\n  " L_RED "날짜는 1995-3-14 처럼 넣으세요." L_WHITE "\r\n"); wait_enter(); }
		return;
	}
	if ( bb.empty() ) bb = ask("\r\n  둘째 사람 생일 (양력, 예: 1996-7-2) >> ", 10, false);
	if ( !parse_date(bb, &y2, &m2, &d2) ) {
		if ( !bb.empty() ) { printf("\r\n  " L_RED "날짜는 1996-7-2 처럼 넣으세요." L_WHITE "\r\n"); wait_enter(); }
		return;
	}
	if ( na.empty() ) na = "첫째";
	if ( nb.empty() ) nb = "둘째";

	int s1 = star_of(m1, d1), s2 = star_of(m2, d2);
	int t1 = ddi_of(y1, m1, d1), t2 = ddi_of(y2, m2, d2);
	const char *why_s, *why_d;
	int ss = star_score(s1, s2, &why_s), ds = ddi_score(t1, t2, &why_d);
	// 두 생일로 정해지는 작은 흔들림 (같은 두 사람은 늘 같은 점수)
	unsigned int h = (y1 * 372 + m1 * 31 + d1) * 2654435761u ^ (y2 * 372 + m2 * 31 + d2) * 40503u;
	int total = (ss + ds) / 2 + (int)(h % 9) - 4;
	if ( total > 99 ) total = 99;
	if ( total < 10 ) total = 10;

	print_header(L_PINK "사랑의 별점" L_WHITE " - 생일 궁합");
	printf("\r\n");
	printf("    " L_CYAN "%-12s" L_WHITE " %04d-%02d-%02d   %-10s (%s)   %s띠\r\n", string_truncate(na, 12, "").c_str(),
			y1, m1, d1, stars[s1].name, element_name[stars[s1].element], ddi[t1]);
	printf("    " L_PINK "%-12s" L_WHITE " %04d-%02d-%02d   %-10s (%s)   %s띠\r\n\r\n", string_truncate(nb, 12, "").c_str(),
			y2, m2, d2, stars[s2].name, element_name[stars[s2].element], ddi[t2]);
	printf("    " L_YELLOW "별자리 궁합" L_WHITE "  %3d점   %s\r\n", ss, why_s);
	printf("    " L_YELLOW "띠 궁합    " L_WHITE "  %3d점   %s\r\n\r\n", ds, why_d);
	printf("    " L_PINK "사랑의 별점" L_WHITE "  " L_YELLOW "%3d%%" L_WHITE "   ", total);
	hearts(total);
	printf("\r\n\r\n    %s\r\n", pct_comment(total));
	printf("\r\n    " L_GRAY "(재미로 보는 궁합입니다. 진짜 궁합은 함께 보낸 시간이 만들어요.)" L_WHITE "\r\n");
	wait_enter();
}

// ------------------------------------------------------------------
// 회원과 궁합: 닉네임으로 이름 궁합, 생일로 생일 궁합
// ------------------------------------------------------------------
static void member_compat(void)
{
	std::string who = ask("\r\n  상대 회원 (아이디 또는 닉네임) >> ", 30, true);
	if ( who.empty() ) return;
	bool exist;
	std::map<std::string, std::string> u = database::user_info((char*)who.c_str(), &exist);
	if ( !exist ) u = database::user_info_by_nick_name((char*)who.c_str(), &exist);
	if ( !exist ) {
		printf("\r\n  " L_RED "'%s' 회원이 없습니다." L_WHITE "\r\n", string_truncate(display_text(who), 20, "").c_str());
		wait_enter();
		return;
	}
	std::string nick = display_text(u["NICK_NAME"]);
	if ( nick.empty() ) nick = u["USER_ID"];
	std::vector<std::string> c; std::vector<int> s;
	if ( name_strokes(user_nick, c, s) && name_strokes(nick, c, s) ) name_compat(user_nick, nick);
	else {
		printf("\r\n  " L_GRAY "닉네임에 한글이 없거나 너무 길어 이름 궁합은 건너뜁니다." L_WHITE "\r\n");
		wait_enter();
	}
	std::string b = u["BIRTHDAY"];
	if ( user_birthday.size() >= 10 && b.size() >= 10 && b.compare(0, 4, "0000") != 0 && user_birthday.compare(0, 4, "0000") != 0 ) {
		birth_compat(user_nick, user_birthday.substr(0, 10), nick, b.substr(0, 10));
	}
}

static void title(void)
{
	while ( 1 ) {
		print_header(L_PINK "사랑의 별점" L_WHITE);
		printf("\r\n");
		printf("                " L_RED "♥" L_PINK "  두 사람은 얼마나 잘 어울릴까요?  " L_RED "♥" L_WHITE "\r\n\r\n");
		printf("        1. 이름 궁합   " L_GRAY "(그 시절 공책에 쓰던 획수 궁합)" L_WHITE "\r\n");
		printf("        2. 생일 궁합   " L_GRAY "(별자리 + 띠)" L_WHITE "\r\n");
		printf("        3. 회원과 궁합 " L_GRAY "(닉네임과 생일로)" L_WHITE "\r\n\r\n");
		if ( user_birthday.size() >= 10 && user_birthday.compare(0, 4, "0000") != 0 ) {
			int y, m, d;
			if ( parse_date(user_birthday.substr(0, 10), &y, &m, &d) ) {
				printf("        " L_GRAY "내 생일 %s : %s, %s띠" L_WHITE "\r\n\r\n", user_birthday.substr(0, 10).c_str(),
						stars[star_of(m, d)].name, ddi[ddi_of(y, m, d)]);
			}
		}
		std::string c = ask("  선택 (끝내기: Q) >> ", 3, false);
		if ( c.empty() ) continue;
		if ( !strcasecmp(c.c_str(), "q") || !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "x") ) return;
		int n = atoi(c.c_str());
		if ( n == 1 ) name_compat("", "");
		else if ( n == 2 ) birth_compat("", "", "", "");
		else if ( n == 3 ) member_compat();
	}
}

int main(int argc, char **argv)
{
	if ( argc < 3 ) {
		printf("usage: %s <host_name> <user_id> [tty]\n", argv[0]);
		return 1;
	}
	snprintf(host_name, sizeof(host_name), "%s", argv[1]);
	user_id = argv[2];
	snprintf(tty, sizeof(tty), "%s", argc > 3 ? argv[3] : "");

    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGHUP, (__sighandler_t)host_close);
    signal(SIGSEGV, (__sighandler_t)host_close);
    signal(SIGBUS, (__sighandler_t)host_close);

	read_settings("hanulso.cfg");

    ioctl(0, TCGETA, &sys_term);
	raw_mode();
	init_lunar();

	if ( database::open() == false )
		exit(1);

	bool exist;
	std::map<std::string, std::string> user = database::user_info((char*)user_id.c_str(), &exist);
	user_nick = display_text(user["NICK_NAME"]);
	if ( user_nick.empty() ) user_nick = user_id;
	user_birthday = user["BIRTHDAY"];

	title();

	host_close();
	return 0;
}

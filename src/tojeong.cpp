#include "main.h"
#include "lunarcalc.h"

// ------------------------------------------------------------------
// 토정비결
//   bin/tojeong <호스트이름> <아이디> <tty>
// 괘를 짓는 법은 전해 오는 그대로다 (음력 생년월일과 보려는 해):
//   상괘 = (그해 나이 + 그해 태세수) 를 8 로 나눈 나머지 (0 이면 8)
//   중괘 = (그해 생월의 날수 + 그 달 월건수) 를 6 으로 나눈 나머지 (0 이면 6)
//   하괘 = (생일 + 그날 일진수) 를 3 으로 나눈 나머지 (0 이면 3)
//   간지의 수: 천간 갑기 9 을경 8 병신 7 정임 6 무계 5, 지지 자오 9 축미 8 인신 7 묘유 6 진술 5 사해 4
// 144 괘의 풀이 글은 옛 책을 옮긴 것이 아니라 괘의 뜻(상 8, 중 6, 하 3) 을 엮어 새로 지었다.
// (재미로 보는 것)
// ------------------------------------------------------------------

struct termio sys_term;

char tty[10];

static std::string user_id;
static std::string user_nick;
static std::string user_birthday;		// YYYY-MM-DD (양력)

#define T_WHITE		"\033[=15F"
#define T_YELLOW	"\033[=14F"
#define T_RED		"\033[=12F"
#define T_CYAN		"\033[=11F"
#define T_GREEN		"\033[=10F"
#define T_GRAY		"\033[=7F"
#define T_MAGENTA	"\033[=13F"

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
	printf(T_WHITE);
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

static std::string ask(const char *msg, int len)
{
	char buf[64];
	printf(ESC_ENG);
	printf("%s", msg);
	line_input(buf, len);
	return trim(buf);
}

static void wait_enter(void)
{
	printf("\r\n " T_GRAY "[Enter] 를 누르세요." T_WHITE);
	press_enter();
}

// ------------------------------------------------------------------
// 풀이 글 (새로 지은 것)
// ------------------------------------------------------------------
struct sang_t { const char *name, *image, *line1, *line2; int score; };
static const sang_t sang[9] = {
	{ "", "", "", "", 0 },
	{ "하늘", "하늘 높이 해가 솟는다",
	  "막혔던 일이 풀리고 하는 일마다 힘이 붙는 해입니다. 큰 뜻을 품었다면",
	  "올해 첫걸음을 떼세요. 다만 높이 오를수록 몸을 낮추어야 오래 갑니다.", 2 },
	{ "연못", "연못에 봄물이 차오른다",
	  "사람 사이에 기쁨이 넘치는 해입니다. 반가운 소식과 모임이 잦고,",
	  "말 한마디로 복을 부릅니다. 즐거움에 취해 씀씀이가 커지지 않게 하세요.", 1 },
	{ "불", "등불이 어둠을 밝힌다",
	  "이름이 알려지고 재주를 인정받는 해입니다. 미뤄 둔 공부나 시험에",
	  "좋은 결과가 있습니다. 서두르면 불꽃이 튀니 한 박자 쉬어 가세요.", 1 },
	{ "우레", "봄 우레가 잠을 깨운다",
	  "변화가 찾아오는 해입니다. 자리를 옮기거나 새 일을 맡기 쉽고,",
	  "처음엔 어수선해도 움직인 만큼 길이 열립니다. 겁내지 마세요.", 0 },
	{ "바람", "바람 따라 돛을 올린다",
	  "흐름을 타면 멀리 가는 해입니다. 거래와 이동, 소식이 바람처럼 오가니",
	  "고집보다 귀를 여세요. 남의 말에 휩쓸려 큰돈을 걸지는 마세요.", 1 },
	{ "물", "깊은 물을 건넌다",
	  "고비가 한두 번 있는 해입니다. 서두르지 말고 돌다리도 두드리세요.",
	  "물이 깊을수록 조용히 흐르듯, 참고 견디면 가을부터 숨통이 트입니다.", -1 },
	{ "산", "산에 올라 숨을 고른다",
	  "멈추어 돌아보는 해입니다. 큰일을 벌이기보다 가진 것을 지키고",
	  "몸과 마음을 가다듬으세요. 올해 다진 터가 내년에 집이 됩니다.", 0 },
	{ "땅", "넓은 들에 씨를 뿌린다",
	  "부지런함이 그대로 열매가 되는 해입니다. 빨리 거두려 하지 말고",
	  "꾸준히 물을 주세요. 늦게 피는 꽃이 더 오래 갑니다.", 0 },
};

struct jung_t { const char *name, *line; int score; };
static const jung_t jung[7] = {
	{ "", "", 0 },
	{ "귀인", "한 해 가운데 귀인이 손을 내밉니다. 윗사람이나 오랜 벗의 도움을 받습니다.", 1 },
	{ "재물", "곳간이 차는 운이 있습니다. 작은 돈이 모여 큰돈이 되니 아껴 모으세요.", 1 },
	{ "구설", "입이 화를 부를 수 있습니다. 남의 이야기는 듣기만 하고 옮기지 마세요.", -1 },
	{ "화목", "집안에 웃음꽃이 핍니다. 가족과 함께하는 시간이 복을 불러옵니다.", 1 },
	{ "길",   "먼 길에 새 인연이 있습니다. 여행이나 이사가 뜻밖의 기회가 됩니다.", 0 },
	{ "건강", "몸을 먼저 돌보아야 합니다. 무리한 밤샘과 과음을 멀리하세요.", -1 },
};

struct ha_t { const char *name, *line; int score; };
static const ha_t ha[4] = {
	{ "", "", 0 },
	{ "끝이 좋음", "처음은 더디나 끝은 환합니다. 마지막까지 손을 놓지 마세요.", 1 },
	{ "고름",      "처음과 끝이 고릅니다. 무리하지 않으면 큰 탈 없이 지나갑니다.", 0 },
	{ "끝을 조심", "처음은 순조로우나 끝이 흐려지기 쉽습니다. 연말에는 몸을 사리세요.", -1 },
};

static const char *month_name[12] = { "정월", "이월", "삼월", "사월", "오월", "유월",
	"칠월", "팔월", "구월", "시월", "동짓달", "섣달" };

static const char *month_good[] = {
	"뜻밖의 기쁜 소식이 문을 두드립니다.",
	"하는 일마다 손발이 척척 맞습니다.",
	"귀인이 동쪽에서 찾아옵니다.",
	"작은 투자가 쏠쏠한 열매를 맺습니다.",
	"오래 기다리던 일이 드디어 풀립니다.",
	"새로운 사람을 만나 힘을 얻습니다.",
	"집안에 경사가 있어 웃음이 넘칩니다.",
	"공부와 일에 머리가 맑게 트입니다.",
	"먼 데서 반가운 사람이 옵니다.",
	"애쓴 만큼 칭찬과 보람이 따릅니다.",
};

static const char *month_mid[] = {
	"큰 탈은 없으니 하던 일을 꾸준히 하세요.",
	"얻는 것과 잃는 것이 비슷합니다. 욕심을 줄이세요.",
	"바쁘지만 실속은 보통입니다. 쉬어 가는 날도 두세요.",
	"사람을 만나면 좋으나 돈거래는 미루세요.",
	"작은 변화가 있습니다. 순리대로 따르면 됩니다.",
	"계획을 다시 짜기 좋은 달입니다.",
	"기다리던 소식은 조금 늦게 옵니다.",
	"가까운 사람의 말에 답이 있습니다.",
	"건강은 무난하니 규칙적으로 생활하세요.",
	"작은 지출이 잦으니 가계부를 살피세요.",
};

static const char *month_bad[] = {
	"말다툼을 피하고 한 걸음 물러서세요.",
	"서류와 약속을 두 번 확인하세요.",
	"감기와 배탈을 조심하세요.",
	"남의 보증이나 돈 부탁은 정중히 거절하세요.",
	"서두르면 일을 그르칩니다. 천천히 가세요.",
	"물가와 높은 곳을 조심하세요.",
	"기대가 크면 실망도 큽니다. 마음을 비우세요.",
	"잃어버리기 쉬운 달입니다. 지갑과 열쇠를 챙기세요.",
	"구설이 따르니 말을 아끼세요.",
	"무리한 계획은 다음 달로 미루세요.",
};

static const char *directions[] = { "동쪽", "서쪽", "남쪽", "북쪽", "동남쪽", "서남쪽", "동북쪽", "서북쪽" };
static const char *colors[] = { "빨강", "노랑", "파랑", "초록", "하양", "보라", "주황", "남색" };

#define COUNT(a) ((int)(sizeof(a) / sizeof(a[0])))

// ------------------------------------------------------------------
// 괘 짓기
// ------------------------------------------------------------------
static const int gan_num[10] = { 9, 8, 7, 6, 5, 9, 8, 7, 6, 5 };			// 갑을병정무기경신임계
static const int ji_num[12] = { 9, 8, 7, 6, 5, 4, 9, 8, 7, 6, 5, 4 };		// 자축인묘진사오미신유술해

struct gwae_t {
	int sang, jung, ha;
	int age, taese, days, wolgeon, iljin;
	std::string year_gapja, month_gapja, day_gapja;
};

static gwae_t make_gwae(int year, int lm, int ld, int ly)
{
	gwae_t g;
	g.age = year - ly + 1;

	int yg = (year + 6 - LUNAR_BASE_YEAR) % 10, yj = (year - LUNAR_BASE_YEAR) % 12;
	g.taese = gan_num[yg] + ji_num[yj];
	g.year_gapja = std::string(cheongan[yg]) + jiji[yj];

	long month_count = lm + 12L * (year - LUNAR_BASE_YEAR);
	int mg = (month_count + 3) % 10, mj = (month_count + 1) % 12;
	g.days = lunar_month_days(year, lm, false);
	g.wolgeon = gan_num[mg] + ji_num[mj];
	g.month_gapja = std::string(cheongan[mg]) + jiji[mj];

	int day = ld > g.days ? g.days : ld;		// 그해 그 달에 없는 날 (30 일) 은 그믐으로
	long abs_days = lunar_abs_days(year, lm, day, false);
	int dg = (abs_days + 4) % 10, dj = (abs_days + 2) % 12;
	g.iljin = gan_num[dg] + ji_num[dj];
	g.day_gapja = std::string(cheongan[dg]) + jiji[dj];

	g.sang = (g.age + g.taese) % 8; if ( g.sang == 0 ) g.sang = 8;
	g.jung = (g.days + g.wolgeon) % 6; if ( g.jung == 0 ) g.jung = 6;
	g.ha = (ld + g.iljin) % 3; if ( g.ha == 0 ) g.ha = 3;
	return g;
}

static unsigned int mix(unsigned int h)
{
	h ^= h >> 13;
	h *= 0x5bd1e995;
	h ^= h >> 15;
	return h;
}

static void stars(int n)
{
	if ( n < 1 ) n = 1;
	if ( n > 5 ) n = 5;
	printf(T_YELLOW);
	for ( int i = 0; i < 5; i++ ) printf("%s", i < n ? "★" : "☆");
	printf(T_WHITE);
}

static void show_reading(const std::string &who, int year, int ly, int lm, int ld, bool from_solar, int sy, int sm, int sd)
{
	gwae_t g = make_gwae(year, lm, ld, ly);
	int code = g.sang * 100 + g.jung * 10 + g.ha;
	int total = sang[g.sang].score + jung[g.jung].score + ha[g.ha].score;		// -3 .. 4
	int total_stars = 3 + (total >= 0 ? (total + 1) / 2 : -((-total + 1) / 2));

	// 1 쪽: 괘와 한 해 풀이
	char title[160];
	snprintf(title, sizeof(title), T_MAGENTA "토정비결" T_WHITE " - %d년 (%s년)", year, g.year_gapja.c_str());
	print_header(title);
	printf("\r\n  " T_CYAN "%s" T_WHITE, string_truncate(who, 16, "").c_str());
	if ( from_solar ) printf("  양력 %d.%d.%d → ", sy, sm, sd);
	else printf("  ");
	printf("음력 %d.%d.%d  (%d년 나이 %d세)\r\n", ly, lm, ld, year, g.age);
	printf("  " T_GRAY "상괘 %d+%d=%d → %d   중괘 %d+%d=%d → %d   하괘 %d+%d=%d → %d" T_WHITE "\r\n",
			g.age, g.taese, g.age + g.taese, g.sang, g.days, g.wolgeon, g.days + g.wolgeon, g.jung,
			ld, g.iljin, ld + g.iljin, g.ha);
	printf("  " T_GRAY "(태세 %s, %d월 %s월 %d일, 일진 %s)" T_WHITE "\r\n\r\n", g.year_gapja.c_str(), lm,
			g.month_gapja.c_str(), g.days, g.day_gapja.c_str());

	printf("  " T_YELLOW "【 %d 괘 】" T_WHITE "  %s · %s · %s     한 해 운  ", code,
			sang[g.sang].name, jung[g.jung].name, ha[g.ha].name);
	stars(total_stars);
	printf("\r\n\r\n");
	printf("  " T_CYAN "◆ %s" T_WHITE "\r\n", sang[g.sang].image);
	printf("    %s\r\n    %s\r\n\r\n", sang[g.sang].line1, sang[g.sang].line2);
	printf("  " T_CYAN "◆ 한 해 가운데" T_WHITE "\r\n    %s\r\n\r\n", jung[g.jung].line);
	printf("  " T_CYAN "◆ 한 해 끝" T_WHITE "\r\n    %s\r\n", ha[g.ha].line);
	wait_enter();

	// 2 쪽: 달마다
	snprintf(title, sizeof(title), T_MAGENTA "토정비결" T_WHITE " - %d년 달마다 (%d 괘)", year, code);
	print_header(title);
	printf("\r\n");
	int best = 0, worst = 0, best_v = -99, worst_v = 99;
	for ( int m = 0; m < 12; m++ ) {
		unsigned int h = mix(code * 7919u + m * 104729u + 12345u);
		int v = (int)(h % 5) - 2 + (total > 1 ? 1 : (total < -1 ? -1 : 0));	// -3 .. 3
		const char *line;
		if ( v >= 1 ) line = month_good[mix(h + 1) % COUNT(month_good)];
		else if ( v <= -1 ) line = month_bad[mix(h + 2) % COUNT(month_bad)];
		else line = month_mid[mix(h + 3) % COUNT(month_mid)];
		printf("  %s%-6s" T_WHITE " ", v >= 1 ? T_YELLOW : (v <= -1 ? T_GRAY : T_WHITE), month_name[m]);
		stars(3 + v);
		printf("  %s\r\n", line);
		if ( v > best_v ) { best_v = v; best = m; }
		if ( v < worst_v ) { worst_v = v; worst = m; }
	}
	unsigned int h = mix(code * 31u + 7u);
	printf("\r\n  " T_CYAN "좋은 달" T_WHITE " %s   " T_CYAN "조심할 달" T_WHITE " %s   " T_CYAN "좋은 쪽" T_WHITE " %s   "
			T_CYAN "행운의 색" T_WHITE " %s\r\n", month_name[best], month_name[worst], directions[h % COUNT(directions)],
			colors[mix(h) % COUNT(colors)]);
	printf("  " T_GRAY "(재미로 보는 풀이입니다. 좋은 말은 믿고, 나쁜 말은 조심하는 데만 쓰세요.)" T_WHITE "\r\n");
	wait_enter();
}

// 생일 입력: "1990-5-3" (양력), "음1990-4-9" 또는 "L1990-4-9" (음력)
static bool parse_birth(std::string s, bool *lunar, int *y, int *m, int *d)
{
	*lunar = false;
	if ( s.compare(0, 2, "음") == 0 ) { *lunar = true; s = s.substr(2); }
	else if ( !s.empty() && (s[0] == 'L' || s[0] == 'l') ) { *lunar = true; s = s.substr(1); }
	s = trim(s);
	if ( sscanf(s.c_str(), "%d-%d-%d", y, m, d) != 3 && sscanf(s.c_str(), "%d.%d.%d", y, m, d) != 3 ) return false;
	return *lunar ? valid_lunar(*y, *m, *d, false) : valid_solar(*y, *m, *d);
}

static void reading(const std::string &who, const std::string &birth, int year)
{
	bool lunar;
	int y, m, d;
	if ( !parse_birth(birth, &lunar, &y, &m, &d) ) {
		printf("\r\n  " T_RED "생일은 1990-5-3 (양력) 또는 L1990-4-9 (음력) 처럼 넣으세요. (%d~%d년)" T_WHITE "\r\n", MIN_YEAR, MAX_YEAR);
		wait_enter();
		return;
	}
	int ly = y, lm = m, ld = d;
	if ( !lunar ) {
		bool leap;
		solar_to_lunar(y, m, d, &ly, &lm, &ld, &leap);
	}
	if ( year < ly || year > MAX_YEAR ) {
		printf("\r\n  " T_RED "%d년은 볼 수 없습니다." T_WHITE "\r\n", year);
		wait_enter();
		return;
	}
	show_reading(who, year, ly, lm, ld, !lunar, y, m, d);
}

static int this_lunar_year(void)
{
	time_t t = time(NULL);
	struct tm tm;
	localtime_r(&t, &tm);
	int ly, lm, ld;
	bool leap;
	solar_to_lunar(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, &ly, &lm, &ld, &leap);
	return ly;
}

static void title(void)
{
	while ( 1 ) {
		int year = this_lunar_year();
		print_header(T_MAGENTA "토정비결" T_WHITE);
		printf("\r\n");
		printf("        " T_GRAY "조선의 토정 이지함이 지었다고 전하는 한 해 신수 풀이." T_WHITE "\r\n");
		printf("        " T_GRAY "음력 생년월일로 상괘, 중괘, 하괘를 지어 144 괘 가운데 하나를 얻습니다." T_WHITE "\r\n\r\n");
		bool have = user_birthday.size() >= 10 && user_birthday.compare(0, 4, "0000") != 0;
		if ( have ) {
			printf("        1. 내 %d년 토정비결 " T_GRAY "(회원 정보의 생일 %s, 양력)" T_WHITE "\r\n", year, user_birthday.substr(0, 10).c_str());
			printf("        2. 내 %d년 토정비결 " T_GRAY "(내년)" T_WHITE "\r\n", year + 1);
		} else {
			printf("        " T_GRAY "1. 2. 내 토정비결 (회원 정보에 생일이 없습니다)" T_WHITE "\r\n");
		}
		printf("        3. 다른 생일로 보기 " T_GRAY "(가족, 친구)" T_WHITE "\r\n\r\n");

		std::string c = ask("  선택 (끝내기: Q) >> ", 3);
		if ( c.empty() ) continue;
		if ( !strcasecmp(c.c_str(), "q") || !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "x") ) return;
		int n = atoi(c.c_str());
		if ( (n == 1 || n == 2) && have ) {
			reading(user_nick, user_birthday.substr(0, 10), n == 1 ? year : year + 1);
		} else if ( n == 3 ) {
			std::string b = ask("\r\n  생일 (양력 1990-5-3, 음력은 앞에 L: L1990-4-9) >> ", 14);
			if ( b.empty() ) continue;
			bool lunar;
			int by, bm, bd;
			if ( !parse_birth(b, &lunar, &by, &bm, &bd) ) {
				reading("", b, year);		// 틀린 날짜 안내
				continue;
			}
			std::string y = ask("\r\n  보려는 해 (Enter: 올해) >> ", 4);
			int yy = y.empty() ? year : atoi(y.c_str());
			reading("생일 " + b, b, yy);
		}
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

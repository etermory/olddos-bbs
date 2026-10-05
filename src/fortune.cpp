#include "main.h"

// 오늘의 운세 : 띠별 / 별자리
//   bin/fortune <호스트이름> <아이디> <tty>
// 회원의 생년월일로 띠와 별자리를 정해 바로 보여 준다.
// 운세는 날짜와 띠(별자리)로 정해져서 하루 동안은 같다. (재미로 보는 것)

struct termio sys_term;

char tty[10];

// ansi.h 의 표준 ANSI 색 대신 이야기 터미널 글자색 (ESC[=nF) 을 쓴다
#undef C_WHITE
#define C_WHITE		"\033[=15F"
#undef C_YELLOW
#define C_YELLOW	"\033[=14F"
#undef C_RED
#define C_RED		"\033[=12F"
#undef C_CYAN
#define C_CYAN		"\033[=11F"
#undef C_GREEN
#define C_GREEN		"\033[=10F"
#undef C_MAGENTA
#define C_MAGENTA	"\033[=13F"
#undef C_GRAY
#define C_GRAY		"\033[=7F"

static const char *ddi[] = { "쥐", "소", "호랑이", "토끼", "용", "뱀", "말", "양", "원숭이", "닭", "개", "돼지" };

struct star_sign {
	const char *name;
	int from_m, from_d;		// 시작 날짜
	const char *range;
};

static const star_sign stars[] = {
	{ "물병자리", 1, 20, "1/20~2/18" },
	{ "물고기자리", 2, 19, "2/19~3/20" },
	{ "양자리", 3, 21, "3/21~4/19" },
	{ "황소자리", 4, 20, "4/20~5/20" },
	{ "쌍둥이자리", 5, 21, "5/21~6/21" },
	{ "게자리", 6, 22, "6/22~7/22" },
	{ "사자자리", 7, 23, "7/23~8/22" },
	{ "처녀자리", 8, 23, "8/23~9/23" },
	{ "천칭자리", 9, 24, "9/24~10/22" },
	{ "전갈자리", 10, 23, "10/23~11/22" },
	{ "사수자리", 11, 23, "11/23~12/24" },
	{ "염소자리", 12, 25, "12/25~1/19" },
};

// 운세 문구
static const char *total_msgs[] = {
	"막혔던 일이 술술 풀리는 날입니다. 미뤄 둔 일을 시작해 보세요.",
	"작은 실수가 생길 수 있으니 한 번 더 확인하세요.",
	"뜻밖의 사람에게서 반가운 소식이 옵니다.",
	"서두르면 일을 그르칩니다. 천천히 가는 것이 빠른 길입니다.",
	"주위의 도움으로 어려운 일을 해결합니다. 고마움을 표현하세요.",
	"새로운 것을 배우기 좋은 날입니다. 호기심을 따라가 보세요.",
	"말 한마디가 큰 힘이 됩니다. 칭찬을 아끼지 마세요.",
	"평범하지만 편안한 하루. 쉬어 가는 것도 좋습니다.",
	"오랫동안 준비한 일이 결실을 맺기 시작합니다.",
	"계획에 없던 일이 생겨도 침착하게 대처하면 전화위복이 됩니다.",
	"작은 욕심을 버리면 더 큰 것을 얻습니다.",
	"오래된 친구와 연락해 보세요. 좋은 기운을 얻습니다.",
	"결정을 내리기 좋은 날입니다. 마음이 가는 쪽을 믿으세요.",
	"남의 일에 지나치게 끼어들지 않는 것이 좋습니다.",
	"정리 정돈이 행운을 부릅니다. 주변을 깨끗이 해 보세요.",
	"생각지 못한 곳에서 기회가 찾아옵니다. 눈을 크게 뜨세요.",
};

static const char *money_msgs[] = {
	"뜻밖의 작은 수입이 있습니다.",
	"충동구매를 조심하세요. 지갑을 닫는 것이 이득입니다.",
	"빌려준 돈이나 물건이 돌아옵니다.",
	"큰 지출은 다음으로 미루세요.",
	"아끼는 만큼 쌓이는 날입니다.",
	"모임에서 지출이 생기지만 아깝지 않은 돈입니다.",
	"재물운이 무난합니다. 계획대로 쓰면 탈이 없습니다.",
	"작은 투자보다 저축이 어울리는 날입니다.",
};

static const char *love_msgs[] = {
	"마음에 둔 사람과 가까워질 기회가 옵니다.",
	"사소한 말다툼은 먼저 사과하면 금방 풀립니다.",
	"새로운 인연이 가까이에 있습니다.",
	"상대의 이야기를 끝까지 들어 주세요. 마음이 통합니다.",
	"함께하는 시간이 즐거운 날입니다.",
	"혼자만의 시간이 필요한 날입니다. 무리하지 마세요.",
	"작은 선물이 큰 감동을 줍니다.",
	"오해가 생기기 쉬우니 말을 조심하세요.",
};

static const char *health_msgs[] = {
	"컨디션이 좋습니다. 가벼운 운동을 해 보세요.",
	"눈이 쉽게 피로해집니다. 화면을 오래 보지 마세요.",
	"물을 자주 마시면 몸이 가벼워집니다.",
	"과식을 조심하세요.",
	"일찍 자는 것이 보약입니다.",
	"허리와 어깨를 자주 펴 주세요.",
	"산책이 기분 전환에 좋습니다.",
	"찬 음식은 피하는 것이 좋습니다.",
};

static const char *colors[] = { "빨강", "주황", "노랑", "초록", "파랑", "남색", "보라", "흰색", "검정", "분홍", "하늘색", "금색" };
static const char *directions[] = { "동쪽", "서쪽", "남쪽", "북쪽", "동남쪽", "동북쪽", "서남쪽", "서북쪽" };

#define COUNT(a)	((int)(sizeof(a) / sizeof(a[0])))

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
	database::close();
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

// 오늘 날짜와 띠(별자리) 번호로 정해지는 수 (하루 동안 같다)
static unsigned int seed_of(int kind, int index, int salt)
{
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	unsigned int h = (tm->tm_year + 1900) * 10000 + (tm->tm_mon + 1) * 100 + tm->tm_mday;
	h = h * 2654435761u + kind * 97 + index * 7919 + salt * 104729;
	h ^= h >> 13;
	h *= 0x5bd1e995;
	h ^= h >> 15;
	return h;
}

// 생년월일의 띠 (입춘 2/4 전이면 앞 해)
int ddi_of(int y, int m, int d)
{
	if ( m < 2 || (m == 2 && d < 4) ) y--;
	return ((y - 4) % 12 + 12) % 12;
}

int star_of(int m, int d)
{
	int md = m * 100 + d;
	for ( int i = COUNT(stars) - 1; i >= 0; i-- ) {
		if ( md >= stars[i].from_m * 100 + stars[i].from_d ) return i;
	}
	return COUNT(stars) - 1;	// 1/1~1/19 는 염소자리
}

void stars_line(const char *label, unsigned int s)
{
	int n = 1 + s % 5;
	printf("  %-10s ", label);
	printf(C_YELLOW);
	for ( int i = 0; i < 5; i++ ) printf("%s", i < n ? "★" : "☆");
	printf(C_WHITE);
}

// kind 0: 띠, 1: 별자리
void print_fortune(int kind, int index)
{
	unsigned int s = seed_of(kind, index, 0);

	printf("\r\n");
	stars_line("총운", s);
	printf("\r\n    %s\r\n\r\n", total_msgs[seed_of(kind, index, 1) % COUNT(total_msgs)]);
	stars_line("금전운", seed_of(kind, index, 2));
	printf("  %s\r\n", money_msgs[seed_of(kind, index, 3) % COUNT(money_msgs)]);
	stars_line("애정운", seed_of(kind, index, 4));
	printf("  %s\r\n", love_msgs[seed_of(kind, index, 5) % COUNT(love_msgs)]);
	stars_line("건강운", seed_of(kind, index, 6));
	printf("  %s\r\n", health_msgs[seed_of(kind, index, 7) % COUNT(health_msgs)]);

	printf("\r\n  행운의 숫자 " C_CYAN "%d" C_WHITE "   행운의 색 " C_CYAN "%s" C_WHITE "   행운의 방향 " C_CYAN "%s" C_WHITE "\r\n",
			1 + seed_of(kind, index, 8) % 45,
			colors[seed_of(kind, index, 9) % COUNT(colors)],
			directions[seed_of(kind, index, 10) % COUNT(directions)]);
}

void show_ddi(int i)
{
	char head[128];
	snprintf(head, sizeof(head), "오늘의 운세 - %s띠", ddi[i]);
	print_header(head);
	print_fortune(0, i);
}

void show_star(int i)
{
	char head[128];
	snprintf(head, sizeof(head), "오늘의 운세 - %s (%s)", stars[i].name, stars[i].range);
	print_header(head);
	print_fortune(1, i);
}

std::string ask(const char *msg)
{
	char cmd[64];
	printf(ESC_ENG);
	printf("%s", msg);
	line_input(cmd, 20);
	return trim(cmd);
}

void wait_enter(void)
{
	printf("\r\n " C_GRAY "재미로 보는 운세입니다.  [Enter] 를 누르세요." C_WHITE);
	press_enter();
}

// 띠나 별자리를 골라서 보기
void choose(int kind)
{
	print_header(kind == 0 ? "띠별 운세" : "별자리 운세");
	printf("\r\n");
	int n = (kind == 0) ? COUNT(ddi) : COUNT(stars);
	for ( int i = 0; i < n; i++ ) {
		if ( kind == 0 ) printf("  " C_YELLOW "%2d" C_WHITE ". %-8s", i + 1, (std::string(ddi[i]) + "띠").c_str());
		else printf("  " C_YELLOW "%2d" C_WHITE ". %-10s %-12s", i + 1, stars[i].name, stars[i].range);
		if ( (i + 1) % (kind == 0 ? 4 : 2) == 0 ) printf("\r\n");
	}
	std::string c = ask("\r\n 번호 (P:상위메뉴) >> ");
	int k = atoi(c.c_str());
	if ( k < 1 || k > n ) return;
	if ( kind == 0 ) show_ddi(k - 1); else show_star(k - 1);
	wait_enter();
}

int main(int argc, char **argv)
{
	std::string user_id = (argc > 2) ? argv[2] : "";
	snprintf(tty, sizeof(tty), "%s", argc > 3 ? argv[3] : "");

    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGHUP, (__sighandler_t)host_close);
    signal(SIGSEGV, (__sighandler_t)host_close);
    signal(SIGBUS, (__sighandler_t)host_close);

	read_settings("hanulso.cfg");

    ioctl(0,TCGETA, &sys_term);
	raw_mode();
    umask(0111);

	// 회원 생년월일
	int by = 0, bm = 0, bd = 0;
	if ( !user_id.empty() && database::open() ) {
		bool exist;
		std::map<std::string, std::string> user = database::user_info((char*)user_id.c_str(), &exist);
		if ( exist ) sscanf(user["BIRTHDAY"].c_str(), "%d-%d-%d", &by, &bm, &bd);
	}
	bool has_birth = by > 1900 && bm >= 1 && bm <= 12 && bd >= 1 && bd <= 31;

	while (1) {
		if ( has_birth ) {
			int di = ddi_of(by, bm, bd);
			int si = star_of(bm, bd);
			char head[128];
			snprintf(head, sizeof(head), "오늘의 운세 - %s띠, %s", ddi[di], stars[si].name);
			print_header(head);
			print_fortune(0, di);
		} else {
			print_header("오늘의 운세");
			printf("\r\n  생년월일 정보가 없어 내 운세를 볼 수 없습니다. (PE 로 회원 정보 수정)\r\n");
		}

		printf("\r\n" C_GRAY " %s" C_WHITE, repeat("─", 39).c_str());
		std::string c = ask(has_birth ?
				"\r\n [S]내 별자리 운세  [1]띠별 운세  [2]별자리 운세  [P]상위메뉴 >> " :
				"\r\n [1]띠별 운세  [2]별자리 운세  [P]상위메뉴 >> ");

		if ( !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "x") ) break;
		if ( !strcasecmp(c.c_str(), "s") && has_birth ) {
			show_star(star_of(bm, bd));
			wait_enter();
		} else if ( c == "1" ) {
			choose(0);
		} else if ( c == "2" ) {
			choose(1);
		}
	}

	database::close();
	ioctl(0, TCSETAF, &sys_term);
	return 0;
}

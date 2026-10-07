#include "main.h"
#include <sys/time.h>

// ------------------------------------------------------------------
// 타자 연습 : 보여 주는 글을 한 줄씩 따라 친다
//   bin/typing <호스트이름> <아이디> <tty>
//   1) 짧은 글 (속담 10 개)  2) 긴 글 (시, 글 한 편)  3) 영문 (문장 10 개)
//   4) 오늘의 도전: 하루 동안 모두 같은 긴 글, 오늘 순위 (가장 좋은 기록)
//   타수 = 맞게 친 글자의 글쇠 수 (한글은 낱자, 겹모음 / 겹받침은 2) / 분.
//   정확도 = 맞게 친 글자 / 원문과 친 글 중 긴 쪽 (빠뜨리거나 더 친 글자도 맞춰 비교).
//   정확도 90% 이상이어야 명예의 전당에 오른다.
// ------------------------------------------------------------------

struct termio sys_term;

char tty[10];

static std::string user_id;
static std::string user_nick;

#define T_WHITE		"\033[=15F"
#define T_YELLOW	"\033[=14F"
#define T_RED		"\033[=12F"
#define T_GREEN		"\033[=10F"
#define T_CYAN		"\033[=11F"
#define T_GRAY		"\033[=7F"
#define T_MAGENTA	"\033[=13F"

#define MIN_ACC		90		// 기록에 남는 정확도
#define INPUT_LEN	70		// 한 줄 입력 (앞의 "  입력> " 8 칸과 함께 78 칸)

enum { MODE_SHORT = 1, MODE_LONG = 2, MODE_ENG = 3 };

// ------------------------------------------------------------------
// 글감 (저작권이 끝난 글, 속담, 새로 쓴 글)
// ------------------------------------------------------------------
struct passage {
	const char *title;
	const char *lines[12];
};

static const passage long_passages[] = {
	{ "서시 (윤동주)", {
		"죽는 날까지 하늘을 우러러 한 점 부끄럼이 없기를,",
		"잎새에 이는 바람에도 나는 괴로워했다.",
		"별을 노래하는 마음으로 모든 죽어 가는 것을 사랑해야지",
		"그리고 나한테 주어진 길을 걸어가야겠다.",
		"오늘 밤에도 별이 바람에 스치운다.", NULL } },
	{ "진달래꽃 (김소월)", {
		"나 보기가 역겨워 가실 때에는 말없이 고이 보내 드리우리다",
		"영변에 약산 진달래꽃 아름 따다 가실 길에 뿌리우리다",
		"가시는 걸음 걸음 놓인 그 꽃을 사뿐히 즈려밟고 가시옵소서",
		"나 보기가 역겨워 가실 때에는 죽어도 아니 눈물 흘리우리다", NULL } },
	{ "엄마야 누나야 (김소월)", {
		"엄마야 누나야 강변 살자",
		"뜰에는 반짝이는 금모래 빛",
		"뒷문 밖에는 갈잎의 노래",
		"엄마야 누나야 강변 살자", NULL } },
	{ "님의 침묵 (한용운)", {
		"님은 갔습니다. 아아, 사랑하는 나의 님은 갔습니다.",
		"푸른 산빛을 깨치고 단풍나무 숲을 향하여 난 작은 길을 걸어서,",
		"차마 떨치고 갔습니다.",
		"황금의 꽃같이 굳고 빛나던 옛 맹서는 차디찬 티끌이 되어서,",
		"한숨의 미풍에 날아갔습니다.", NULL } },
	{ "애국가 (1, 2 절)", {
		"동해 물과 백두산이 마르고 닳도록 하느님이 보우하사 우리나라 만세",
		"무궁화 삼천리 화려 강산 대한 사람 대한으로 길이 보전하세",
		"남산 위에 저 소나무 철갑을 두른 듯 바람 서리 불변함은 우리 기상일세",
		"무궁화 삼천리 화려 강산 대한 사람 대한으로 길이 보전하세", NULL } },
	{ "훈민정음 서문", {
		"나라의 말이 중국과 달라 문자와 서로 통하지 아니하니,",
		"이런 까닭으로 어리석은 백성이 이르고자 하는 바가 있어도",
		"마침내 제 뜻을 능히 펴지 못하는 사람이 많으니라.",
		"내가 이를 위하여 가엾게 여겨 새로 스물여덟 글자를 만드니,",
		"사람마다 하여금 쉬이 익혀 날마다 씀에 편안케 하고자 할 따름이니라.", NULL } },
	{ "PC통신의 밤", {
		"모뎀이 삐삐 소리를 내며 전화를 걸면 마음이 두근거렸다.",
		"접속이 되면 파란 화면에 하얀 글씨로 반가운 인사가 떴다.",
		"밤새 동호회 게시판을 읽고 대화방에서 모르는 사람과 웃었다.",
		"전화 요금이 많이 나와 어머니께 꾸중을 들은 날도 있었다.",
		"그래도 그 밤들은 오래도록 따뜻한 추억으로 남아 있다.", NULL } },
};
#define LONG_N	(int)(sizeof(long_passages) / sizeof(long_passages[0]))

static const passage eng_passages[] = {
	{ "Gettysburg Address (Lincoln)", {
		"Four score and seven years ago our fathers brought forth on",
		"this continent, a new nation, conceived in Liberty, and",
		"dedicated to the proposition that all men are created equal.",
		"Now we are engaged in a great civil war, testing whether that",
		"nation, or any nation so conceived and so dedicated,",
		"can long endure.", NULL } },
	{ "Hamlet (Shakespeare)", {
		"To be, or not to be, that is the question:",
		"Whether 'tis nobler in the mind to suffer",
		"The slings and arrows of outrageous fortune,",
		"Or to take arms against a sea of troubles",
		"And by opposing end them.", NULL } },
};
#define ENG_LONG_N	(int)(sizeof(eng_passages) / sizeof(eng_passages[0]))

static const char *proverbs[] = {
	"가는 말이 고와야 오는 말이 곱다", "낮말은 새가 듣고 밤말은 쥐가 듣는다", "발 없는 말이 천 리 간다",
	"세 살 버릇 여든까지 간다", "천 리 길도 한 걸음부터", "티끌 모아 태산",
	"원숭이도 나무에서 떨어질 때가 있다", "백지장도 맞들면 낫다", "소 잃고 외양간 고친다",
	"윗물이 맑아야 아랫물이 맑다", "될성부른 나무는 떡잎부터 알아본다", "등잔 밑이 어둡다",
	"구슬이 서 말이라도 꿰어야 보배", "호랑이도 제 말 하면 온다", "하늘이 무너져도 솟아날 구멍이 있다",
	"고생 끝에 낙이 온다", "돌다리도 두들겨 보고 건너라", "열 번 찍어 안 넘어가는 나무 없다",
	"아니 땐 굴뚝에 연기 날까", "빈 수레가 요란하다", "믿는 도끼에 발등 찍힌다",
	"우물 안 개구리", "개구리 올챙이 적 생각 못 한다", "가랑비에 옷 젖는 줄 모른다",
	"작은 고추가 더 맵다", "공든 탑이 무너지랴", "꿩 먹고 알 먹는다",
	"누워서 떡 먹기", "말 한마디로 천 냥 빚을 갚는다", "시작이 반이다",
	NULL };

static const char *eng_sentences[] = {
	"The quick brown fox jumps over the lazy dog.", "Sphinx of black quartz, judge my vow.",
	"How vexingly quick daft zebras jump!", "The five boxing wizards jump quickly.",
	"Practice makes perfect.", "Where there is a will, there is a way.",
	"Knowledge is power.", "Time is money.", "An apple a day keeps the doctor away.",
	"All that glitters is not gold.", "Actions speak louder than words.",
	"Rome was not built in a day.", "Better late than never.",
	"The pen is mightier than the sword.", "Hello, welcome to the old DOS museum BBS.",
	"Slow and steady wins the race.", "Every cloud has a silver lining.",
	"Do not count your chickens before they hatch.",
	NULL };

// ------------------------------------------------------------------
// 터미널
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

static long now_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000L + tv.tv_usec / 1000;
}

static void wait_enter(void)
{
	printf("\r\n " T_GRAY "[Enter] 를 누르세요." T_WHITE);
	press_enter();
}

static std::string today(void)
{
	time_t t = time(NULL);
	struct tm tm;
	localtime_r(&t, &tm);
	char b[16];
	snprintf(b, sizeof(b), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	return b;
}

static unsigned long today_seed(void)
{
	time_t t = time(NULL);
	struct tm tm;
	localtime_r(&t, &tm);
	return (unsigned long)((tm.tm_year + 1900) * 1000 + tm.tm_yday);
}

// ------------------------------------------------------------------
// 글자와 글쇠 수
// ------------------------------------------------------------------

// 완성형 글을 글자 단위로 (한글 / 기호는 2 바이트)
static std::vector<std::string> split_chars(const std::string &s)
{
	std::vector<std::string> r;
	for ( unsigned int i = 0; i < s.size(); ) {
		int l = ((unsigned char)s[i] >= 0x81 && i + 1 < s.size()) ? 2 : 1;
		r.push_back(s.substr(i, l));
		i += l;
	}
	return r;
}

// 완성형 두 바이트 -> 유니코드 (못 바꾸면 0)
static int ks_to_ucs(unsigned char a, unsigned char b)
{
	static iconv_t cd = (iconv_t)-1;
	if ( cd == (iconv_t)-1 ) cd = iconv_open("UCS-2BE", "CP949");
	if ( cd == (iconv_t)-1 ) return 0;
	char in[2] = { (char)a, (char)b };
	char out[4];
	char *ip = in, *op = out;
	size_t il = 2, ol = sizeof(out);
	if ( iconv(cd, &ip, &il, &op, &ol) == (size_t)-1 || ol != sizeof(out) - 2 ) {
		iconv(cd, NULL, NULL, NULL, NULL);
		return 0;
	}
	return ((unsigned char)out[0] << 8) | (unsigned char)out[1];
}

// 한 글자를 치는 글쇠 수. 한글은 초성 + 중성 + 종성 (겹모음 ㅘㅙㅚㅝㅞㅟㅢ, 겹받침은 2)
static int strokes(const std::string &ch)
{
	if ( ch.size() == 1 ) return 1;
	int u = ks_to_ucs(ch[0], ch[1]);
	if ( u < 0xAC00 || u > 0xD7A3 ) return 1;
	int s = u - 0xAC00;
	int jung = (s / 28) % 21, jong = s % 28;
	int n = 1;
	n += (jung == 9 || jung == 10 || jung == 11 || jung == 14 || jung == 15 || jung == 16 || jung == 19) ? 2 : 1;
	if ( jong ) n += (jong == 3 || jong == 5 || jong == 6 || (jong >= 9 && jong <= 15) || jong == 18) ? 2 : 1;
	return n;
}

// 원문과 친 글을 맞춰 본다 (가장 긴 공통 부분열). ok[i] = 친 글의 i 번째 글자가 맞음
static int match_chars(const std::vector<std::string> &t, const std::vector<std::string> &y,
		std::vector<bool> &ok, int *ok_strokes)
{
	int n = t.size(), m = y.size();
	std::vector<std::vector<short> > L(n + 1, std::vector<short>(m + 1, 0));
	for ( int i = n - 1; i >= 0; i-- )
		for ( int j = m - 1; j >= 0; j-- )
			L[i][j] = t[i] == y[j] ? L[i + 1][j + 1] + 1 : std::max(L[i + 1][j], L[i][j + 1]);
	ok.assign(m, false);
	*ok_strokes = 0;
	int i = 0, j = 0;
	while ( i < n && j < m ) {
		if ( t[i] == y[j] ) { ok[j] = true; *ok_strokes += strokes(y[j]); i++; j++; }
		else if ( L[i + 1][j] >= L[i][j + 1] ) i++;
		else j++;
	}
	return L[0][0];
}

// ------------------------------------------------------------------
// DB
// ------------------------------------------------------------------
static void create_tables(void)
{
	// MODE: 1 짧은 글, 2 긴 글, 3 영문
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS game_typing ( "
			"USER_ID VARCHAR(50) NOT NULL, "
			"MODE INT NOT NULL, "
			"NAME VARCHAR(50) NOT NULL, "
			"SPEED INT NOT NULL, "				/* 가장 빠른 타수 (정확도 90% 이상) */
			"ACC INT NOT NULL, "
			"PLAYS INT NOT NULL, "
			"DATE_TIME DATETIME NOT NULL, "
			"PRIMARY KEY (USER_ID, MODE) )");
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS game_typing_daily ( "
			"DAY DATE NOT NULL, "
			"USER_ID VARCHAR(50) NOT NULL, "
			"NAME VARCHAR(50) NOT NULL, "
			"SPEED INT NOT NULL, "
			"ACC INT NOT NULL, "
			"TRIES INT NOT NULL, "
			"DATE_TIME DATETIME NOT NULL, "
			"PRIMARY KEY (DAY, USER_ID) )");
}

// 기록 저장. 더 빠르면 바꾼다. 정확도가 모자라면 판 수만
static void save_score(int mode, int speed, int acc)
{
	char q[1024];
	std::string id = database::escape(user_id.c_str());
	std::string name = database::escape(user_nick.c_str());
	int s = acc >= MIN_ACC ? speed : 0;
	snprintf(q, sizeof(q), "INSERT INTO game_typing (USER_ID, MODE, NAME, SPEED, ACC, PLAYS, DATE_TIME) "
			"VALUES ('%s', %d, '%s', %d, %d, 1, NOW()) ON DUPLICATE KEY UPDATE NAME='%s', PLAYS=PLAYS+1, "
			"DATE_TIME=IF(%d > SPEED, NOW(), DATE_TIME), ACC=IF(%d > SPEED, %d, ACC), SPEED=GREATEST(SPEED, %d)",
			id.c_str(), mode, name.c_str(), s, acc, name.c_str(), s, s, acc, s);
	mysql_query(mysql, q);
}

static void save_daily(int speed, int acc)
{
	char q[1024];
	std::string id = database::escape(user_id.c_str());
	std::string name = database::escape(user_nick.c_str());
	int s = acc >= MIN_ACC ? speed : 0;
	snprintf(q, sizeof(q), "INSERT INTO game_typing_daily (DAY, USER_ID, NAME, SPEED, ACC, TRIES, DATE_TIME) "
			"VALUES ('%s', '%s', '%s', %d, %d, 1, NOW()) ON DUPLICATE KEY UPDATE NAME='%s', TRIES=TRIES+1, "
			"DATE_TIME=IF(%d > SPEED, NOW(), DATE_TIME), ACC=IF(%d > SPEED, %d, ACC), SPEED=GREATEST(SPEED, %d)",
			today().c_str(), id.c_str(), name.c_str(), s, acc, name.c_str(), s, s, acc, s);
	mysql_query(mysql, q);
}

// ------------------------------------------------------------------
// 연습
// ------------------------------------------------------------------

// 줄들을 차례로 친다. 다 치면 true (중간에 그만두면 false). 타수, 정확도를 돌려준다
static bool practice(const std::string &title, const std::vector<std::string> &lines, bool hangul,
		int *speed_out, int *acc_out)
{
	long total_ms = 0;
	int total_ok_strokes = 0, total_ok = 0, total_len = 0;

	print_header((T_CYAN "타자 연습 - " T_WHITE + title).c_str());
	printf("\r\n  " T_GRAY "노란 글을 똑같이 치고 Enter. 첫 글자부터 잽니다. 빈 줄로 Enter 는 그만두기" T_WHITE "\r\n");

	for ( unsigned int l = 0; l < lines.size(); l++ ) {
		std::vector<std::string> t = split_chars(lines[l]);

		printf("\r\n  " T_GRAY "%2d/%-2d" T_WHITE " " T_YELLOW "%s" T_WHITE "\r\n", l + 1, (int)lines.size(), lines[l].c_str());
		printf(hangul ? ESC_HAN : ESC_ENG);
		printf("  입력> ");
		fflush(stdout);

		// 첫 글자를 치는 순간부터
		int c = getchar();
		if ( c == EOF ) host_close();
		long t0 = now_ms();
		ungetc(c, stdin);
		char buf[INPUT_LEN + 8];
		line_input(buf, INPUT_LEN);
		long ms = now_ms() - t0;
		printf(ESC_ENG);

		if ( trim(buf).empty() ) {
			printf("\r\n  그만둘까요? (y/N) ");
			if ( yesno(NO) == YES ) return false;
			l--;
			continue;
		}

		// 친 줄을 다시 찍는다: 틀린 글자는 빨강
		std::vector<std::string> y = split_chars(buf);
		std::vector<bool> ok;
		int ok_strokes;
		int n_ok = match_chars(t, y, ok, &ok_strokes);
		printf("\r  입력> ");
		for ( unsigned int k = 0; k < y.size(); k++ ) {
			printf("%s%s", ok[k] ? T_WHITE : T_RED, y[k] == " " && !ok[k] ? "_" : y[k].c_str());
		}
		printf(T_WHITE "\033[K");

		int len = std::max(t.size(), y.size());
		if ( ms < 500 ) ms = 500;
		int speed = (int)(ok_strokes * 60000L / ms);
		int acc = len ? n_ok * 100 / len : 0;
		printf("\r\n        %s%4d 타" T_WHITE "  정확도 %s%3d%%" T_WHITE "  " T_GRAY "%ld.%ld초" T_WHITE,
				T_CYAN, speed, acc >= MIN_ACC ? T_GREEN : T_RED, acc, ms / 1000, (ms % 1000) / 100);

		total_ms += ms;
		total_ok_strokes += ok_strokes;
		total_ok += n_ok;
		total_len += len;
		printf("\r\n");
	}

	*speed_out = total_ms ? (int)(total_ok_strokes * 60000L / total_ms) : 0;
	*acc_out = total_len ? total_ok * 100 / total_len : 0;

	printf("\r\n  " T_GRAY "%s" T_WHITE "\r\n", repeat("─", 36).c_str());
	printf("  결과:  평균 " T_CYAN "%d 타" T_WHITE "/분,  정확도 %s%d%%" T_WHITE ",  걸린 시간 %ld.%ld초\r\n",
			*speed_out, *acc_out >= MIN_ACC ? T_GREEN : T_RED, *acc_out, total_ms / 1000, (total_ms % 1000) / 100);
	if ( *acc_out < MIN_ACC ) {
		printf("  " T_GRAY "정확도가 %d%% 이상이어야 기록에 남습니다." T_WHITE "\r\n", MIN_ACC);
	}
	return true;
}

// 짧은 글 / 영문 문장: 무작위 10 개
static std::vector<std::string> pick_sentences(const char **list, int count)
{
	int n = 0;
	while ( list[n] ) n++;
	std::vector<int> idx;
	for ( int i = 0; i < n; i++ ) idx.push_back(i);
	for ( int i = n - 1; i > 0; i-- ) std::swap(idx[i], idx[rand() % (i + 1)]);
	std::vector<std::string> r;
	for ( int i = 0; i < count && i < n; i++ ) r.push_back(list[idx[i]]);
	return r;
}

static std::vector<std::string> passage_lines(const passage &p)
{
	std::vector<std::string> r;
	for ( int i = 0; p.lines[i]; i++ ) r.push_back(p.lines[i]);
	return r;
}

// 긴 글 고르기 (Enter 면 아무거나). 취소면 -1
static int choose_passage(const passage *list, int n, const char *head)
{
	print_header((T_CYAN "타자 연습 - " T_WHITE + std::string(head)).c_str());
	printf("\r\n");
	for ( int i = 0; i < n; i++ ) {
		printf("        %2d. %s\r\n", i + 1, list[i].title);
	}
	char cmd[8];
	printf(ESC_ENG);
	printf("\r\n  번호 (Enter: 아무거나, 그만: Q) >> ");
	line_input(cmd, 3);
	std::string c = trim(cmd);
	if ( !strcasecmp(c.c_str(), "q") || !strcasecmp(c.c_str(), "p") ) return -1;
	if ( c.empty() ) return rand() % n;
	int k = atoi(c.c_str());
	return (k >= 1 && k <= n) ? k - 1 : -1;
}

static void play_mode(int mode)
{
	std::vector<std::string> lines;
	std::string title;
	bool hangul = mode != MODE_ENG;
	if ( mode == MODE_SHORT ) {
		lines = pick_sentences(proverbs, 10);
		title = "짧은 글 (속담)";
	} else if ( mode == MODE_LONG ) {
		int k = choose_passage(long_passages, LONG_N, "긴 글");
		if ( k < 0 ) return;
		lines = passage_lines(long_passages[k]);
		title = long_passages[k].title;
	} else {
		print_header(T_CYAN "타자 연습 - 영문" T_WHITE);
		printf("\r\n        1. 짧은 문장 10 개\r\n");
		for ( int i = 0; i < ENG_LONG_N; i++ ) printf("        %d. %s\r\n", i + 2, eng_passages[i].title);
		char cmd[8];
		printf(ESC_ENG);
		printf("\r\n  번호 (Enter: 짧은 문장, 그만: Q) >> ");
		line_input(cmd, 3);
		std::string c = trim(cmd);
		if ( !strcasecmp(c.c_str(), "q") || !strcasecmp(c.c_str(), "p") ) return;
		int k = c.empty() ? 1 : atoi(c.c_str());
		if ( k == 1 ) {
			lines = pick_sentences(eng_sentences, 10);
			title = "영문 (짧은 문장)";
		} else if ( k >= 2 && k < 2 + ENG_LONG_N ) {
			lines = passage_lines(eng_passages[k - 2]);
			title = eng_passages[k - 2].title;
		} else {
			return;
		}
	}

	int speed, acc;
	if ( !practice(title, lines, hangul, &speed, &acc) ) return;
	save_score(mode, speed, acc);
	wait_enter();
}

// ------------------------------------------------------------------
// 순위
// ------------------------------------------------------------------
static void show_rank(void)
{
	static const char *names[] = { "", "◆ 짧은 글", "◆ 긴 글", "◆ 영문" };
	print_header(T_CYAN "타자 연습 명예의 전당" T_WHITE);
	for ( int mode = 1; mode <= 3; mode++ ) {
		char q[256];
		snprintf(q, sizeof(q), "SELECT * FROM game_typing WHERE MODE=%d AND SPEED > 0 ORDER BY SPEED DESC, ACC DESC LIMIT 4", mode);
		std::vector<std::map<std::string, std::string> > rows = database::fetch_rows(q);
		printf("  " T_YELLOW "%s" T_WHITE "\r\n", names[mode]);
		if ( rows.size() == 0 ) printf("  " T_GRAY "    아직 기록이 없습니다." T_WHITE "\r\n");
		for ( unsigned int i = 0; i < rows.size(); i++ ) {
			bool me = rows[i]["USER_ID"] == user_id;
			std::string d = rows[i]["DATE_TIME"].size() >= 10 ? rows[i]["DATE_TIME"].substr(0, 10) : "";
			printf("  %s%4d  %-16s %5s 타  %3s%%  %4s판  %s" T_WHITE "\r\n", me ? T_YELLOW : T_WHITE, i + 1,
					string_truncate(display_text(rows[i]["NAME"]), 16, "").c_str(),
					rows[i]["SPEED"].c_str(), rows[i]["ACC"].c_str(), rows[i]["PLAYS"].c_str(), d.c_str());
		}
	}
	wait_enter();
}

static void show_daily_rank(void)
{
	const passage &p = long_passages[today_seed() % LONG_N];
	print_header(T_CYAN "타자 연습 - 오늘의 도전 순위" T_WHITE);
	std::string q = "SELECT * FROM game_typing_daily WHERE DAY='" + today() + "' ORDER BY SPEED DESC, ACC DESC LIMIT 14";
	std::vector<std::map<std::string, std::string> > rows = database::fetch_rows((char*)q.c_str());
	printf("\r\n  " T_GRAY "%s, 오늘의 글: " T_WHITE "%s" T_GRAY " (가장 좋은 기록)" T_WHITE "\r\n\r\n", today().c_str(), p.title);
	printf("  " T_GRAY "%4s  %-16s %8s  %5s  %s" T_WHITE "\r\n", "순위", "이름", "타수", "정확도", "도전");
	if ( rows.size() == 0 ) printf("  " T_GRAY "    아직 아무도 도전하지 않았습니다." T_WHITE "\r\n");
	int rank = 0;
	for ( unsigned int i = 0; i < rows.size(); i++ ) {
		bool me = rows[i]["USER_ID"] == user_id;
		bool rec = atoi(rows[i]["SPEED"].c_str()) > 0;
		if ( rec ) rank++;
		printf("  %s%4s  %-16s %5s 타  %5s%%  %s번" T_WHITE "\r\n", me ? T_YELLOW : T_WHITE,
				rec ? TO_STRING(rank).c_str() : "-",
				string_truncate(display_text(rows[i]["NAME"]), 16, "").c_str(),
				rows[i]["SPEED"].c_str(), rows[i]["ACC"].c_str(), rows[i]["TRIES"].c_str());
	}
	wait_enter();
}

static void daily(void)
{
	const passage &p = long_passages[today_seed() % LONG_N];
	int speed, acc;
	if ( !practice(std::string("오늘의 도전: ") + p.title, passage_lines(p), true, &speed, &acc) ) return;
	save_daily(speed, acc);
	save_score(MODE_LONG, speed, acc);
	wait_enter();
	show_daily_rank();
}

static void title(void)
{
	while ( 1 ) {
		print_header(T_CYAN "타자 연습" T_WHITE);
		printf("\r\n");
		printf("        " T_YELLOW "⊙" T_WHITE "  보여 주는 글을 한 줄씩 똑같이 쳐 보세요.\r\n");
		printf("        " T_GRAY "   타수는 1 분에 맞게 친 글쇠 수, 정확도 %d%% 이상이면 기록에 남습니다." T_WHITE "\r\n\r\n", MIN_ACC);
		printf("        1. 짧은 글 (속담 10 개)\r\n");
		printf("        2. 긴 글 (시와 글 한 편)\r\n");
		printf("        3. 영문\r\n");
		printf("        4. " T_MAGENTA "오늘의 도전" T_WHITE " (모두 같은 긴 글, 오늘 순위: %s)\r\n",
				long_passages[today_seed() % LONG_N].title);
		printf("        5. 명예의 전당\r\n");
		printf("        6. 오늘의 도전 순위\r\n\r\n");

		char cmd[16];
		printf(ESC_ENG);
		printf("  선택 (끝내기: Q) >> ");
		line_input(cmd, 3);
		std::string c = trim(cmd);
		if ( c.empty() ) continue;
		if ( !strcasecmp(c.c_str(), "q") || !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "x") ) return;
		int n = atoi(c.c_str());
		if ( n >= 1 && n <= 3 ) play_mode(n);
		else if ( n == 4 ) daily();
		else if ( n == 5 ) show_rank();
		else if ( n == 6 ) show_daily_rank();
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

	srand(time(NULL) ^ getpid());

	if ( database::open() == false )
		exit(1);
	create_tables();

	bool exist;
	std::map<std::string, std::string> user = database::user_info((char*)user_id.c_str(), &exist);
	user_nick = display_text(user["NICK_NAME"]);
	if ( user_nick.empty() ) user_nick = user_id;

	title();

	host_close();
	return 0;
}

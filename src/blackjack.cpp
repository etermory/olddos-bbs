#include "main.h"

// ------------------------------------------------------------------
// 블랙잭
//   bin/blackjack <호스트이름> <아이디> <tty>
//   21 에 가깝게, 넘으면 진다. A 는 1 또는 11, 그림 카드는 10.
//   H 히트(한 장 더), S 스탠드(그만), D 더블다운(걸기 두 배, 한 장만).
//   블랙잭(처음 두 장이 21)은 1.5 배. 딜러는 17 이상이면 멈춘다.
//   칩은 회원마다 남는다 (처음 1000). 다 잃으면 하루에 한 번 1000 을 다시 받는다.
// ------------------------------------------------------------------

struct termio sys_term;

char tty[10];

static std::string user_id;
static std::string user_nick;

#define J_WHITE		"\033[=15F"
#define J_YELLOW	"\033[=14F"
#define J_RED		"\033[=12F"
#define J_GREEN		"\033[=10F"
#define J_CYAN		"\033[=11F"
#define J_GRAY		"\033[=7F"
#define J_MAGENTA	"\033[=13F"

#define START_CHIPS	1000
#define MIN_BET		10

static long chips;
static long last_bet = 50;
static std::vector<int> shoe;		// 카드 0~51 (무늬 = /13, 숫자 = %13)

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
	printf(J_WHITE);
	fflush(stdout);
	database::close();
    ioctl(0, TCSETAF, &sys_term);
    exit(1);
}

static void at(int row, int col)
{
	printf("\033[%d;%dH", row, col);
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

static void wait_enter(void)
{
	printf("\r\n " J_GRAY "[Enter] 를 누르세요." J_WHITE);
	press_enter();
}

// ------------------------------------------------------------------
// DB
// ------------------------------------------------------------------
static void create_tables(void)
{
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS game_blackjack ( "
			"USER_ID VARCHAR(50) NOT NULL PRIMARY KEY, "
			"NAME VARCHAR(50) NOT NULL, "
			"CHIPS INT NOT NULL, "
			"BEST INT NOT NULL, "				/* 가장 많이 가졌던 칩 */
			"HANDS INT NOT NULL, "
			"WINS INT NOT NULL, "
			"REFILL DATE, "						/* 마지막으로 칩을 다시 받은 날 */
			"DATE_TIME DATETIME NOT NULL )");
}

static void load_chips(void)
{
	std::string q = "SELECT CHIPS FROM game_blackjack WHERE USER_ID='" + database::escape(user_id.c_str()) + "'";
	bool ok;
	std::string r = database::fetch((char*)q.c_str(), &ok);
	if ( r.empty() ) {
		chips = START_CHIPS;
		char ins[512];
		snprintf(ins, sizeof(ins), "INSERT IGNORE INTO game_blackjack (USER_ID, NAME, CHIPS, BEST, HANDS, WINS, DATE_TIME) "
				"VALUES ('%s', '%s', %d, %d, 0, 0, NOW())", database::escape(user_id.c_str()).c_str(),
				database::escape(user_nick.c_str()).c_str(), START_CHIPS, START_CHIPS);
		mysql_query(mysql, ins);
	} else {
		chips = atol(r.c_str());
	}
}

static void save_hand(bool won)
{
	char q[512];
	snprintf(q, sizeof(q), "UPDATE game_blackjack SET NAME='%s', CHIPS=%ld, BEST=GREATEST(BEST, %ld), HANDS=HANDS+1, "
			"WINS=WINS+%d, DATE_TIME=NOW() WHERE USER_ID='%s'", database::escape(user_nick.c_str()).c_str(),
			chips, chips, won ? 1 : 0, database::escape(user_id.c_str()).c_str());
	mysql_query(mysql, q);
}

// 다 잃었을 때: 하루 한 번 다시 받는다. 받았으면 true
static bool refill(void)
{
	std::string id = database::escape(user_id.c_str());
	bool ok;
	std::string q = "SELECT COUNT(*) FROM game_blackjack WHERE USER_ID='" + id + "' AND REFILL = CURDATE()";
	if ( atoi(database::fetch((char*)q.c_str(), &ok).c_str()) > 0 ) return false;
	chips = START_CHIPS;
	q = "UPDATE game_blackjack SET CHIPS=" + TO_STRING(START_CHIPS) + ", REFILL=CURDATE() WHERE USER_ID='" + id + "'";
	mysql_query(mysql, q.c_str());
	return true;
}

static void show_rank(void)
{
	print_header(J_CYAN "블랙잭 명예의 전당" J_WHITE);
	std::vector<std::map<std::string, std::string> > rows =
		database::fetch_rows((char*)"SELECT * FROM game_blackjack ORDER BY BEST DESC, DATE_TIME LIMIT 15");
	printf("\r\n  " J_GRAY "%4s  %-16s %10s %10s %10s" J_WHITE "\r\n", "순위", "이름", "최고 칩", "지금 칩", "이긴/판");
	if ( rows.size() == 0 ) printf("  " J_GRAY "    아직 기록이 없습니다." J_WHITE "\r\n");
	for ( unsigned int i = 0; i < rows.size(); i++ ) {
		bool me = rows[i]["USER_ID"] == user_id;
		char wp[24];
		snprintf(wp, sizeof(wp), "%s/%s", rows[i]["WINS"].c_str(), rows[i]["HANDS"].c_str());
		printf("  %s%4d  %-16s %10s %10s %10s" J_WHITE "\r\n", me ? J_YELLOW : J_WHITE, i + 1,
				string_truncate(display_text(rows[i]["NAME"]), 16, "").c_str(),
				rows[i]["BEST"].c_str(), rows[i]["CHIPS"].c_str(), wp);
	}
	wait_enter();
}

// ------------------------------------------------------------------
// 카드
// ------------------------------------------------------------------
static void shuffle_shoe(void)
{
	shoe.clear();
	for ( int d = 0; d < 4; d++ ) for ( int i = 0; i < 52; i++ ) shoe.push_back(i);	// 네 벌
	for ( int i = shoe.size() - 1; i > 0; i-- ) {
		int j = rand() % (i + 1);
		std::swap(shoe[i], shoe[j]);
	}
}

static int draw_card(void)
{
	if ( shoe.size() < 30 ) shuffle_shoe();
	int c = shoe.back();
	shoe.pop_back();
	return c;
}

static int card_value(int c)
{
	int r = c % 13;			// 0 A, 1~9 2~10, 10 J, 11 Q, 12 K
	if ( r == 0 ) return 11;
	if ( r >= 9 ) return 10;
	return r + 1;
}

static int hand_value(const std::vector<int> &h, bool *soft)
{
	int v = 0, aces = 0;
	for ( unsigned int i = 0; i < h.size(); i++ ) {
		v += card_value(h[i]);
		if ( h[i] % 13 == 0 ) aces++;
	}
	while ( v > 21 && aces > 0 ) { v -= 10; aces--; }
	if ( soft ) *soft = aces > 0;
	return v;
}

static bool is_blackjack(const std::vector<int> &h)
{
	return h.size() == 2 && hand_value(h, NULL) == 21;
}

// 카드 그림 (석 줄, 8 칸). hidden 이면 뒷면
static void draw_hand(int row, const std::vector<int> &h, bool hide_second)
{
	static const char *rank_s[] = { " A", " 2", " 3", " 4", " 5", " 6", " 7", " 8", " 9", "10", " J", " Q", " K" };
	static const char *suit_s[] = { "♠", "♥", "◆", "♣" };
	for ( int line = 0; line < 3; line++ ) {
		at(row + line, 4);
		printf("\033[K");
		for ( unsigned int i = 0; i < h.size(); i++ ) {
			bool back = hide_second && i == 1;
			int s = h[i] / 13 % 4;
			const char *col = back ? J_CYAN : ((s == 1 || s == 2) ? J_RED : J_WHITE);
			if ( line == 0 ) printf("%s┌──┐" J_WHITE " ", back ? J_CYAN : J_WHITE);
			else if ( line == 2 ) printf("%s└──┘" J_WHITE " ", back ? J_CYAN : J_WHITE);
			else if ( back ) printf(J_CYAN "│▒▒│" J_WHITE " ");
			else printf(J_WHITE "│%s%s%s" J_WHITE "│ ", col, rank_s[h[i] % 13], suit_s[s]);
		}
	}
}

#define DEALER_ROW	6
#define PLAYER_ROW	12
#define MSG_ROW		17
#define INFO_ROW	19
#define PROMPT_ROW	21

static void draw_table(const std::vector<int> &dealer, const std::vector<int> &player, bool hide, long bet)
{
	at(DEALER_ROW - 1, 3);
	printf("\033[K" J_CYAN "딜러" J_WHITE);
	if ( !hide ) printf("  (%d)", hand_value(dealer, NULL));
	draw_hand(DEALER_ROW, dealer, hide);

	bool soft;
	int v = hand_value(player, &soft);
	at(PLAYER_ROW - 1, 3);
	printf("\033[K" J_YELLOW "%s" J_WHITE "  (%s%d)", user_nick.c_str(), soft && v < 21 ? "부드러운 " : "", v);
	draw_hand(PLAYER_ROW, player, false);

	at(INFO_ROW, 3);
	printf("\033[K칩 " J_YELLOW "%ld" J_WHITE "   건 칩 " J_GREEN "%ld" J_WHITE, chips, bet);
}

static void message(const char *fmt, const char *color, long n)
{
	at(MSG_ROW, 3);
	printf("\033[K%s", color);
	printf(fmt, n);
	printf(J_WHITE);
}

// 한 판. 그만두면 (걸 칩에서 Q, 칩이 없음) false
static bool play_hand(void)
{
	char buf[32];
	if ( chips < MIN_BET ) {
		print_header(J_CYAN "블랙잭" J_WHITE);
		if ( refill() ) printf("\r\n  " J_YELLOW "칩을 다 잃었습니다. 오늘 한 번 " J_WHITE "%d" J_YELLOW " 칩을 다시 드립니다." J_WHITE "\r\n", START_CHIPS);
		else { printf("\r\n  " J_RED "칩을 다 잃었습니다. 내일 다시 받을 수 있습니다." J_WHITE "\r\n"); wait_enter(); return false; }
		wait_enter();
	}

	print_header(J_CYAN "블랙잭" J_WHITE);
	if ( last_bet > chips ) last_bet = chips;
	at(INFO_ROW, 3);
	printf("칩 " J_YELLOW "%ld" J_WHITE, chips);
	at(PROMPT_ROW, 3);
	printf(ESC_ENG);
	printf("걸 칩 (%d~%ld, Enter: %ld, 그만: Q) >> ", MIN_BET, chips, last_bet);
	line_input(buf, 8);
	std::string b = trim(buf);
	if ( !strcasecmp(b.c_str(), "q") || !strcasecmp(b.c_str(), "p") ) return false;
	long bet = b.empty() ? last_bet : atol(b.c_str());
	if ( !b.empty() && !is_number(b) ) return true;
	if ( bet < MIN_BET ) bet = MIN_BET;
	if ( bet > chips ) bet = chips;
	last_bet = bet;

	std::vector<int> dealer, player;
	player.push_back(draw_card()); dealer.push_back(draw_card());
	player.push_back(draw_card()); dealer.push_back(draw_card());

	bool hide = true, doubled = false;
	draw_table(dealer, player, hide, bet);

	// 블랙잭 확인
	bool pbj = is_blackjack(player), dbj = is_blackjack(dealer);
	if ( !pbj && !dbj ) {
		// 플레이어 차례
		while ( 1 ) {
			int v = hand_value(player, NULL);
			if ( v >= 21 ) break;
			at(PROMPT_ROW, 3);
			printf("\033[K" J_GRAY "H" J_WHITE " 히트(한 장 더)   " J_GRAY "S" J_WHITE " 스탠드(그만)");
			bool can_double = player.size() == 2 && chips >= bet * 2;
			if ( can_double ) printf("   " J_GRAY "D" J_WHITE " 더블다운(두 배, 한 장)");
			printf(" >> ");
			line_input(buf, 2);
			char k = tolower(buf[0]);
			if ( k == 'h' ) {
				player.push_back(draw_card());
				draw_table(dealer, player, hide, bet);
			} else if ( k == 's' ) {
				break;
			} else if ( k == 'd' && can_double ) {
				bet *= 2;
				doubled = true;
				player.push_back(draw_card());
				draw_table(dealer, player, hide, bet);
				break;
			}
		}
	}

	// 딜러 차례
	hide = false;
	int pv = hand_value(player, NULL);
	if ( pv <= 21 && !pbj ) {
		while ( hand_value(dealer, NULL) < 17 ) {
			draw_table(dealer, player, hide, bet);
			fflush(stdout);
			usleep(500000);
			dealer.push_back(draw_card());
		}
	}
	draw_table(dealer, player, hide, bet);
	int dv = hand_value(dealer, NULL);

	long win = 0;			// 칩 변화
	const char *msg;
	const char *color = J_WHITE;
	if ( pbj && dbj ) { msg = "둘 다 블랙잭! 비겼습니다."; }
	else if ( pbj ) { win = bet * 3 / 2; msg = "★ 블랙잭! %ld 칩을 땄습니다."; color = J_YELLOW; }
	else if ( dbj ) { win = -bet; msg = "딜러 블랙잭. %ld 칩을 잃었습니다."; color = J_RED; }
	else if ( pv > 21 ) { win = -bet; msg = "버스트! 21 을 넘었습니다. %ld 칩을 잃었습니다."; color = J_RED; }
	else if ( dv > 21 ) { win = bet; msg = "딜러 버스트! %ld 칩을 땄습니다."; color = J_YELLOW; }
	else if ( pv > dv ) { win = bet; msg = "이겼습니다! %ld 칩을 땄습니다."; color = J_YELLOW; }
	else if ( pv < dv ) { win = -bet; msg = "졌습니다. %ld 칩을 잃었습니다."; color = J_RED; }
	else { msg = "비겼습니다. 건 칩을 돌려받습니다."; }
	chips += win;
	message(msg, color, win < 0 ? -win : win);
	if ( doubled ) printf(J_GRAY "  (더블다운)" J_WHITE);
	save_hand(win > 0);
	at(INFO_ROW, 3);
	printf("\033[K칩 " J_YELLOW "%ld" J_WHITE, chips);
	at(PROMPT_ROW, 1);
	printf("\033[K");
	wait_enter();
	return true;
}

static void title(void)
{
	shuffle_shoe();
	while ( 1 ) {
		load_chips();
		print_header(J_CYAN "블랙잭" J_WHITE);
		printf("\r\n");
		printf("        " J_WHITE "┌──┐┌──┐" J_WHITE "   카드 합을 " J_YELLOW "21" J_WHITE " 에 가깝게! 넘으면 집니다.\r\n");
		printf("        " J_WHITE "│ A" J_GRAY "♠" J_WHITE "││ K" J_RED "♥" J_WHITE "│" "   A 는 1 또는 11, J Q K 는 10.\r\n");
		printf("        └──┘└──┘   처음 두 장이 21 이면 블랙잭 (1.5 배).\r\n\r\n");
		printf("        " J_GRAY "딜러는 16 이하면 한 장 더, 17 이상이면 멈춥니다." J_WHITE "\r\n\r\n");
		printf("        내 칩: " J_YELLOW "%ld" J_WHITE "\r\n\r\n", chips);
		printf("        1. 게임하기\r\n");
		printf("        2. 명예의 전당\r\n\r\n");

		char cmd[16];
		printf(ESC_ENG);
		printf("  선택 (끝내기: Q) >> ");
		line_input(cmd, 3);
		std::string c = trim(cmd);
		if ( !strcasecmp(c.c_str(), "q") || !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "x") ) return;
		int n = atoi(c.c_str());
		if ( n == 1 ) {
			// 걸 칩을 묻는 곳에서 Q 를 누를 때까지 이어서
			while ( play_hand() ) ;
		} else if ( n == 2 ) {
			show_rank();
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

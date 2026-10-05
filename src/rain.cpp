#include "main.h"
#include <sys/time.h>

// ------------------------------------------------------------------
// »ê¼ººñ : ÇÑ¸ÞÅ¸ÀÚ±³»çÀÇ ±× »ê¼ººñ °°Àº Å¸ÀÚ ¿¬½À °ÔÀÓ
//   bin/rain <È£½ºÆ®ÀÌ¸§> <¾ÆÀÌµð> <tty>
// À§¿¡¼­ ¶³¾îÁö´Â ´Ü¾î¸¦ ÃÄ¼­ ¾ø¾Ø´Ù. ¶¥¿¡ ´êÀ¸¸é »ý¸íÀÌ ÁÙ°í
// 10 ´Ü¾î¸¶´Ù ´Ü°è°¡ ¿Ã¶ó »¡¶óÁø´Ù.
//   ³ë¶õ ´Ü¾î: ¸ÂÈ÷¸é È­¸éÀÇ ´Ü¾î°¡ ¸ðµÎ »ç¶óÁü
//   ÃÊ·Ï ´Ü¾î: ¸ÂÈ÷¸é »ý¸í +1
// ------------------------------------------------------------------

struct termio sys_term;

char tty[10];

static std::string user_id;
static std::string user_nick;

#define R_WHITE		"\033[=15F"
#define R_YELLOW	"\033[=14F"
#define R_RED		"\033[=12F"
#define R_GREEN		"\033[=10F"
#define R_CYAN		"\033[=11F"
#define R_MAGENTA	"\033[=13F"
#define R_GRAY		"\033[=7F"
#define R_BROWN		"\033[=6F"
#define R_BLUE		"\033[=9F"

#define TOP_ROW		4		// ´Ü¾î°¡ ³ªÅ¸³ª´Â ÁÙ
#define GROUND_ROW	20		// ¶¥
#define STATUS_ROW	21
#define INPUT_ROW	22
#define MAX_LIFE	10
#define MAX_STAGE	12
#define INPUT_MAX	30

static const char *words_han[] = {
	"ÇÏ´Ã", "¹Ù´Ù", "±¸¸§", "³ª¹«", "»ç°ú", "ÇÐ±³", "Ä£±¸", "°¡¹æ", "¿¬ÇÊ", "°øÃ¥",
	"ÄÄÇ»ÅÍ", "¸ðµ©", "Åë½Å", "°Ô½ÃÆÇ", "ÀÚ·á½Ç", "´ëÈ­¹æ", "µ¿È£È¸", "ÀüÈ­¼±", "Å°º¸µå", "¸ð´ÏÅÍ",
	"µµ½º", "À©µµ¿ì", "µð½ºÄÏ", "ÇÏµåµð½ºÅ©", "ÇÁ¸°ÅÍ", "¸¶¿ì½º", "½ºÇÇÄ¿", "ÀÌ¾ß±â", "»õ·Ò", "ÇÏÀÌÅÚ",
	"Ãµ¸®¾È", "³ª¿ì´©¸®", "À¯´ÏÅÚ", "Á¢¼Ó", "Ã¤ÆÃ", "¹ø°³", "Á¤¸ð", "¾ÆÀÌµð", "ºñ¹Ð¹øÈ£", "¿î¿µÀÚ",
	"¹«±ÃÈ­", "Áø´Þ·¡", "°³³ª¸®", "¼Ò³ª¹«", "´ÜÇ³", "º½ºñ", "¿©¸§", "°¡À»", "°Ü¿ï", "´«»ç¶÷",
	"È£¶ûÀÌ", "°í¾çÀÌ", "°­¾ÆÁö", "Åä³¢", "°ÅºÏÀÌ", "´Ù¶÷Áã", "Âü»õ", "±îÄ¡", "Á¦ºñ", "µ¶¼ö¸®",
	"»ç¶û", "¿ìÁ¤", "Èñ¸Á", "Çàº¹", "Ãß¾ï", "¾à¼Ó", "±â´Ù¸²", "¼³·½", "±×¸®¿ò", "¿ôÀ½",
	"±èÄ¡", "µÈÀå", "ºñºö¹ä", "¶±ººÀÌ", "¶ó¸é", "¸¸µÎ", "³Ã¸é", "ºÒ°í±â", "ÀâÃ¤", "¼ÛÆí",
	"¼­¿ï", "ºÎ»ê", "´ë±¸", "ÀÎÃµ", "±¤ÁÖ", "´ëÀü", "¿ï»ê", "Á¦ÁÖ", "°­¸ª", "°æÁÖ",
	"ÇÑ±Û", "¼¼Á¾´ë¿Õ", "ÈÆ¹ÎÁ¤À½", "Å¸ÀÚ", "¿¬½À", "»ê¼ººñ", "¹«Áö°³", "¹ø°³ºÒ", "¼Ò³ª±â", "Àå¸¶",
	"ÀÚÀü°Å", "±âÂ÷", "¹ö½º", "ºñÇà±â", "ÁöÇÏÃ¶", "ÅÃ½Ã", "¹è³¶", "¿©Çà", "Áöµµ", "³ªÄ§¹Ý",
	"¼±»ý´Ô", "ÇÐ»ý", "¼÷Á¦", "½ÃÇè", "¹æÇÐ", "¿îµ¿È¸", "¼ÒÇ³", "µµ½Ã¶ô", "±Þ½Ä", "Ä¥ÆÇ",
	"Ãà±¸", "¾ß±¸", "³ó±¸", "¹è±¸", "Å¹±¸", "¼ö¿µ", "ÅÂ±Çµµ", "¾¾¸§", "´Þ¸®±â", "ÁÙ³Ñ±â",
	"ÇÇ¾Æ³ë", "±âÅ¸", "³ë·¡", "À½¾Ç", "±×¸²", "»çÁø", "¿µÈ­", "¸¸È­", "¼Ò¼³", "µ¿È­",
	"¾Æ¹öÁö", "¾î¸Ó´Ï", "ÇÒ¸Ó´Ï", "ÇÒ¾Æ¹öÁö", "´©³ª", "¾ð´Ï", "µ¿»ý", "°¡Á·", "ÀÌ¿ô", "¼Õ´Ô",
	"¿À´Ã", "³»ÀÏ", "¾îÁ¦", "¾ÆÄ§", "Àú³á", "»õº®", "ÇÑ³·", "¹ãÇÏ´Ã", "º°ºû", "´Þºû",
	"¿ìÃ¼±¹", "ÀºÇà", "º´¿ø", "¾à±¹", "½ÃÀå", "¹®¹æ±¸", "¿À¶ô½Ç", "¸¸È­¹æ", "ºÐ½ÄÁý", "»§Áý",
	"°¨ÀÚ", "°í±¸¸¶", "¿Á¼ö¼ö", "¼ö¹Ú", "Âü¿Ü", "Æ÷µµ", "µþ±â", "º¹¼þ¾Æ", "±Ö", "¹èÃß",
	"¹Ù¶÷°³ºñ", "Á¾ÀÌÇÐ", "µüÁö", "±¸½½", "ÆØÀÌ", "¿¬³¯¸®±â", "À·³îÀÌ", "Á¦±â", "°í¹«ÁÙ", "¼ú·¡Àâ±â",
	"ÇÁ·Î±×·¥", "¼ÒÇÁÆ®¿þ¾î", "ÇÏµå¿þ¾î", "ÀÎÅÍ³Ý", "ÀüÀÚ¿ìÆí", "È¨ÆäÀÌÁö", "´Ù¿î·Îµå", "¾÷·Îµå", "ÆÄÀÏ", "Æú´õ",
	NULL
};

static const char *words_eng[] = {
	"apple", "banana", "orange", "grape", "lemon", "melon", "peach", "cherry", "berry", "mango",
	"house", "school", "window", "door", "table", "chair", "paper", "pencil", "book", "desk",
	"modem", "dos", "disk", "floppy", "keyboard", "monitor", "mouse", "printer", "memory", "cpu",
	"basic", "pascal", "cobol", "fortran", "assembly", "compile", "debug", "linker", "editor", "shell",
	"login", "logout", "telnet", "board", "chat", "email", "upload", "download", "zmodem", "kermit",
	"rain", "cloud", "storm", "thunder", "snow", "wind", "sunny", "rainbow", "river", "ocean",
	"tiger", "rabbit", "turtle", "eagle", "horse", "monkey", "dragon", "snake", "sheep", "puppy",
	"happy", "smile", "friend", "dream", "hope", "love", "peace", "music", "dance", "story",
	"red", "blue", "green", "yellow", "white", "black", "purple", "silver", "golden", "pink",
	"one", "two", "three", "four", "five", "seven", "eight", "nine", "ten", "zero",
	"spring", "summer", "autumn", "winter", "morning", "evening", "night", "today", "tomorrow", "yesterday",
	"computer", "program", "software", "hardware", "network", "internet", "server", "client", "system", "screen",
	"quick", "brown", "fox", "jumps", "over", "lazy", "dog", "hello", "world", "typing",
	NULL
};

struct drop {
	std::string text;
	int row, col;
	int kind;		// 0 º¸Åë, 1 ³ë¶û(¸ðµÎ Áö¿ì±â), 2 ÃÊ·Ï(»ý¸í)
};

static std::vector<drop> drops;
static int life, stage, score, cleared, typed_chars;
static long start_ms;
static std::string input;
static bool han_mode;
static std::string ground;		// ¶¥ (»ê¼ººñ¿¡ ³ì¾Æ ±¸¸ÛÀÌ ³­´Ù)

// ------------------------------------------------------------------
// È­¸é / ÀÔ·Â
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
	printf(R_WHITE);
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
	printf("\033[=9F\033[=1G%s\033[=15F\033[=1G", repeat("¦¡", 40).c_str());
    printf("\033[1;1H");
	printf("\033[1A\033[7m%s\033[0m", host_name);
	int center = (80 - strlen(strip_ansi_codes(head_title))) / 2;
	if ( center < 0 ) center = 0;
    printf("\033[2;1H");
	printf("\r\033[%dC%s", center, head_title);
    printf("\033[3;1H");
	printf("\033[=0F\033[=1G%s\033[=15F\033[=1G", repeat("¦¬", 40).c_str());
    printf("\033[4;1H");
}

static long now_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000L + tv.tv_usec / 1000;
}

static std::string ask(const char *msg)
{
	char buf[64];
	printf(ESC_ENG);
	printf("%s", msg);
	line_input(buf, 20);
	return trim(buf);
}

static void wait_enter(void)
{
	printf("\r\n " R_GRAY "[Enter] ¸¦ ´©¸£¼¼¿ä." R_WHITE);
	press_enter();
}

// ³²¾Æ ÀÖ´Â ÀÔ·ÂÀ» ¹ö¸°´Ù (°ÔÀÓÀÌ ³¡³­ µÚ Ä¡´ø ±ÛÀÚ°¡ ´ÙÀ½ È­¸éÀ¸·Î ³Ñ¾î°¡Áö ¾Ê°Ô)
static void flush_input(void)
{
	fd_set fds;
	struct timeval tv;
	char buf[256];
	while ( 1 ) {
		FD_ZERO(&fds);
		FD_SET(0, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 200000;
		if ( select(1, &fds, NULL, NULL, &tv) <= 0 ) break;
		if ( read(0, buf, sizeof(buf)) <= 0 ) break;
	}
}

// ------------------------------------------------------------------
// DB
// ------------------------------------------------------------------
static void create_tables(void)
{
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS game_rain ( "
			"USER_ID VARCHAR(50) NOT NULL, "
			"MODE INT NOT NULL, "
			"NAME VARCHAR(50) NOT NULL, "
			"BEST INT NOT NULL, "
			"BEST_STAGE INT NOT NULL, "
			"PLAYS INT NOT NULL, "
			"DATE_TIME DATETIME NOT NULL, "
			"PRIMARY KEY (USER_ID, MODE) )");
}

// ±â·ÏÀ» ³²±â°í ÀÌ¹ø Á¡¼ö°¡ ¸î µîÀÎÁö µ¹·ÁÁØ´Ù
static int save_score(int mode)
{
	char q[1024];
	std::string id = database::escape(user_id.c_str());
	std::string name = database::escape(user_nick.c_str());
	snprintf(q, sizeof(q), "INSERT INTO game_rain (USER_ID, MODE, NAME, BEST, BEST_STAGE, PLAYS, DATE_TIME) "
			"VALUES ('%s', %d, '%s', %d, %d, 1, NOW()) ON DUPLICATE KEY UPDATE NAME='%s', PLAYS=PLAYS+1, "
			"BEST_STAGE=IF(%d > BEST, %d, BEST_STAGE), DATE_TIME=IF(%d > BEST, NOW(), DATE_TIME), BEST=GREATEST(BEST, %d)",
			id.c_str(), mode, name.c_str(), score, stage, name.c_str(), score, stage, score, score);
	mysql_query(mysql, q);

	bool ok;
	snprintf(q, sizeof(q), "SELECT COUNT(*) FROM game_rain WHERE MODE=%d AND BEST > %d", mode, score);
	return atoi(database::fetch(q, &ok).c_str()) + 1;
}

static void show_rank(void)
{
	print_header(R_CYAN "»ê¼ººñ ¸í¿¹ÀÇ Àü´ç" R_WHITE);
	for ( int mode = 0; mode < 2; mode++ ) {
		char q[256];
		snprintf(q, sizeof(q), "SELECT * FROM game_rain WHERE MODE=%d ORDER BY BEST DESC, DATE_TIME LIMIT 8", mode);
		std::vector<std::map<std::string, std::string> > rows = database::fetch_rows(q);
		printf("\r\n  " R_YELLOW "%s" R_WHITE "\r\n", mode == 0 ? "¡ß ÇÑ±Û" : "¡ß ¿µ¾î");
		printf("  " R_GRAY "%4s  %-16s %8s %6s %6s  %s" R_WHITE "\r\n", "¼øÀ§", "ÀÌ¸§", "Á¡¼ö", "´Ü°è", "ÆÇ¼ö", "³¯Â¥");
		if ( rows.size() == 0 ) printf("  " R_GRAY "    ¾ÆÁ÷ ±â·ÏÀÌ ¾ø½À´Ï´Ù." R_WHITE "\r\n");
		for ( unsigned int i = 0; i < rows.size(); i++ ) {
			bool me = rows[i]["USER_ID"] == user_id;
			std::string d = rows[i]["DATE_TIME"].size() >= 10 ? rows[i]["DATE_TIME"].substr(0, 10) : "";
			printf("  %s%4d  %-16s %8s %6s %6s  %s" R_WHITE "\r\n", me ? R_YELLOW : R_WHITE, i + 1,
					string_truncate(display_text(rows[i]["NAME"]), 16, "").c_str(),
					rows[i]["BEST"].c_str(), rows[i]["BEST_STAGE"].c_str(), rows[i]["PLAYS"].c_str(), d.c_str());
		}
	}
	wait_enter();
}

// ------------------------------------------------------------------
// °ÔÀÓ
// ------------------------------------------------------------------
// ´Ü°è¸¶´Ù ÇÑ Ä­ ¶³¾îÁö´Â ½Ã°£(ms)°ú »õ ´Ü¾î°¡ ³ª¿À´Â °£°Ý(Ä­)
static int fall_ms(void)
{
	int ms = 900 - (stage - 1) * 65;
	return ms < 180 ? 180 : ms;
}

static int spawn_gap(void)
{
	if ( stage <= 2 ) return 4;
	if ( stage <= 5 ) return 3;
	if ( stage <= 9 ) return 2;
	return 1;
}

static void draw_drop(const drop &d)
{
	at(d.row, d.col);
	const char *c = d.kind == 1 ? R_YELLOW : (d.kind == 2 ? R_GREEN : R_WHITE);
	printf("%s%s" R_WHITE, c, d.text.c_str());
}

static void erase_drop(const drop &d)
{
	at(d.row, d.col);
	printf("%s", std::string(d.text.size(), ' ').c_str());
}

static void draw_ground(void)
{
	at(GROUND_ROW, 1);
	printf(R_BROWN "%s" R_WHITE, ground.c_str());
}

static void draw_status(void)
{
	long sec = (now_ms() - start_ms) / 1000;
	int speed = sec > 0 ? (int)(typed_chars * 60 / sec) : 0;
	std::string hearts;
	for ( int i = 0; i < MAX_LIFE; i++ ) hearts += i < life ? "¢¾" : "¢½";
	at(STATUS_ROW, 1);
	printf("\033[K  Á¡¼ö " R_YELLOW "%-6d" R_WHITE " ´Ü°è " R_CYAN "%-2d" R_WHITE " ¸ÂÈû %-4d ºÐ´ç %-4d" "  " R_RED "%s" R_WHITE,
			score, stage, cleared, speed, hearts.c_str());
}

static void draw_input(void)
{
	at(INPUT_ROW, 1);
	printf("\033[K  " R_CYAN "ÀÔ·Â >>" R_WHITE " %s", input.c_str());
	fflush(stdout);
}

// ÀÔ·Â ÁÙ ¾Æ·¡¿¡ Àá±ñ ¾Ë¸² (2 ÃÊ µÚ Áö¿ò)
static long flash_until = 0;

static void flash(const char *msg)
{
	at(INPUT_ROW + 1, 45);
	printf("[K%s", msg);
	flash_until = now_ms() + 2000;
}

static const char *pick_word(void)
{
	const char **list = han_mode ? words_han : words_eng;
	int n = 0;
	while ( list[n] ) n++;
	// È­¸é¿¡ ÀÌ¹Ì ÀÖ´Â ´Ü¾î´Â ÇÇÇÑ´Ù
	for ( int tries = 0; tries < 10; tries++ ) {
		const char *w = list[rand() % n];
		bool dup = false;
		for ( unsigned int i = 0; i < drops.size(); i++ ) {
			if ( drops[i].text == w ) dup = true;
		}
		if ( !dup ) return w;
	}
	return list[rand() % n];
}

static void spawn(void)
{
	drop d;
	d.text = pick_word();
	d.row = TOP_ROW;
	// ¿À¸¥ÂÊ ³¡(80 Ä­)¿¡ ´êÀ¸¸é ÅÍ¹Ì³ÎÀÌ ÁÙÀ» ³Ñ±â¹Ç·Î 78 Ä­ ¾È¿¡¼­
	int maxcol = 78 - (int)d.text.size();
	d.col = 3 + rand() % (maxcol - 2);
	int r = rand() % 100;
	d.kind = r < 5 ? 1 : (r < 9 ? 2 : 0);
	drops.push_back(d);
	draw_drop(d);
}

// ÇÑ Ä­¾¿ ¶³¾î¶ß¸°´Ù. ¶¥¿¡ ´êÀ¸¸é »ý¸í -1
static void fall(void)
{
	for ( unsigned int i = 0; i < drops.size(); ) {
		erase_drop(drops[i]);
		drops[i].row++;
		if ( drops[i].row >= GROUND_ROW ) {
			// ¶¥ÀÌ ³ì´Â´Ù
			// ¢Æ ´Â µÎ ¹ÙÀÌÆ®(µÎ Ä­)¶ó Â¦¼ö Ä­ ´ÜÀ§·Î
			unsigned int from = (drops[i].col - 1) & ~1u;
			unsigned int to = drops[i].col - 1 + drops[i].text.size();
			for ( unsigned int k = from; k < to && k + 1 < ground.size(); k += 2 ) {
				ground[k] = ' ';
				ground[k + 1] = ' ';
			}
			draw_ground();
			life--;
			drops.erase(drops.begin() + i);
			continue;
		}
		draw_drop(drops[i]);
		i++;
	}
}

// Enter: Ä£ ´Ü¾î¿Í °°Àº ´Ü¾î Áß °¡Àå ¾Æ·¡ °ÍÀ» ¾ø¾Ø´Ù
static void submit(void)
{
	std::string w = trim(input);
	input.clear();
	draw_input();
	if ( w.empty() ) return;

	int best = -1;
	for ( unsigned int i = 0; i < drops.size(); i++ ) {
		if ( drops[i].text == w && (best < 0 || drops[i].row > drops[best].row) ) best = i;
	}
	if ( best < 0 ) {
		flash(R_GRAY "¶¯!" R_WHITE);
		return;
	}

	drop d = drops[best];
	erase_drop(d);
	drops.erase(drops.begin() + best);
	cleared++;
	typed_chars += han_mode ? (int)d.text.size() / 2 : (int)d.text.size();
	// ³·°Ô ³»·Á¿Â ´Ü¾îÀÏ¼ö·Ï Á¡¼ö°¡ Á¶±Ý Àû´Ù
	score += (int)d.text.size() * 5 * stage + (GROUND_ROW - d.row);

	if ( d.kind == 1 ) {
		for ( unsigned int i = 0; i < drops.size(); i++ ) {
			erase_drop(drops[i]);
			score += (int)drops[i].text.size() * 5 * stage;
			cleared++;
		}
		drops.clear();
		flash(R_YELLOW "¸ðµÎ »ç¶óÁ®¶ó!" R_WHITE);
	} else if ( d.kind == 2 ) {
		if ( life < MAX_LIFE ) life++;
		flash(R_GREEN "»ý¸í +1" R_WHITE);
	}

	// 10 ´Ü¾î¸¶´Ù ´Ü°è°¡ ¿À¸¥´Ù
	int new_stage = cleared / 10 + 1;
	if ( new_stage > MAX_STAGE ) new_stage = MAX_STAGE;
	if ( new_stage > stage ) {
		stage = new_stage;
		flash(R_MAGENTA "´Ü°è°¡ ¿Ã¶ú½À´Ï´Ù!" R_WHITE);
	}
}

// ±ÛÀÚ ÇÏ³ª¸¦ ÀÔ·Â ÁÙ¿¡ (ÇÑ±ÛÀº µÎ ¹ÙÀÌÆ®°¡ ´Ù ¿À¸é)
static void key(unsigned char c)
{
	static int han_lead = -1;
	static int esc_state = 0;

	// È­»ìÇ¥ °°Àº ESC ¹®ÀÚ¿­Àº ¹ö¸°´Ù
	if ( esc_state == 1 ) { esc_state = (c == '[') ? 2 : 0; return; }
	if ( esc_state == 2 ) { if ( (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '~' ) esc_state = 0; return; }
	if ( c == 0x1b ) { esc_state = 1; return; }

	if ( han_lead >= 0 ) {
		if ( c >= 0xA1 && input.size() + 2 <= INPUT_MAX ) {
			input += (char)han_lead;
			input += (char)c;
		}
		han_lead = -1;
		draw_input();
		return;
	}
	if ( c >= 0xA1 ) {
		han_lead = c;
		return;
	}
	if ( c == '\r' ) {
		submit();
	} else if ( c == '\b' || c == 0x7f ) {
		if ( !input.empty() ) {
			// ¸¶Áö¸· ±ÛÀÚ°¡ ÇÑ±ÛÀÌ¸é µÎ ¹ÙÀÌÆ®
			unsigned int k = 0, last = 1;
			while ( k < input.size() ) {
				last = ((unsigned char)input[k] >= 0xA1 && k + 1 < input.size()) ? 2 : 1;
				k += last;
			}
			input.erase(input.size() - last);
		}
		draw_input();
	} else if ( c >= 0x20 && c < 0x7f && input.size() < INPUT_MAX ) {
		input += (char)c;
		draw_input();
	}
}

static void play(bool han)
{
	han_mode = han;
	drops.clear();
	life = MAX_LIFE;
	stage = 1;
	score = cleared = typed_chars = 0;
	input.clear();
	flash_until = 0;
	ground = repeat("¢Æ", 39);

	print_header(R_CYAN "»ê ¼º ºñ" R_WHITE "  -  " R_GRAY "±×¸¸ÇÏ·Á¸é Q ¸¦ Ä¡°í Enter" R_WHITE);
	draw_ground();
	at(INPUT_ROW + 1, 1);
	printf(R_GRAY "  %s" R_WHITE, han ? "ÇÑ±Û ÀÔ·Â »óÅÂ·Î ¹Ù²ã¼­ Ä¡¼¼¿ä." : "¿µ¹® ÀÔ·Â »óÅÂ·Î Ä¡¼¼¿ä.");
	printf(han ? ESC_HAN : ESC_ENG);
	start_ms = now_ms();
	draw_status();
	draw_input();

	long next_fall = now_ms() + fall_ms();
	int ticks = spawn_gap();		// Ã³À½¿£ ¹Ù·Î ÇÏ³ª ³ª¿Àµµ·Ï

	while ( life > 0 ) {
		long wait = next_fall - now_ms();
		if ( wait < 0 ) wait = 0;
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(0, &fds);
		struct timeval tv;
		tv.tv_sec = wait / 1000;
		tv.tv_usec = (wait % 1000) * 1000;
		int r = select(1, &fds, NULL, NULL, &tv);
		if ( r > 0 ) {
			unsigned char buf[64];
			int n = read(0, buf, sizeof(buf));
			if ( n <= 0 ) host_close();
			for ( int i = 0; i < n; i++ ) {
				if ( buf[i] == '\r' && !strcasecmp(trim(input).c_str(), "q") ) {
					life = 0;
					break;
				}
				key(buf[i]);
			}
		}
		if ( life > 0 && now_ms() >= next_fall ) {
			fall();
			if ( ++ticks >= spawn_gap() ) {
				spawn();
				ticks = 0;
			}
			next_fall = now_ms() + fall_ms();
			if ( flash_until && now_ms() > flash_until ) {
				at(INPUT_ROW + 1, 45);
				printf("[K");
				flash_until = 0;
			}
			draw_status();
			draw_input();
		}
	}

	draw_status();
	printf(ESC_ENG);
	fflush(stdout);
	flush_input();

	int rank = save_score(han ? 0 : 1);
	print_header(R_CYAN "»ê ¼º ºñ" R_WHITE " - °á°ú");
	printf("\r\n\r\n");
	printf("        " R_RED "  .   '  .    '   .   ,  '  .   '" R_WHITE "\r\n");
	printf("        " R_GRAY "    '  .   ,   '   .    '   .  " R_WHITE "\r\n");
	printf("        " R_BROWN "¢Æ¢Æ¢Æ  ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ  ¢Æ¢Æ¢Æ¢Æ   ¢Æ¢Æ¢Æ¢Æ¢Æ  ¢Æ¢Æ" R_WHITE "\r\n\r\n");
	long sec = (now_ms() - start_ms) / 1000;
	printf("        Á¡¼ö        " R_YELLOW "%d" R_WHITE "\r\n", score);
	printf("        ´Ü°è        " R_CYAN "%d" R_WHITE "\r\n", stage);
	printf("        ¸ÂÈù ´Ü¾î   %d\r\n", cleared);
	printf("        ºÐ´ç ±ÛÀÚ   %d\r\n", sec > 0 ? (int)(typed_chars * 60 / sec) : 0);
	printf("\r\n        %s ¼øÀ§ " R_MAGENTA "%d µî" R_WHITE "\r\n", han ? "ÇÑ±Û" : "¿µ¾î", rank);
	wait_enter();
}

static void title(void)
{
	while ( 1 ) {
		print_header(R_CYAN "»ê ¼º ºñ" R_WHITE);
		printf("\r\n");
		printf(R_GRAY "          '    .     '     ,    .    '     .    '    ,     '\r\n");
		printf("       .    " R_WHITE "ÄÄÇ»ÅÍ" R_GRAY "    '     .   " R_YELLOW "ÇÏÀÌÅÚ" R_GRAY "    ,    '    .     " R_WHITE "¸ðµ©" R_GRAY "\r\n");
		printf("          ,     '     .    " R_GREEN "¹«Áö°³" R_GRAY "    '    .     ,    '    .\r\n");
		printf("       '     .   " R_WHITE "Åë½Å" R_GRAY "    ,    '     .     '    " R_WHITE "»ê¼ººñ" R_GRAY "   ,     '\r\n");
		printf("          .    '     ,    .     '    ,     .     '     .     ,\r\n");
		printf(R_BROWN "       ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ¢Æ" R_WHITE "\r\n\r\n");
		printf("    ÇÏ´Ã¿¡¼­ ¶³¾îÁö´Â ´Ü¾î¸¦ ÃÄ¼­ ¾ø¾Ö¼¼¿ä. ¶¥¿¡ ´êÀ¸¸é »ý¸íÀÌ ÁÙ¾îµì´Ï´Ù.\r\n");
		printf("    10 ´Ü¾î¸¶´Ù ´Ü°è°¡ ¿Ã¶ó »¡¶óÁý´Ï´Ù.\r\n");
		printf("    " R_YELLOW "³ë¶õ ´Ü¾î" R_WHITE ": È­¸éÀÇ ´Ü¾î°¡ ¸ðµÎ »ç¶óÁü   " R_GREEN "ÃÊ·Ï ´Ü¾î" R_WHITE ": »ý¸í +1\r\n\r\n");
		printf("    " R_YELLOW "[1]" R_WHITE " ÇÑ±Û »ê¼ººñ   " R_YELLOW "[2]" R_WHITE " ¿µ¾î »ê¼ººñ   "
				R_YELLOW "[R]" R_WHITE " ¸í¿¹ÀÇ Àü´ç   " R_YELLOW "[Q]" R_WHITE " ³ª°¡±â\r\n");

		std::string c = ask("\r\n ¼±ÅÃ >> ");
		if ( c == "1" ) play(true);
		else if ( c == "2" ) play(false);
		else if ( !strcasecmp(c.c_str(), "r") ) show_rank();
		else if ( c.empty() || !strcasecmp(c.c_str(), "q") || !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "x") ) return;
	}
}

int main(int argc, char **argv)
{
	if ( argc < 3 ) {
		printf("usage: %s <host_name> <user_id> [tty]\n", argv[0]);
		return 1;
	}
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

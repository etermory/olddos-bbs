#include "main.h"

// ------------------------------------------------------------------
// 용사의 전설 : 옛 BBS 도어 게임 방식의 1인용 RPG
//   bin/hero <호스트이름> <아이디> <tty>
// 하루에 숲 사냥 횟수가 정해져 있고, 사부님을 이겨 레벨을 올려
// 12 레벨에 붉은 용을 쓰러뜨리면 영웅이 된다.
// ------------------------------------------------------------------

struct termio sys_term;

char tty[10];

static std::string user_id;

// 색 (이야기 터미널 글자색)
#define C_WHITE		"\033[=15F"
#define C_YELLOW	"\033[=14F"
#define C_RED		"\033[=12F"
#define C_GREEN		"\033[=10F"
#define C_CYAN		"\033[=11F"
#define C_MAGENTA	"\033[=13F"
#define C_GRAY		"\033[=7F"
#define C_BROWN		"\033[=6F"

#define MAX_LEVEL		12
#define FIGHTS_PER_DAY	15

// ------------------------------------------------------------------
// 아스키 그림
// ------------------------------------------------------------------
static const char *art_title[] = {
	"   _   _  _____  ____    ___  ",
	"  | | | || ____||  _ \\  / _ \\ ",
	"  | |_| ||  _|  | |_) || | | |",
	"  |  _  || |___ |  _ < | |_| |",
	"  |_| |_||_____||_| \\_\\ \\___/ ",
	NULL
};

static const char *art_town[] = {
	"        |>>>                        |>>>",
	"        |          ~      ~         |",
	"    _  _|_  _                   _  _|_  _",
	"   |;|_|;|_|;|     ________     |;|_|;|_|;|",
	"   \\\\.    .  /    /        \\    \\\\.    .  /",
	"    \\\\:  .  /    /  ______  \\    \\\\:  .  /",
	"     ||:   |    |  |      |  |    ||:   |",
	"     ||:.  |    |  |  []  |  |    ||:.  |",
	"   __||:  .|____|__|______|__|____||:  .|__",
	NULL
};

static const char *art_forest[] = {
	"      ^       ^^      ^       ^^       ^",
	"     /|\\     /||\\    /|\\     /||\\     /|\\",
	"    /_|_\\   /_||_\\  /_|_\\   /_||_\\   /_|_\\",
	"   /__|__\\ /__||__\\/__|__\\ /__||__\\ /__|__\\",
	"      |       ||      |       ||       |",
	"  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
	NULL
};

static const char *art_beast[] = {
	"        /\\_____/\\",
	"       /  o   o  \\",
	"      ( ==  ^  == )",
	"       )         (",
	"      (  )     (  )",
	"     ( (  )   (  ) )",
	"    (__(__)___(__)__)",
	NULL
};

static const char *art_human[] = {
	"         _______",
	"        /  ___  \\",
	"       |  (o o)  |",
	"       |   \\_/   |",
	"        \\_______/",
	"      ___|  |  |___",
	"     /   |  |  |   \\  --|==>",
	"    /____|__|__|____\\",
	NULL
};

static const char *art_ghost[] = {
	"        .-\"\"\"\"-.",
	"       /  _  _  \\",
	"      |  (o)(o)  |",
	"      |    __    |",
	"      |   (__)   |",
	"       \\        /",
	"        \\/\\/\\/\\/  ~~",
	NULL
};

static const char *art_undead[] = {
	"           .-.",
	"          (o.o)",
	"           |=|",
	"          __|__",
	"        //.=|=.\\\\",
	"       // .=|=. \\\\",
	"       \\\\ .=|=. //",
	"           | |",
	"          _| |_",
	NULL
};

static const char *art_goblin[] = {
	"       \\\\  //",
	"      .-'^^'-.",
	"     /  O  O  \\",
	"    |   (__)   |",
	"     \\  \\__/  /",
	"      '-.__.-'",
	"       /|  |\\    ===[]",
	NULL
};

static const char *art_serpent[] = {
	"            ____",
	"           / o  \\___",
	"          |   ____  >~",
	"     _____/  /    \\_/",
	"    /  _____/",
	"   /  /    ___",
	"  (  (____/   \\____",
	"   \\______________/",
	NULL
};

static const char *art_dragon[] = {
	"                 __    __",
	"          /\\    /  \\__/  \\    /\\",
	"         /  \\  |  O    O  |  /  \\",
	"        / /\\ \\  \\   /\\   /  / /\\ \\",
	"       /_/  \\_\\  \\  \\/  /  /_/  \\_\\",
	"                /  \\__/  \\",
	"         ~~~~  / /|    |\\ \\  ~~~~",
	"        ~~~~  /_/ |____| \\_\\  ~~~~",
	NULL
};

static const char *art_master[] = {
	"           ___",
	"          (o o)",
	"         __\\_/__",
	"        /  | |  \\",
	"       /|  | |  |\\",
	"      (_|  | |  |_)",
	"         /_/ \\_\\",
	NULL
};

static const char *art_shop[] = {
	"           />",
	"   ()     //-------------------(",
	"  (*)OXOX|[=================-   >",
	"   ()     \\\\-------------------(",
	"           \\>",
	NULL
};

static const char *art_healer[] = {
	"         _____",
	"        |_____|",
	"         |   |",
	"        /     \\",
	"       |   +   |",
	"       |  +++  |",
	"        \\_____/",
	NULL
};

static const char *art_grave[] = {
	"           _____",
	"          /     \\",
	"         |  R.I.P |",
	"         |        |",
	"         |        |",
	"      ___|________|___",
	NULL
};

// ------------------------------------------------------------------
// 게임 자료
// ------------------------------------------------------------------
struct item {
	const char *name;
	int power;
	int price;
};

static const item weapons[] = {
	{ "맨주먹", 0, 0 },
	{ "몽둥이", 5, 200 },
	{ "단검", 10, 1000 },
	{ "철검", 20, 3000 },
	{ "도끼", 35, 10000 },
	{ "장창", 50, 30000 },
	{ "월도", 75, 100000 },
	{ "청강검", 100, 300000 },
	{ "현철검", 150, 1000000 },
	{ "칠성검", 225, 3000000 },
	{ "벽력검", 300, 10000000 },
	{ "천룡검", 400, 30000000 },
};

static const item armors[] = {
	{ "평상복", 0, 0 },
	{ "가죽옷", 4, 200 },
	{ "누비옷", 8, 1000 },
	{ "사슬갑옷", 16, 3000 },
	{ "비늘갑옷", 28, 10000 },
	{ "판금갑옷", 40, 30000 },
	{ "흑철갑", 60, 100000 },
	{ "은린갑", 80, 300000 },
	{ "금강갑", 120, 1000000 },
	{ "용린갑", 180, 3000000 },
	{ "천잠보의", 240, 10000000 },
	{ "신선갑", 320, 30000000 },
};

static const int item_count = sizeof(weapons) / sizeof(weapons[0]);

// 다음 레벨에 필요한 경험치 (레벨 1~11)
static const int exp_need[] = { 0, 100, 300, 700, 1500, 3000, 5500, 9000, 14000, 21000, 30000, 42000 };

struct monster_kind {
	const char *name;
	const char *art;	// b: 짐승, h: 사람, g: 귀신, u: 해골, m: 도깨비, s: 뱀
	const char *attack;	// 공격 모습
};

// 레벨마다 3 종 (약함, 보통, 강함)
static const monster_kind monsters[11][3] = {
	{ { "들쥐 떼", "b", "이빨로 물어뜯습니다" }, { "굶주린 들개", "b", "달려들어 뭅니다" }, { "초보 산적", "h", "몽둥이를 휘두릅니다" } },
	{ { "독사", "s", "독니를 들이댑니다" }, { "멧돼지", "b", "엄니로 들이받습니다" }, { "산적 졸개", "h", "칼을 휘두릅니다" } },
	{ { "늑대", "b", "목덜미를 노립니다" }, { "도깨비", "m", "방망이를 내리칩니다" }, { "외눈 산적", "h", "도끼를 내리찍습니다" } },
	{ { "구렁이", "s", "몸으로 휘감습니다" }, { "해골 병사", "u", "녹슨 칼을 휘두릅니다" }, { "산적 두목", "h", "쌍도를 휘두릅니다" } },
	{ { "처녀 귀신", "g", "서늘한 손을 뻗습니다" }, { "강시", "u", "두 팔을 뻗어 찌릅니다" }, { "반달곰", "b", "앞발로 후려칩니다" } },
	{ { "구미호", "b", "여우불을 던집니다" }, { "독각귀", "m", "뿔로 들이받습니다" }, { "흑호", "b", "발톱으로 할큅니다" } },
	{ { "석상 괴물", "m", "돌주먹을 내리칩니다" }, { "야차", "m", "삼지창으로 찌릅니다" }, { "이무기 새끼", "s", "꼬리로 후려칩니다" } },
	{ { "불여우", "b", "불을 토합니다" }, { "거대 거미", "b", "독침을 쏩니다" }, { "망령 무사", "g", "검기를 날립니다" } },
	{ { "화염 도깨비", "m", "불방망이를 휘두릅니다" }, { "흑기사", "h", "대검을 내려칩니다" }, { "해골 장수", "u", "창을 꿰찌릅니다" } },
	{ { "설인", "b", "얼음 덩이를 던집니다" }, { "마도사", "h", "번개 주문을 겁니다" }, { "이무기", "s", "물기둥을 뿜습니다" } },
	{ { "천년 구미호", "b", "아홉 꼬리로 휘감습니다" }, { "흑룡 새끼", "s", "검은 불을 뿜습니다" }, { "마두", "h", "마공을 펼칩니다" } },
};

static const char *masters[] = {
	"", "떠돌이 검객 장삼", "산사의 노승 혜각", "궁수 연화", "철권 마석", "검성 백운",
	"쌍검 소진", "창술가 위무", "도사 청허", "검귀 한월", "무림맹주 운룡", "검선 태허",
};

// ------------------------------------------------------------------
// 용사 정보
// ------------------------------------------------------------------
struct hero {
	std::string name;
	int level, exp, hp, max_hp, str, def;
	int gold, bank, weapon, armor;
	int fights, dead, master_done;
	int kills, wins, deaths;
	std::string play_date;
};

static hero h;

// ------------------------------------------------------------------
// 화면 / 입력
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

void print_art(const char **art, const char *color)
{
	printf("%s", color);
	for (int i=0; art[i] != NULL; i++) {
		printf("      %s\r\n", art[i]);
	}
	printf(C_WHITE);
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
	printf("\r\n " C_GRAY "[Enter] 를 누르세요." C_WHITE);
	press_enter();
}

int rnd(int a, int b)
{
	if ( b <= a ) return a;
	return a + rand() % (b - a + 1);
}

std::string money(long v)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "%ld", v);
	std::string s = buf, r;
	int n = s.size();
	for (int i=0; i<n; i++) {
		r += s[i];
		if ( (n - i - 1) % 3 == 0 && i != n - 1 && s[i] != '-' ) r += ',';
	}
	return r;
}

std::string today_string(void)
{
	char buf[32];
	time_t t = time(NULL);
	strftime(buf, sizeof(buf), "%Y-%m-%d", localtime(&t));
	return buf;
}

// ------------------------------------------------------------------
// DB
// ------------------------------------------------------------------
void create_tables(void)
{
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS game_hero ( "
			"USER_ID VARCHAR(50) NOT NULL PRIMARY KEY, "
			"NAME VARCHAR(50) NOT NULL, "
			"LEVEL INT NOT NULL, EXP INT NOT NULL, "
			"HP INT NOT NULL, MAX_HP INT NOT NULL, STR INT NOT NULL, DEF INT NOT NULL, "
			"GOLD INT NOT NULL, BANK INT NOT NULL, WEAPON INT NOT NULL, ARMOR INT NOT NULL, "
			"FIGHTS INT NOT NULL, DEAD INT NOT NULL, MASTER_DONE INT NOT NULL, "
			"KILLS INT NOT NULL, WINS INT NOT NULL, DEATHS INT NOT NULL, "
			"PLAY_DATE DATE NOT NULL )");
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS game_news ( "
			"NO INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, "
			"DATE_TIME DATETIME NOT NULL, "
			"TEXT VARCHAR(255) NOT NULL )");
}

void add_news(const std::string &text)
{
	std::string q = "INSERT INTO game_news (DATE_TIME, TEXT) VALUES (NOW(), '" + database::escape(text.c_str()) + "')";
	mysql_query(mysql, q.c_str());
}

void new_hero(void)
{
	h.level = 1; h.exp = 0;
	h.max_hp = 20; h.hp = 20;
	h.str = 10; h.def = 2;
	h.gold = 50; h.bank = 0;
	h.weapon = 0; h.armor = 0;
	h.fights = FIGHTS_PER_DAY; h.dead = 0; h.master_done = 0;
	h.play_date = today_string();
}

void save_hero(void)
{
	char q[2048];
	snprintf(q, sizeof(q), "REPLACE INTO game_hero (USER_ID, NAME, LEVEL, EXP, HP, MAX_HP, STR, DEF, "
			"GOLD, BANK, WEAPON, ARMOR, FIGHTS, DEAD, MASTER_DONE, KILLS, WINS, DEATHS, PLAY_DATE) "
			"VALUES ('%s', '%s', %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, '%s')",
			database::escape(user_id.c_str()).c_str(), database::escape(h.name.c_str()).c_str(),
			h.level, h.exp, h.hp, h.max_hp, h.str, h.def, h.gold, h.bank, h.weapon, h.armor,
			h.fights, h.dead, h.master_done, h.kills, h.wins, h.deaths,
			database::escape(h.play_date.c_str()).c_str());
	mysql_query(mysql, q);
}

// 처음이면 false
bool load_hero(void)
{
	std::string q = "SELECT * FROM game_hero WHERE USER_ID='" + database::escape(user_id.c_str()) + "'";
	std::vector<std::map<std::string, std::string> > rows = database::fetch_rows((char*)q.c_str());
	if ( rows.size() == 0 ) return false;

	std::map<std::string, std::string> &r = rows[0];
	h.name = r["NAME"];
	h.level = atoi(r["LEVEL"].c_str());
	h.exp = atoi(r["EXP"].c_str());
	h.hp = atoi(r["HP"].c_str());
	h.max_hp = atoi(r["MAX_HP"].c_str());
	h.str = atoi(r["STR"].c_str());
	h.def = atoi(r["DEF"].c_str());
	h.gold = atoi(r["GOLD"].c_str());
	h.bank = atoi(r["BANK"].c_str());
	h.weapon = atoi(r["WEAPON"].c_str());
	h.armor = atoi(r["ARMOR"].c_str());
	h.fights = atoi(r["FIGHTS"].c_str());
	h.dead = atoi(r["DEAD"].c_str());
	h.master_done = atoi(r["MASTER_DONE"].c_str());
	h.kills = atoi(r["KILLS"].c_str());
	h.wins = atoi(r["WINS"].c_str());
	h.deaths = atoi(r["DEATHS"].c_str());
	h.play_date = r["PLAY_DATE"];

	if ( h.level < 1 ) h.level = 1;
	if ( h.level > MAX_LEVEL ) h.level = MAX_LEVEL;
	if ( h.weapon < 0 || h.weapon >= item_count ) h.weapon = 0;
	if ( h.armor < 0 || h.armor >= item_count ) h.armor = 0;
	return true;
}

// 날이 바뀌면 사냥 횟수와 체력을 채우고 죽었으면 되살린다
void check_new_day(void)
{
	std::string today = today_string();
	if ( h.play_date == today ) return;

	h.play_date = today;
	h.fights = FIGHTS_PER_DAY;
	h.master_done = 0;
	h.hp = h.max_hp;
	if ( h.dead ) {
		h.dead = 0;
	}
	save_hero();
}

int attack_power(void)
{
	return h.str + weapons[h.weapon].power;
}

int defense_power(void)
{
	return h.def + armors[h.armor].power;
}

// ------------------------------------------------------------------
// 레벨에 맞는 기준 능력치 (몬스터 세기 계산용)
// ------------------------------------------------------------------
static int ref_max_hp(int level)
{
	int hp = 20;
	for (int i=1; i<level; i++) hp += 10 + i * 5;
	return hp;
}

static int ref_attack(int level)
{
	int s = 10;
	for (int i=1; i<level; i++) s += 3 + i;
	int w = (level - 1 < item_count) ? weapons[level - 1].power : weapons[item_count - 1].power;
	return s + w;
}

static int ref_defense(int level)
{
	int d = 2;
	for (int i=1; i<level; i++) d += 2 + i / 2;
	int a = (level - 1 < item_count) ? armors[level - 1].power : armors[item_count - 1].power;
	return d + a;
}

struct foe {
	std::string name;
	std::string attack_msg;
	const char **art;
	const char *color;
	int hp, max_hp, atk, def;
	int exp, gold;
};

const char **art_of(const char *kind)
{
	switch ( kind[0] ) {
		case 'b': return art_beast;
		case 'h': return art_human;
		case 'g': return art_ghost;
		case 'u': return art_undead;
		case 'm': return art_goblin;
		case 's': return art_serpent;
	}
	return art_beast;
}

// 레벨 level 의 몬스터 (strength: 0 약함, 1 보통, 2 강함)
foe make_monster(int level, int strength)
{
	if ( level > 11 ) level = 11;
	static const double mul[] = { 0.85, 1.0, 1.2 };
	double m = mul[strength] * rnd(90, 110) / 100.0;

	const monster_kind &k = monsters[level - 1][strength];
	foe f;
	f.name = k.name;
	f.attack_msg = k.attack;
	f.art = art_of(k.art);
	f.color = (strength == 2) ? C_RED : (strength == 1 ? C_YELLOW : C_GREEN);

	int ratk = ref_attack(level);
	f.def = (int)(ratk * 0.3 * m);
	// 한 번 싸우면 체력의 20% 쯤 줄도록 (장비가 처지면 35% 쯤)
	f.max_hp = (int)(ratk * 1.4 * m) + 5;
	f.hp = f.max_hp;
	f.atk = (int)((ref_max_hp(level) / 8.0 + ref_defense(level) / 2.0) / 0.75 * m);

	int need = exp_need[(level < 11) ? level : 11];
	// 레벨마다 12 번쯤 싸우면 사부님께 도전할 수 있도록
	f.exp = (int)(need / 12.0 * m) + 1;
	int price = weapons[(level < item_count) ? level : item_count - 1].price;
	f.gold = (int)(price / 12.0 * m) + level * 10;
	return f;
}

// ------------------------------------------------------------------
// 전투
// ------------------------------------------------------------------
enum fight_result { WIN, LOSE, RUN };

void print_bar(const char *label, int v, int max, const char *color)
{
	int width = 20;
	int fill = (max > 0) ? v * width / max : 0;
	if ( fill < 0 ) fill = 0;
	if ( v > 0 && fill == 0 ) fill = 1;
	printf(" %-16s %s", string_truncate(label, 16, "").c_str(), color);
	for (int i=0; i<width; i++) printf("%s", i < fill ? "#" : ".");
	printf(C_WHITE " %d/%d", v < 0 ? 0 : v, max);
}

// 받는 피해 (공격력 - 방어력/2, 공격력의 50~100%)
int damage(int atk, int def)
{
	int d = rnd(atk / 2, atk) - def / 2;
	return d < 0 ? 0 : d;
}

// can_run: 도망칠 수 있는지 (사부님 결투는 도망 대신 포기)
fight_result fight(foe &f, const char *title, bool can_run)
{
	std::vector<std::string> log;
	log.push_back(std::string(C_CYAN) + f.name + C_WHITE + " 이(가) 나타났습니다!");

	while (1) {
		print_header(title);
		print_art(f.art, f.color);
		printf("\r\n");
		print_bar(f.name.c_str(), f.hp, f.max_hp, C_RED);
		printf("\r\n");
		print_bar(h.name.c_str(), h.hp, h.max_hp, C_GREEN);
		printf("\r\n\r\n");

		// 최근 전투 기록 4 줄
		unsigned int start = log.size() > 4 ? log.size() - 4 : 0;
		for (unsigned int i=start; i<log.size(); i++) {
			printf(" %s\r\n", log[i].c_str());
		}
		for (unsigned int i=log.size() - start; i<4; i++) printf("\r\n");

		if ( f.hp <= 0 || h.hp <= 0 ) break;

		std::string cmd = ask(can_run ? "\r\n [A]공격  [R]도망  >> " : "\r\n [A]공격  [R]포기  >> ");

		if ( !strcasecmp(cmd.c_str(), "r") ) {
			if ( !can_run ) return RUN;
			if ( rnd(1, 100) <= 55 ) {
				printf("\r\n " C_YELLOW "무사히 도망쳤습니다." C_WHITE);
				wait_enter();
				return RUN;
			}
			log.push_back("도망치려 했지만 붙잡혔습니다!");
		} else if ( !strcasecmp(cmd.c_str(), "a") || cmd.empty() ) {
			int d = damage(attack_power(), f.def);
			// 가끔 회심의 일격
			bool crit = rnd(1, 100) <= 8;
			if ( crit ) d = d * 2 + 1;
			f.hp -= d;
			char buf[256];
			if ( d == 0 ) snprintf(buf, sizeof(buf), "공격이 빗나갔습니다.");
			else if ( crit ) snprintf(buf, sizeof(buf), C_YELLOW "회심의 일격!" C_WHITE " %s 에게 %d 의 피해를 입혔습니다.", f.name.c_str(), d);
			else snprintf(buf, sizeof(buf), "%s 에게 %d 의 피해를 입혔습니다.", f.name.c_str(), d);
			log.push_back(buf);
			if ( f.hp <= 0 ) continue;
		} else {
			continue;
		}

		// 적의 반격
		int d = damage(f.atk, defense_power());
		h.hp -= d;
		char buf[256];
		if ( d == 0 ) snprintf(buf, sizeof(buf), "%s 의 공격을 피했습니다.", f.name.c_str());
		else snprintf(buf, sizeof(buf), C_RED "%s 이(가) %s." C_WHITE " %d 의 피해!", f.name.c_str(), f.attack_msg.c_str(), d);
		log.push_back(buf);
	}

	return (f.hp <= 0) ? WIN : LOSE;
}

void hero_dies(const std::string &killer)
{
	h.hp = 0;
	h.dead = 1;
	h.deaths++;
	int lost = h.gold;
	h.gold = 0;
	h.exp -= h.exp / 10;
	save_hero();

	print_header("쓰러졌습니다");
	print_art(art_grave, C_GRAY);
	printf("\r\n " C_RED "%s 에게 쓰러졌습니다..." C_WHITE "\r\n", killer.c_str());
	printf("\r\n 지니고 있던 돈 %s 냥을 잃고, 경험치가 조금 줄었습니다.", money(lost).c_str());
	printf("\r\n 오늘은 더 싸울 수 없습니다. 내일 다시 깨어납니다.\r\n");

	add_news(h.name + " 이(가) " + killer + " 에게 쓰러졌습니다.");
	wait_enter();
}

// ------------------------------------------------------------------
// 숲
// ------------------------------------------------------------------
void forest_event(void)
{
	print_header("숲 속");
	print_art(art_forest, C_GREEN);
	printf("\r\n");

	int r = rnd(1, 4);
	if ( r == 1 ) {
		int g = h.level * rnd(20, 60);
		h.gold += g;
		printf(" 덤불 속에서 낡은 주머니를 발견했습니다. " C_YELLOW "%s 냥" C_WHITE "을 얻었습니다!", money(g).c_str());
	} else if ( r == 2 ) {
		h.hp = h.max_hp;
		printf(" 맑은 옹달샘을 찾아 물을 마셨습니다. " C_GREEN "체력이 모두 회복되었습니다!" C_WHITE);
	} else if ( r == 3 ) {
		int e = exp_need[h.level < 11 ? h.level : 11] / 20 + 1;
		h.exp += e;
		printf(" 길 잃은 노인을 마을까지 모셔다 드렸습니다. 경험치 " C_CYAN "%d" C_WHITE " 를 얻었습니다.", e);
	} else {
		h.fights++;
		printf(" 산들바람이 붑니다. 기운이 나서 " C_YELLOW "사냥 횟수가 1 늘었습니다." C_WHITE);
	}
	printf("\r\n");
	save_hero();
	wait_enter();
}

void fight_dragon(void);
void go_healer(void);

void go_forest(void)
{
	while (1) {
		check_new_day();
		print_header("숲 속");
		print_art(art_forest, C_GREEN);
		printf("\r\n 남은 사냥 횟수 " C_YELLOW "%d" C_WHITE "   체력 %d/%d   소지금 %s 냥\r\n",
				h.fights, h.hp, h.max_hp, money(h.gold).c_str());

		if ( h.dead ) {
			printf("\r\n " C_RED "오늘은 쓰러져서 더 이상 사냥할 수 없습니다." C_WHITE "\r\n");
			wait_enter();
			return;
		}

		printf("\r\n  " C_YELLOW "[L]" C_WHITE " 몬스터를 찾아 나선다");
		printf("\r\n  " C_YELLOW "[H]" C_WHITE " 약방에 들른다");
		if ( h.level >= MAX_LEVEL ) {
			printf("\r\n  " C_RED "[D] 붉은 용의 둥지로 간다" C_WHITE);
		}
		printf("\r\n  " C_YELLOW "[Q]" C_WHITE " 마을로 돌아간다\r\n");

		std::string cmd = ask("\r\n 선택 >> ");
		if ( !strcasecmp(cmd.c_str(), "q") || !strcasecmp(cmd.c_str(), "p") ) return;

		if ( !strcasecmp(cmd.c_str(), "h") ) {
			go_healer();
			continue;
		}

		if ( !strcasecmp(cmd.c_str(), "d") && h.level >= MAX_LEVEL ) {
			fight_dragon();
			if ( h.dead ) return;
			continue;
		}

		if ( strcasecmp(cmd.c_str(), "l") && !cmd.empty() ) continue;

		if ( h.fights <= 0 ) {
			printf("\r\n " C_GRAY "오늘은 너무 지쳤습니다. 내일 다시 오세요." C_WHITE);
			wait_enter();
			continue;
		}
		h.fights--;

		// 열 번에 한 번은 다른 일이 생긴다
		if ( rnd(1, 10) == 1 ) {
			forest_event();
			continue;
		}

		int strength = rnd(1, 100) <= 50 ? 0 : (rnd(1, 100) <= 70 ? 1 : 2);
		foe f = make_monster(h.level, strength);
		fight_result r = fight(f, "숲 속의 결투", true);

		if ( r == WIN ) {
			h.exp += f.exp;
			h.gold += f.gold;
			h.kills++;
			save_hero();
			printf("\r\n " C_YELLOW "%s 을(를) 물리쳤습니다!" C_WHITE, f.name.c_str());
			printf("\r\n 경험치 " C_CYAN "%s" C_WHITE ", 돈 " C_YELLOW "%s 냥" C_WHITE " 을 얻었습니다.",
					money(f.exp).c_str(), money(f.gold).c_str());
			if ( h.level < MAX_LEVEL && h.exp >= exp_need[h.level] ) {
				printf("\r\n " C_MAGENTA "사부님께 도전할 만큼 강해졌습니다!" C_WHITE);
			}
			wait_enter();
		} else if ( r == LOSE ) {
			hero_dies(f.name);
			return;
		} else {
			save_hero();
		}
	}
}

// ------------------------------------------------------------------
// 사부님 (레벨 올리기)
// ------------------------------------------------------------------
void go_master(void)
{
	print_header("사부님의 수련장");
	print_art(art_master, C_CYAN);

	if ( h.level >= MAX_LEVEL ) {
		printf("\r\n 사부님: \"더 가르칠 것이 없다. 숲 깊은 곳의 붉은 용을 물리쳐라.\"\r\n");
		wait_enter();
		return;
	}

	printf("\r\n 사부님 " C_CYAN "%s" C_WHITE " 께서 기다리고 계십니다.\r\n", masters[h.level]);
	printf(" 경험치 %s / %s\r\n", money(h.exp).c_str(), money(exp_need[h.level]).c_str());

	if ( h.exp < exp_need[h.level] ) {
		printf("\r\n 사부님: \"아직 수련이 부족하다. 경험을 더 쌓고 오너라.\"\r\n");
		wait_enter();
		return;
	}
	if ( h.master_done ) {
		printf("\r\n 사부님: \"오늘은 이미 겨루었다. 내일 다시 오너라.\"\r\n");
		wait_enter();
		return;
	}
	if ( h.dead ) {
		printf("\r\n 사부님: \"몸부터 추스르고 오너라.\"\r\n");
		wait_enter();
		return;
	}

	std::string cmd = ask("\r\n 사부님께 도전하시겠습니까? (Y/n) >> ");
	if ( !strcasecmp(cmd.c_str(), "n") ) return;

	h.master_done = 1;
	foe f = make_monster(h.level, 2);
	f.name = masters[h.level];
	f.attack_msg = "가르침의 일격을 날립니다";
	f.art = art_master;
	f.color = C_CYAN;
	f.max_hp = (int)(f.max_hp * 1.3);
	f.hp = f.max_hp;
	f.atk = (int)(f.atk * 1.1);

	fight_result r = fight(f, "사부님과의 결투", false);
	// 수련 결투에서는 죽지 않는다
	if ( h.hp <= 0 ) h.hp = 1;

	if ( r == WIN ) {
		h.level++;
		int add_hp = 10 + (h.level - 1) * 5;
		int add_str = 3 + (h.level - 1);
		int add_def = 2 + (h.level - 1) / 2;
		h.max_hp += add_hp;
		h.str += add_str;
		h.def += add_def;
		h.hp = h.max_hp;
		save_hero();

		printf("\r\n " C_MAGENTA "사부님: \"훌륭하구나! 이제 너는 %d 레벨이다.\"" C_WHITE, h.level);
		printf("\r\n 최대 체력 +%d, 힘 +%d, 방어 +%d", add_hp, add_str, add_def);
		char buf[256];
		snprintf(buf, sizeof(buf), "%s 이(가) 사부님 %s 을(를) 이기고 %d 레벨이 되었습니다!",
				h.name.c_str(), masters[h.level - 1], h.level);
		add_news(buf);
	} else {
		save_hero();
		printf("\r\n 사부님: \"아직 멀었구나. 내일 다시 오너라.\"");
	}
	wait_enter();
}

// ------------------------------------------------------------------
// 붉은 용
// ------------------------------------------------------------------
void fight_dragon(void)
{
	print_header("붉은 용의 둥지");
	print_art(art_dragon, C_RED);
	printf("\r\n 뜨거운 바람이 몰아칩니다. 동굴 깊은 곳에서 붉은 용이 눈을 뜹니다...\r\n");
	std::string cmd = ask("\r\n 정말 싸우시겠습니까? (y/N) >> ");
	if ( strcasecmp(cmd.c_str(), "y") ) return;

	foe f = make_monster(11, 2);
	f.name = "붉은 용";
	f.attack_msg = "지옥불을 토해냅니다";
	f.art = art_dragon;
	f.color = C_RED;
	// 최고 장비의 12 레벨 용사가 열에 여섯 번쯤 이기는 세기
	f.max_hp = (int)(f.max_hp * 2.5);
	f.hp = f.max_hp;
	f.atk = (int)(f.atk * 1.2);
	f.def = (int)(f.def * 1.1);

	fight_result r = fight(f, "붉은 용과의 결투", true);
	if ( r == LOSE ) {
		hero_dies("붉은 용");
		return;
	}
	if ( r == RUN ) {
		save_hero();
		return;
	}

	// 영웅이 되어 처음부터 다시
	h.wins++;
	int wins = h.wins, kills = h.kills, deaths = h.deaths;
	std::string name = h.name;
	new_hero();
	h.name = name;
	h.wins = wins; h.kills = kills; h.deaths = deaths;
	h.fights = 0;
	save_hero();

	print_header("영웅 탄생");
	print_art(art_title, C_YELLOW);
	printf("\r\n " C_YELLOW "붉은 용을 쓰러뜨렸습니다! 마을 사람들이 환호합니다!" C_WHITE "\r\n");
	printf("\r\n 이제 %d 번째 영웅의 칭호를 얻었습니다.", h.wins);
	printf("\r\n 새로운 모험을 위해 1 레벨부터 다시 시작합니다. (영웅 칭호는 남습니다)\r\n");

	char buf[256];
	snprintf(buf, sizeof(buf), "*** %s 이(가) 붉은 용을 쓰러뜨리고 영웅이 되었습니다! (%d 번째) ***",
			h.name.c_str(), h.wins);
	add_news(buf);
	wait_enter();
}

// ------------------------------------------------------------------
// 가게
// ------------------------------------------------------------------
void go_shop(bool weapon)
{
	const item *list = weapon ? weapons : armors;
	int &have = weapon ? h.weapon : h.armor;

	while (1) {
		print_header(weapon ? "대장간" : "갑옷 가게");
		print_art(art_shop, C_GRAY);
		printf("\r\n 지금 %s : " C_CYAN "%s" C_WHITE "  (+%d)    소지금 " C_YELLOW "%s 냥" C_WHITE "\r\n",
				weapon ? "무기" : "갑옷", list[have].name, list[have].power, money(h.gold).c_str());
		printf(" %s\r\n", repeat("─", 36).c_str());

		// 두 단으로
		int rows = (item_count - 1 + 1) / 2;
		for (int r=0; r<rows; r++) {
			for (int c=0; c<2; c++) {
				int i = 1 + c * rows + r;
				if ( i >= item_count ) break;
				printf("  %s%2d. %-10s +%-4d %10s냥" C_WHITE "   ",
						i == have ? C_CYAN : (list[i].price <= h.gold ? C_WHITE : C_GRAY),
						i, list[i].name, list[i].power, money(list[i].price).c_str());
			}
			printf("\r\n");
		}

		printf("\r\n " C_GRAY "가지고 있던 것은 값의 절반을 쳐줍니다." C_WHITE);
		std::string cmd = ask("\r\n 살 물건 번호 (Q:나가기) >> ");
		if ( !strcasecmp(cmd.c_str(), "q") || !strcasecmp(cmd.c_str(), "p") || cmd.empty() ) return;

		int n = atoi(cmd.c_str());
		if ( n < 1 || n >= item_count ) continue;
		if ( n == have ) {
			printf("\r\n 이미 가지고 있습니다.");
			wait_enter();
			continue;
		}

		int refund = list[have].price / 2;
		if ( h.gold + refund < list[n].price ) {
			printf("\r\n " C_RED "돈이 모자랍니다." C_WHITE);
			wait_enter();
			continue;
		}

		h.gold = h.gold + refund - list[n].price;
		have = n;
		save_hero();
		printf("\r\n " C_YELLOW "%s" C_WHITE " 을(를) 샀습니다!", list[n].name);
		wait_enter();
	}
}

void go_healer(void)
{
	print_header("약방");
	print_art(art_healer, C_GREEN);

	int lost = h.max_hp - h.hp;
	int cost = lost * h.level * 2;
	printf("\r\n 체력 %d/%d    소지금 %s 냥\r\n", h.hp, h.max_hp, money(h.gold).c_str());

	if ( h.dead ) {
		printf("\r\n 의원: \"쓰러진 몸은 하룻밤 푹 쉬어야 하네.\"\r\n");
		wait_enter();
		return;
	}
	if ( lost <= 0 ) {
		printf("\r\n 의원: \"아주 건강하구먼.\"\r\n");
		wait_enter();
		return;
	}

	printf("\r\n 의원: \"모두 고치려면 %s 냥일세.\"\r\n", money(cost).c_str());
	std::string cmd = ask("\r\n [A]모두 치료  [숫자]그만큼 치료  [Q]나가기 >> ");
	if ( !strcasecmp(cmd.c_str(), "q") || cmd.empty() ) return;

	int heal = lost;
	if ( strcasecmp(cmd.c_str(), "a") ) heal = atoi(cmd.c_str());
	if ( heal <= 0 ) return;
	if ( heal > lost ) heal = lost;

	int price = heal * h.level * 2;
	if ( price > h.gold ) {
		heal = h.gold / (h.level * 2);
		price = heal * h.level * 2;
	}
	if ( heal <= 0 ) {
		printf("\r\n " C_RED "돈이 모자랍니다." C_WHITE);
		wait_enter();
		return;
	}

	h.gold -= price;
	h.hp += heal;
	save_hero();
	printf("\r\n 체력이 %d 회복되었습니다. (%s 냥)", heal, money(price).c_str());
	wait_enter();
}

void go_bank(void)
{
	while (1) {
		print_header("전장 (돈 맡기는 곳)");
		printf("\r\n 숲에서 쓰러지면 지니고 있던 돈을 모두 잃습니다. 전장에 맡긴 돈은 안전합니다.\r\n");
		printf("\r\n 소지금 " C_YELLOW "%s 냥" C_WHITE "    맡긴 돈 " C_CYAN "%s 냥" C_WHITE "\r\n",
				money(h.gold).c_str(), money(h.bank).c_str());

		std::string cmd = ask("\r\n [D]맡기기  [W]찾기  [Q]나가기 >> ");
		if ( !strcasecmp(cmd.c_str(), "q") || !strcasecmp(cmd.c_str(), "p") || cmd.empty() ) return;

		bool deposit = !strcasecmp(cmd.c_str(), "d");
		bool withdraw = !strcasecmp(cmd.c_str(), "w");
		if ( !deposit && !withdraw ) continue;

		std::string a = ask("\r\n 얼마나? (숫자, A:모두) >> ");
		int amount = !strcasecmp(a.c_str(), "a") ? (deposit ? h.gold : h.bank) : atoi(a.c_str());
		if ( amount <= 0 ) continue;

		if ( deposit ) {
			if ( amount > h.gold ) amount = h.gold;
			h.gold -= amount;
			h.bank += amount;
		} else {
			if ( amount > h.bank ) amount = h.bank;
			h.bank -= amount;
			h.gold += amount;
		}
		save_hero();
	}
}

// ------------------------------------------------------------------
// 순위 / 소식 / 상태
// ------------------------------------------------------------------
void show_rank(void)
{
	print_header("용사 순위");
	printf("\r\n  %4s  %-16s %6s %6s %10s %8s\r\n", "순위", "이름", "영웅", "레벨", "경험치", "처치");
	printf("  %s\r\n", repeat("─", 30).c_str());

	std::vector<std::map<std::string, std::string> > rows = database::fetch_rows(
			(char*)"SELECT * FROM game_hero ORDER BY WINS DESC, LEVEL DESC, EXP DESC LIMIT 15");
	for (unsigned int i=0; i<rows.size(); i++) {
		std::map<std::string, std::string> &r = rows[i];
		bool me = (r["USER_ID"] == user_id);
		printf("  %s%4d  %-16s %6s %6s %10s %8s" C_WHITE "\r\n", me ? C_YELLOW : C_WHITE, i + 1,
				string_truncate(r["NAME"], 16, "").c_str(), r["WINS"].c_str(), r["LEVEL"].c_str(),
				money(atol(r["EXP"].c_str())).c_str(), r["KILLS"].c_str());
	}
	wait_enter();
}

void show_news(void)
{
	print_header("마을 소식");
	printf("\r\n");
	std::vector<std::map<std::string, std::string> > rows = database::fetch_rows(
			(char*)"SELECT * FROM game_news ORDER BY NO DESC LIMIT 15");
	if ( rows.size() == 0 ) {
		printf("  아직 아무 소식도 없습니다.\r\n");
	}
	for (unsigned int i=0; i<rows.size(); i++) {
		std::string t = rows[i]["DATE_TIME"];
		printf("  " C_GRAY "%s" C_WHITE " %s\r\n", t.size() >= 16 ? t.substr(5, 11).c_str() : t.c_str(),
				string_truncate(rows[i]["TEXT"], 64, "").c_str());
	}
	wait_enter();
}

void show_status(void)
{
	printf(" " C_CYAN "%s" C_WHITE "  레벨 %d  체력 %d/%d  공격 %d  방어 %d  경험치 %s",
			h.name.c_str(), h.level, h.hp, h.max_hp, attack_power(), defense_power(), money(h.exp).c_str());
	if ( h.level < MAX_LEVEL ) printf("/%s", money(exp_need[h.level]).c_str());
	printf("\r\n 무기 %s  갑옷 %s  소지금 " C_YELLOW "%s 냥" C_WHITE "  남은 사냥 %d  영웅 %d 번\r\n",
			weapons[h.weapon].name, armors[h.armor].name, money(h.gold).c_str(), h.fights, h.wins);
}

// ------------------------------------------------------------------
// 마을
// ------------------------------------------------------------------
void town(void)
{
	while (1) {
		check_new_day();
		print_header("용사의 전설 - 하늘마을");
		print_art(art_town, C_BROWN);
		printf("\r\n");
		show_status();
		printf(" %s\r\n", repeat("─", 39).c_str());
		printf("  " C_YELLOW "[F]" C_WHITE " 숲으로 사냥      " C_YELLOW "[M]" C_WHITE " 사부님께 도전    " C_YELLOW "[H]" C_WHITE " 약방\r\n");
		printf("  " C_YELLOW "[W]" C_WHITE " 대장간(무기)     " C_YELLOW "[A]" C_WHITE " 갑옷 가게        " C_YELLOW "[B]" C_WHITE " 전장(돈 맡기기)\r\n");
		printf("  " C_YELLOW "[R]" C_WHITE " 용사 순위        " C_YELLOW "[N]" C_WHITE " 마을 소식        " C_YELLOW "[Q]" C_WHITE " 게임 끝내기\r\n");

		std::string cmd = ask(" 선택 >> ");
		const char *c = cmd.c_str();

		if ( !strcasecmp(c, "q") || !strcasecmp(c, "p") || !strcasecmp(c, "x") ) break;
		else if ( !strcasecmp(c, "f") ) go_forest();
		else if ( !strcasecmp(c, "m") ) go_master();
		else if ( !strcasecmp(c, "h") ) go_healer();
		else if ( !strcasecmp(c, "w") ) go_shop(true);
		else if ( !strcasecmp(c, "a") ) go_shop(false);
		else if ( !strcasecmp(c, "b") ) go_bank();
		else if ( !strcasecmp(c, "r") ) show_rank();
		else if ( !strcasecmp(c, "n") ) show_news();
	}
}

void title_screen(bool first)
{
	print_header("용사의 전설");
	printf("\r\n");
	print_art(art_title, C_YELLOW);
	printf("\r\n      " C_RED "~ 붉은 용을 쓰러뜨릴 용사를 찾습니다 ~" C_WHITE "\r\n\r\n");

	if ( first ) {
		printf("  하늘마을 북쪽 숲에는 오래전부터 붉은 용이 살고 있습니다.\r\n");
		printf("  숲에서 몬스터를 물리쳐 경험을 쌓고, 사부님을 이겨 실력을 올리세요.\r\n");
		printf("  12 레벨이 되면 용의 둥지에 들어갈 수 있습니다.\r\n\r\n");
		printf("  하루에 숲 사냥은 %d 번. 쓰러지면 지닌 돈을 잃고 다음 날 깨어납니다.\r\n", FIGHTS_PER_DAY);
		printf("  돈은 전장에 맡겨 두세요!\r\n");
		printf("\r\n  새 용사 " C_CYAN "%s" C_WHITE " 의 모험이 시작됩니다.\r\n", h.name.c_str());
	} else {
		printf("  다시 오셨군요, " C_CYAN "%s" C_WHITE " 님.\r\n", h.name.c_str());
	}
	wait_enter();
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

    ioctl(0,TCGETA, &sys_term);
	raw_mode();
    umask(0111);

	srand(time(NULL) ^ getpid());

	if ( database::open() == false )
		exit(1);

	create_tables();

	bool first = !load_hero();
	if ( first ) {
		bool exist;
		std::map<std::string, std::string> user = database::user_info((char*)user_id.c_str(), &exist);
		if ( !exist ) {
			printf("\r\n회원 정보가 없습니다.");
			wait_enter();
			host_close();
		}
		new_hero();
		h.name = user["NICK_NAME"];
		h.kills = h.wins = h.deaths = 0;
		save_hero();
		add_news(h.name + " 이(가) 하늘마을에 찾아왔습니다.");
	}

	check_new_day();
	title_screen(first);
	town();

	host_close();
	return 0;
}

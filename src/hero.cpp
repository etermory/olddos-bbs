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
// ansi.h 의 표준 ANSI 색 대신 이야기 터미널 글자색 (ESC[=nF) 을 쓴다
#undef C_WHITE
#define C_WHITE		"\033[=15F"
#undef C_YELLOW
#define C_YELLOW	"\033[=14F"
#undef C_RED
#define C_RED		"\033[=12F"
#undef C_GREEN
#define C_GREEN		"\033[=10F"
#undef C_CYAN
#define C_CYAN		"\033[=11F"
#undef C_MAGENTA
#define C_MAGENTA	"\033[=13F"
#undef C_GRAY
#define C_GRAY		"\033[=7F"
#undef C_BROWN
#define C_BROWN		"\033[=6F"
#undef C_BLUE
#define C_BLUE		"\033[=9F"

#define MAX_LEVEL		12
#define FIGHTS_PER_DAY	15

// ------------------------------------------------------------------
// 아스키 그림
// 그림 안의 {r} 같은 표시는 글자색을 바꾼다.
//   {r}빨강 {y}노랑 {g}초록 {c}하늘 {m}보라 {w}흰색 {b}갈색 {k}회색 {B}파랑
//   {X} 는 그림의 기본색으로 되돌림
// ------------------------------------------------------------------
static const char *art_title[] = {
	"{r}   _   _  _____  ____    ___  ",
	"{r}  | | | || ____||  _ \\  / _ \\ ",
	"{y}  | |_| ||  _|  | |_) || | | |",
	"{y}  |  _  || |___ |  _ < | |_| |",
	"{g}  |_| |_||_____||_| \\_\\ \\___/ ",
	NULL
};

static const char *art_town[] = {
	"{y}        |>>>                        |>>>",
	"{b}        |{c}          ~      ~         {b}|",
	"{r}    _  _|_  _                   _  _|_  _",
	"{b}   |;|_|;|_|;|     {r}________{b}     |;|_|;|_|;|",
	"{b}   \\\\.    .  /    {r}/        \\{b}    \\\\.    .  /",
	"{b}    \\\\:  .  /    {r}/  ______  \\{b}    \\\\:  .  /",
	"{b}     ||:   |    {w}|  |      |  |{b}    ||:   |",
	"{b}     ||:.  |    {w}|  |  {y}[]{w}  |  |{b}    ||:.  |",
	"{g}  ~~{b}_||:  .|____{w}|__|______|__|{b}____||:  .|_{g}~~",
	NULL
};

static const char *art_forest[] = {
	"{g}      ^       ^^      ^       ^^       ^",
	"{g}     /|\\     /||\\    /|\\     /||\\     /|\\",
	"{g}    /_|_\\   /_||_\\  /_|_\\   /_||_\\   /_|_\\",
	"{g}   /__|__\\ /__||__\\/__|__\\ /__||__\\ /__|__\\",
	"{b}      |       ||      |       ||       |",
	"{b}  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
	NULL
};

static const char *art_beast[] = {
	"        /\\_____/\\",
	"       /  {y}o   o{X}  \\",
	"      ( ==  {r}^{X}  == )",
	"       )         (",
	"      (  )     (  )",
	"     ( (  )   (  ) )",
	"    (__(__)___(__)__)",
	NULL
};

static const char *art_human[] = {
	"         _______",
	"        /  ___  \\",
	"       |  ({y}o o{X})  |",
	"       |   \\_/   |",
	"        \\_______/",
	"      ___|  |  |___",
	"     /   |  |  |   \\  {w}--|==>{X}",
	"    /____|__|__|____\\",
	NULL
};

static const char *art_ghost[] = {
	"        .-\"\"\"\"-.",
	"       /  _  _  \\",
	"      |  ({r}o{X})({r}o{X})  |",
	"      |    __    |",
	"      |   (__)   |",
	"       \\        /",
	"        \\/\\/\\/\\/  {k}~~{X}",
	NULL
};

static const char *art_undead[] = {
	"           .-.",
	"          ({r}o{X}.{r}o{X})",
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
	"     /  {y}O  O{X}  \\",
	"    |   (__)   |",
	"     \\  {r}\\__/{X}  /",
	"      '-.__.-'",
	"       /|  |\\    {b}===[]{X}",
	NULL
};

static const char *art_serpent[] = {
	"            ____",
	"           / {y}o{X}  \\___",
	"          |   ____  {r}>~{X}",
	"     _____/  /    \\_/",
	"    /  _____/",
	"   /  /    ___",
	"  (  (____/   \\____",
	"   \\______________/",
	NULL
};

static const char *art_dragon[] = {
	"                     /|      /|",
	"   /\\    /\\         / |______/ |",
	"  /  \\/\\/  \\       /   \\\\  //   \\___",
	" / /\\    /\\ \\     |     {y}@{X}            \\___",
	"/_/  \\/\\/  \\_\\    |      __{w}VVVVVVVVVVV{X}/",
	"      \\     \\____/      /  {y}~~~~~~~~~~~~~~{X}",
	"       \\  /\\/\\/\\/\\/\\    \\__{w}^^^^^^^^^^^{X}\\",
	"        \\/           \\_________________/",
	"        /_/\\_\\   /_/\\_\\      \\/\\/\\/\\",
	"                                  \\/\\/\\>",
	NULL
};

static const char *art_master[] = {
	"           ___",
	"          ({w}o o{X})",
	"         __\\_/__",
	"        /  | |  \\",
	"       /|  | |  |\\",
	"      (_|  | |  |_)   {w}|{X}",
	"         /_/ \\_\\     {w}|{X}",
	NULL
};

static const char *art_shop[] = {
	"           {y}/>",
	"   {r}(){X}     {y}//{w}-------------------(",
	"  {r}(*){b}OXOX{y}|[{w}=================-   >",
	"   {r}(){X}     {y}\\\\{w}-------------------(",
	"           {y}\\>",
	NULL
};

static const char *art_healer[] = {
	"         _____",
	"        |_____|",
	"         |   |",
	"        /     \\",
	"       |   {r}+{X}   |",
	"       |  {r}+++{X}  |",
	"        \\_____/",
	NULL
};

static const char *art_bird[] = {
	"      \\\\         //",
	"       \\\\  ___  //",
	"        ( {r}o   o{X} )",
	"     ___\\   v   /___",
	"    /    \\_____/    \\",
	"   /_/\\_/       \\_/\\_\\",
	"           /_\\",
	NULL
};

static const char *art_insect[] = {
	"      \\\\   ____   //",
	"       \\\\ /    \\ //",
	"   =====( {r}o  o{X} )=====",
	"       // \\____/ \\\\",
	"      //    ||    \\\\",
	"     //     ||     \\\\",
	NULL
};

static const char *art_golem[] = {
	"         _______",
	"        |{y} o   o {X}|",
	"        |  ___  |",
	"     ___|_______|___",
	"    |   |  ___  |   |",
	"    |___|_|   |_|___|",
	"        |  | |  |",
	"       _|__| |__|_",
	NULL
};

static const char *art_plant[] = {
	"         \\   |   /",
	"       --  ({r}@{X})  --",
	"         /   |   \\",
	"        \\\\   |   //",
	"         \\\\  |  //",
	"        {b}___|||||___",
	"       {b}/___________\\",
	NULL
};

static const char *art_aqua[] = {
	"          _     _",
	"         ( \\___/ )",
	"     __   ({y}o   o{X})   __",
	"    (  \\_/   ^   \\_/  )",
	"     \\__/   ---   \\__/",
	"        /|       |\\",
	"       /_|       |_\\",
	"   {B}~~~~~~~~~~~~~~~~~~~~~~~",
	NULL
};

static const char *art_ogre[] = {
	"         ,        ,",
	"        /(.-\"\"\"-.)\\",
	"    |\\  \\/       \\/  /|",
	"    | \\ / =.   .= \\ / |",
	"    \\( \\   {r}o\\ /o{X}   / )/",
	"     \\_, '-/   \\-' ,_/",
	"       /   \\___/   \\",
	"       \\ \\__/ \\__/ /",
	"     ___\\ \\|---|/ /___",
	NULL
};

static const char *art_grave[] = {
	"           _____",
	"          /     \\",
	"         | {w}R.I.P{X} |",
	"         |       |",
	"     {g},,,{X} |       | {g},,,",
	"    {b}___|_______|___",
	NULL
};

static const char *banner_victory[] = {
	"{c}__   _____ ___ _____ ___  _____   __",
	"{c}\\ \\ / /_ _/ __|_   _/ _ \\| _ \\ \\ / /",
	"{y} \\ V / | | (__  | || (_) |   /\\ V / ",
	"{y}  \\_/ |___\\___| |_| \\___/|_|_\\ |_|  ",
	NULL
};

static const char *banner_levelup[] = {
	"{m} _    _____   _____ _      _   _ ___ ",
	"{m}| |  | __\\ \\ / / __| |    | | | | _ \\",
	"{y}| |__| _| \\ V /| _|| |__  | |_| |  _/",
	"{y}|____|___| \\_/ |___|____|  \\___/|_|  ",
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
	{ "죽창", 8, 600 },
	{ "단검", 10, 1000 },
	{ "환도", 15, 2000 },
	{ "철검", 20, 3000 },
	{ "철퇴", 28, 6000 },
	{ "도끼", 35, 10000 },
	{ "쌍절곤", 42, 18000 },
	{ "장창", 50, 30000 },
	{ "언월도", 62, 60000 },
	{ "월도", 75, 100000 },
	{ "쌍검", 88, 180000 },
	{ "청강검", 100, 300000 },
	{ "방천화극", 125, 600000 },
	{ "현철검", 150, 1000000 },
	{ "묵룡창", 190, 1800000 },
	{ "칠성검", 225, 3000000 },
	{ "벽력검", 300, 10000000 },
	{ "진천도", 350, 18000000 },
	{ "천룡검", 400, 30000000 },
};

static const item armors[] = {
	{ "평상복", 0, 0 },
	{ "가죽옷", 4, 200 },
	{ "솜옷", 6, 600 },
	{ "누비옷", 8, 1000 },
	{ "가죽갑옷", 12, 2000 },
	{ "사슬갑옷", 16, 3000 },
	{ "쇄자갑", 22, 6000 },
	{ "비늘갑옷", 28, 10000 },
	{ "두정갑", 34, 18000 },
	{ "판금갑옷", 40, 30000 },
	{ "철엽갑", 50, 60000 },
	{ "흑철갑", 60, 100000 },
	{ "수은갑", 70, 180000 },
	{ "은린갑", 80, 300000 },
	{ "귀갑", 100, 600000 },
	{ "금강갑", 120, 1000000 },
	{ "화린갑", 150, 1800000 },
	{ "용린갑", 180, 3000000 },
	{ "천잠보의", 240, 10000000 },
	{ "봉황갑", 280, 18000000 },
	{ "신선갑", 320, 30000000 },
};

static const int item_count = sizeof(weapons) / sizeof(weapons[0]);

// 몬스터 세기를 정할 때 쓰는 "그 레벨에 알맞은" 무기/갑옷/값 (레벨 1~12)
static const int ref_weapon_power[] = { 0, 5, 10, 20, 35, 50, 75, 100, 150, 225, 300, 400 };
static const int ref_armor_power[] = { 0, 4, 8, 16, 28, 40, 60, 80, 120, 180, 240, 320 };
static const int ref_price[] = { 0, 200, 1000, 3000, 10000, 30000, 100000, 300000, 1000000, 3000000, 10000000, 30000000 };

// 장신구 (공격, 방어, 최대 체력)
struct accessory {
	const char *name;
	int atk, def, hp;
	int price;
};

static const accessory accessories[] = {
	{ "없음", 0, 0, 0, 0 },
	{ "나무 반지", 0, 3, 0, 500 },
	{ "구리 팔찌", 5, 0, 0, 1500 },
	{ "호랑이 이빨", 12, 0, 0, 8000 },
	{ "거북 부적", 0, 15, 0, 15000 },
	{ "산삼 주머니", 0, 0, 40, 40000 },
	{ "은 반지", 20, 10, 0, 120000 },
	{ "비취 목걸이", 0, 0, 100, 400000 },
	{ "금강 팔찌", 0, 60, 0, 1200000 },
	{ "용의 눈 반지", 80, 0, 0, 4000000 },
	{ "봉황의 깃털", 60, 60, 200, 15000000 },
};

static const int accessory_count = sizeof(accessories) / sizeof(accessories[0]);

// 소모품 (값은 레벨을 곱함)
enum { POTION_S, POTION_L, SMOKE, ELIXIR, CONSUMABLE_COUNT };

struct consumable {
	const char *name;
	const char *desc;
	int price;
};

static const consumable consumables[CONSUMABLE_COUNT] = {
	{ "회복약(소)", "체력을 1/3 회복", 20 },
	{ "회복약(대)", "체력을 모두 회복", 60 },
	{ "연막탄", "전투에서 반드시 도망", 30 },
	{ "힘의 영약", "그 전투 동안 공격 +50%", 80 },
};

#define MAX_CONSUMABLE	9

// 다음 레벨에 필요한 경험치 (레벨 1~11)
static const int exp_need[] = { 0, 100, 300, 700, 1500, 3000, 5500, 9000, 14000, 21000, 30000, 42000 };

struct monster_kind {
	const char *name;
	const char *art;	// b 짐승, h 사람, g 귀신, u 해골, m 도깨비, s 뱀,
				// w 새, i 벌레, o 골렘, p 식물, a 물짐승, k 거인
	const char *attack;	// 공격 모습
};

// 레벨마다 6 종 (0,1 약함 / 2,3 보통 / 4,5 강함)
static const monster_kind monsters[11][6] = {
	// 1 레벨
	{ { "들쥐 떼", "b", "이빨로 물어뜯습니다" }, { "까마귀 떼", "w", "부리로 쪼아댑니다" }, { "굶주린 들개", "b", "달려들어 뭅니다" }, { "독버섯 요정", "p", "독가루를 뿌립니다" }, { "초보 산적", "h", "몽둥이를 휘두릅니다" }, { "장수풍뎅이", "i", "뿔로 들이받습니다" } },
	// 2 레벨
	{ { "독사", "s", "독니를 들이댑니다" }, { "왕지네", "i", "독턱으로 뭅니다" }, { "멧돼지", "b", "엄니로 들이받습니다" }, { "수리부엉이", "w", "발톱으로 낚아챕니다" }, { "산적 졸개", "h", "칼을 휘두릅니다" }, { "진흙 인형", "o", "진흙 주먹을 날립니다" } },
	// 3 레벨
	{ { "늑대", "b", "목덜미를 노립니다" }, { "식인 덩굴", "p", "덩굴로 휘감습니다" }, { "도깨비", "m", "방망이를 내리칩니다" }, { "왕게", "a", "집게로 집습니다" }, { "외눈 산적", "h", "도끼를 내리찍습니다" }, { "독수리", "w", "급강하해 할큅니다" } },
	// 4 레벨
	{ { "구렁이", "s", "몸으로 휘감습니다" }, { "왕거미", "i", "거미줄을 쏘아댑니다" }, { "해골 병사", "u", "녹슨 칼을 휘두릅니다" }, { "물귀신", "g", "물속으로 끌어당깁니다" }, { "산적 두목", "h", "쌍도를 휘두릅니다" }, { "바위 거인", "o", "바위를 던집니다" } },
	// 5 레벨
	{ { "처녀 귀신", "g", "서늘한 손을 뻗습니다" }, { "독나방 떼", "i", "독가루를 흩뿌립니다" }, { "강시", "u", "두 팔을 뻗어 찌릅니다" }, { "식인 물고기", "a", "날카로운 이빨로 뭅니다" }, { "반달곰", "b", "앞발로 후려칩니다" }, { "오우거", "k", "거대한 곤봉을 휘두릅니다" } },
	// 6 레벨
	{ { "구미호", "b", "여우불을 던집니다" }, { "썩은 나무 정령", "p", "가지를 채찍처럼 휘두릅니다" }, { "독각귀", "m", "뿔로 들이받습니다" }, { "불새", "w", "불꽃 날개를 휘두릅니다" }, { "흑호", "b", "발톱으로 할큅니다" }, { "철갑 오우거", "k", "철퇴를 내리칩니다" } },
	// 7 레벨
	{ { "석상 괴물", "o", "돌주먹을 내리칩니다" }, { "대왕 전갈", "i", "꼬리 독침을 찌릅니다" }, { "야차", "m", "삼지창으로 찌릅니다" }, { "바다뱀", "a", "몸으로 조여옵니다" }, { "이무기 새끼", "s", "꼬리로 후려칩니다" }, { "저주받은 무사", "g", "원한의 검을 휘두릅니다" } },
	// 8 레벨
	{ { "불여우", "b", "불을 토합니다" }, { "뇌조", "w", "번개를 떨어뜨립니다" }, { "거대 거미", "i", "독침을 쏩니다" }, { "망령 무사", "g", "검기를 날립니다" }, { "강철 골렘", "o", "강철 주먹을 내리찍습니다" }, { "외눈 거인", "k", "나무 기둥을 휘두릅니다" } },
	// 9 레벨
	{ { "화염 도깨비", "m", "불방망이를 휘두릅니다" }, { "천년 고목", "p", "뿌리로 땅을 뒤흔듭니다" }, { "흑기사", "h", "대검을 내려칩니다" }, { "심해 거북", "a", "등껍질로 들이받습니다" }, { "해골 장수", "u", "창을 꿰찌릅니다" }, { "삼두 오우거", "k", "세 머리로 물어뜯습니다" } },
	// 10 레벨
	{ { "설인", "b", "얼음 덩이를 던집니다" }, { "천둥새", "w", "폭풍을 일으킵니다" }, { "마도사", "h", "번개 주문을 겁니다" }, { "그림자 사신", "g", "낫을 휘두릅니다" }, { "이무기", "s", "물기둥을 뿜습니다" }, { "흑요석 골렘", "o", "날카로운 팔을 휘두릅니다" } },
	// 11 레벨
	{ { "천년 구미호", "b", "아홉 꼬리로 휘감습니다" }, { "해룡", "a", "거센 물살을 뿜습니다" }, { "흑룡 새끼", "s", "검은 불을 뿜습니다" }, { "마왕의 근위병", "u", "저주받은 창을 꿰찌릅니다" }, { "마두", "h", "마공을 펼칩니다" }, { "대지의 거인", "k", "땅을 내리쳐 흔듭니다" } },
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
	int acc;				// 장신구
	int bag[CONSUMABLE_COUNT];	// 소모품 개수
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

// 그림 안의 색 표시를 글자색으로 바꿔 출력
void print_marked(const char *line, const char *base)
{
	printf("%s", base);
	for (const char *p = line; *p; p++) {
		if ( p[0] == '{' && p[1] != 0 && p[2] == '}' ) {
			switch ( p[1] ) {
				case 'r': printf(C_RED); break;
				case 'y': printf(C_YELLOW); break;
				case 'g': printf(C_GREEN); break;
				case 'c': printf(C_CYAN); break;
				case 'm': printf(C_MAGENTA); break;
				case 'w': printf(C_WHITE); break;
				case 'b': printf(C_BROWN); break;
				case 'k': printf(C_GRAY); break;
				case 'B': printf(C_BLUE); break;
				case 'X': printf("%s", base); break;
			}
			p += 2;
		} else {
			putchar(*p);
		}
	}
	printf(C_WHITE);
}

void at(int row, int col)
{
	printf("\033[%d;%dH", row, col);
}

// row, col 위치에 그림을 그린다
void draw_art(const char **art, int row, int col, const char *color)
{
	for (int i=0; art[i] != NULL; i++) {
		at(row + i, col);
		print_marked(art[i], color);
	}
}

// 지금 커서 위치부터 한 줄씩 그린다
void print_art(const char **art, const char *color)
{
	for (int i=0; art[i] != NULL; i++) {
		printf("      ");
		print_marked(art[i], color);
		printf("\r\n");
	}
}

// 체력 막대 (■ 10 칸, 남은 비율에 따라 초록/노랑/빨강)
void hp_bar(int v, int max)
{
	if ( v < 0 ) v = 0;
	int fill = (max > 0) ? (v * 10 + max - 1) / max : 0;
	if ( fill > 10 ) fill = 10;
	const char *color = (v * 2 > max) ? C_GREEN : (v * 4 > max ? C_YELLOW : C_RED);
	printf("%s", color);
	for (int i=0; i<10; i++) printf("%s", i < fill ? "■" : "□");
	printf(C_WHITE " %d/%d", v, max);
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

// 1 만이 넘고 만 단위로 떨어지면 "1500만" 처럼 줄인다 (좁은 표에서)
std::string money_short(long v)
{
	if ( v >= 10000 && v % 10000 == 0 ) {
		char buf[32];
		snprintf(buf, sizeof(buf), "%ld만", v / 10000);
		return buf;
	}
	return money(v);
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
	// 나중에 추가한 컬럼 (이미 있으면 오류가 나므로 무시)
	mysql_query(mysql, "ALTER TABLE game_hero ADD COLUMN ACC INT NOT NULL DEFAULT 0");
	mysql_query(mysql, "ALTER TABLE game_hero ADD COLUMN BAG VARCHAR(64) NOT NULL DEFAULT ''");
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
	h.acc = 0;
	for (int i=0; i<CONSUMABLE_COUNT; i++) h.bag[i] = 0;
	h.bag[POTION_S] = 2;
	h.play_date = today_string();
}

void save_hero(void)
{
	// 소지품은 "개수,개수,..." 로 저장
	std::string bag;
	for (int i=0; i<CONSUMABLE_COUNT; i++) {
		char n[16];
		snprintf(n, sizeof(n), "%s%d", i ? "," : "", h.bag[i]);
		bag += n;
	}

	char q[2048];
	snprintf(q, sizeof(q), "REPLACE INTO game_hero (USER_ID, NAME, LEVEL, EXP, HP, MAX_HP, STR, DEF, "
			"GOLD, BANK, WEAPON, ARMOR, FIGHTS, DEAD, MASTER_DONE, KILLS, WINS, DEATHS, ACC, BAG, PLAY_DATE) "
			"VALUES ('%s', '%s', %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, '%s', '%s')",
			database::escape(user_id.c_str()).c_str(), database::escape(h.name.c_str()).c_str(),
			h.level, h.exp, h.hp, h.max_hp, h.str, h.def, h.gold, h.bank, h.weapon, h.armor,
			h.fights, h.dead, h.master_done, h.kills, h.wins, h.deaths, h.acc, bag.c_str(),
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
	h.acc = atoi(r["ACC"].c_str());
	if ( h.acc < 0 || h.acc >= accessory_count ) h.acc = 0;
	for (int i=0; i<CONSUMABLE_COUNT; i++) h.bag[i] = 0;
	std::vector<std::string> bag = split_string(r["BAG"], ',');
	for (unsigned int i=0; i<bag.size() && i<CONSUMABLE_COUNT; i++) {
		h.bag[i] = atoi(bag[i].c_str());
		if ( h.bag[i] < 0 ) h.bag[i] = 0;
		if ( h.bag[i] > MAX_CONSUMABLE ) h.bag[i] = MAX_CONSUMABLE;
	}
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
	return h.str + weapons[h.weapon].power + accessories[h.acc].atk;
}

int defense_power(void)
{
	return h.def + armors[h.armor].power + accessories[h.acc].def;
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
	int w = ref_weapon_power[(level - 1 < 11) ? level - 1 : 11];
	return s + w;
}

static int ref_defense(int level)
{
	int d = 2;
	for (int i=1; i<level; i++) d += 2 + i / 2;
	int a = ref_armor_power[(level - 1 < 11) ? level - 1 : 11];
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
		case 'w': return art_bird;
		case 'i': return art_insect;
		case 'o': return art_golem;
		case 'p': return art_plant;
		case 'a': return art_aqua;
		case 'k': return art_ogre;
	}
	return art_beast;
}

// 레벨 level 의 몬스터 (strength: 0 약함, 1 보통, 2 강함)
foe make_monster(int level, int strength)
{
	if ( level > 11 ) level = 11;
	static const double mul[] = { 0.85, 1.0, 1.2 };
	double m = mul[strength] * rnd(90, 110) / 100.0;

	// 같은 세기의 두 종 가운데 하나
	const monster_kind &k = monsters[level - 1][strength * 2 + rnd(0, 1)];
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
	f.atk = (int)((ref_max_hp(level) / 8.0 + ref_defense(level) / 2.0) / 0.75 * m * 1.05);

	int need = exp_need[(level < 11) ? level : 11];
	// 레벨마다 30 번쯤 싸워야 사부님께 도전할 수 있도록 (하루 15 번이니 2~3 일에 한 레벨)
	f.exp = (int)(need / 30.0 * m) + 1;
	int price = ref_price[(level < 11) ? level : 11];
	// 그 레벨 장비 값을 20 번쯤 싸워야 모으도록
	f.gold = (int)(price / 20.0 * m) + level * 10;
	return f;
}

// ------------------------------------------------------------------
// 전투
// ------------------------------------------------------------------
enum fight_result { WIN, LOSE, RUN };

// 받는 피해 (공격력 - 방어력/2, 공격력의 50~100%)
int damage(int atk, int def)
{
	int d = rnd(atk / 2, atk) - def / 2;
	return d < 0 ? 0 : d;
}

void draw_fight(foe &f, const char *title, std::vector<std::string> &log)
{
	print_header(title);
	draw_art(f.art, 5, 2, f.color);

	int c = 44;
	at(5, c);  printf("%s◀ %s" C_WHITE, f.color, f.name.c_str());
	at(6, c + 2); hp_bar(f.hp, f.max_hp);
	at(9, c);  printf(C_GREEN "▶ " C_WHITE "%s " C_CYAN "Lv.%d" C_WHITE, string_truncate(h.name, 16, "").c_str(), h.level);
	at(10, c + 2); hp_bar(h.hp, h.max_hp);
	at(11, c + 2); printf(C_GRAY "공격 %d  방어 %d" C_WHITE, attack_power(), defense_power());
	at(12, c + 2); printf(C_GRAY "%s / %s" C_WHITE, weapons[h.weapon].name, armors[h.armor].name);

	at(15, 1); printf(C_GRAY "%s" C_WHITE, repeat("─", 39).c_str());
	unsigned int start = log.size() > 5 ? log.size() - 5 : 0;
	int r = 16;
	for (unsigned int i=start; i<log.size(); i++, r++) {
		at(r, 2);
		printf("%s", log[i].c_str());
	}
	at(21, 1); printf(C_GRAY "%s" C_WHITE, repeat("─", 39).c_str());
	at(22, 1);
}

// can_run: 도망칠 수 있는지 (사부님 결투는 도망 대신 포기)
fight_result fight(foe &f, const char *title, bool can_run)
{
	std::vector<std::string> log;
	log.push_back(std::string(f.color) + f.name + C_WHITE " 이(가) 나타났습니다!");
	bool elixir = false;	// 힘의 영약을 먹었는지

	while (1) {
		draw_fight(f, title, log);

		if ( f.hp <= 0 || h.hp <= 0 ) {
			printf(" " C_GRAY "[Enter] 를 누르세요." C_WHITE);
			press_enter();
			break;
		}

		std::string cmd = ask(can_run ?
				" " C_YELLOW "[A]" C_WHITE "공격 " C_YELLOW "[S]" C_WHITE "혼신의 일격 " C_YELLOW "[I]" C_WHITE "아이템 " C_YELLOW "[R]" C_WHITE "도망 >> " :
				" " C_YELLOW "[A]" C_WHITE "공격 " C_YELLOW "[S]" C_WHITE "혼신의 일격 " C_YELLOW "[I]" C_WHITE "아이템 " C_YELLOW "[R]" C_WHITE "포기 >> ");
		int atk = elixir ? attack_power() * 3 / 2 : attack_power();

		char buf[256];
		if ( !strcasecmp(cmd.c_str(), "i") ) {
			at(23, 1);
			printf("\033[K ");
			for (int i=0; i<CONSUMABLE_COUNT; i++) {
				printf(C_YELLOW "%d" C_WHITE ".%s(%d) ", i + 1, consumables[i].name, h.bag[i]);
			}
			std::string a = ask(">> ");
			int n = atoi(a.c_str()) - 1;
			if ( n < 0 || n >= CONSUMABLE_COUNT ) continue;
			if ( h.bag[n] <= 0 ) {
				log.push_back(C_GRAY + std::string(consumables[n].name) + " 이(가) 없습니다." C_WHITE);
				continue;
			}
			if ( n == SMOKE && !can_run ) {
				log.push_back(C_GRAY "사부님과의 결투에서는 쓸 수 없습니다." C_WHITE);
				continue;
			}
			h.bag[n]--;
			if ( n == POTION_S || n == POTION_L ) {
				int heal = (n == POTION_S) ? h.max_hp / 3 + 1 : h.max_hp;
				if ( heal > h.max_hp - h.hp ) heal = h.max_hp - h.hp;
				h.hp += heal;
				snprintf(buf, sizeof(buf), "%s 을(를) 마셨습니다. 체력 " C_GREEN "+%d" C_WHITE, consumables[n].name, heal);
				log.push_back(buf);
			} else if ( n == SMOKE ) {
				save_hero();
				at(23, 1);
				printf("\033[K " C_YELLOW "연막탄을 던지고 연기 속으로 사라졌습니다." C_WHITE);
				wait_enter();
				return RUN;
			} else if ( n == ELIXIR ) {
				elixir = true;
				log.push_back(C_MAGENTA "힘의 영약을 마셨습니다. 온몸에 힘이 넘칩니다!" C_WHITE);
			}
			save_hero();
		} else if ( !strcasecmp(cmd.c_str(), "r") ) {
			if ( !can_run ) return RUN;
			if ( rnd(1, 100) <= 55 ) {
				at(23, 1);
				printf(" " C_YELLOW "무사히 도망쳤습니다." C_WHITE);
				wait_enter();
				return RUN;
			}
			log.push_back(C_GRAY "도망치려 했지만 붙잡혔습니다!" C_WHITE);
		} else if ( !strcasecmp(cmd.c_str(), "s") ) {
			// 혼신의 일격: 40% 는 빗나가고, 맞으면 2.5 배
			if ( rnd(1, 100) <= 40 ) {
				log.push_back(C_GRAY "혼신의 일격이 크게 빗나갔습니다!" C_WHITE);
			} else {
				int d = damage(atk, f.def) * 5 / 2 + 1;
				f.hp -= d;
				snprintf(buf, sizeof(buf), C_YELLOW "혼신의 일격!" C_WHITE " %s 에게 " C_YELLOW "%d" C_WHITE " 의 피해!", f.name.c_str(), d);
				log.push_back(buf);
				if ( f.hp <= 0 ) continue;
			}
		} else if ( !strcasecmp(cmd.c_str(), "a") || cmd.empty() ) {
			int d = damage(atk, f.def);
			// 가끔 회심의 일격
			bool crit = rnd(1, 100) <= 8;
			if ( crit ) d = d * 2 + 1;
			f.hp -= d;
			if ( d == 0 ) snprintf(buf, sizeof(buf), C_GRAY "공격이 빗나갔습니다." C_WHITE);
			else if ( crit ) snprintf(buf, sizeof(buf), C_YELLOW "회심의 일격!" C_WHITE " %s 에게 " C_YELLOW "%d" C_WHITE " 의 피해!", f.name.c_str(), d);
			else snprintf(buf, sizeof(buf), "%s 에게 " C_YELLOW "%d" C_WHITE " 의 피해를 입혔습니다.", f.name.c_str(), d);
			log.push_back(buf);
			if ( f.hp <= 0 ) continue;
		} else {
			continue;
		}

		// 적의 반격
		int d = damage(f.atk, defense_power());
		h.hp -= d;
		if ( d == 0 ) snprintf(buf, sizeof(buf), "%s 의 공격을 " C_GREEN "피했습니다." C_WHITE, f.name.c_str());
		else snprintf(buf, sizeof(buf), "%s 이(가) %s. " C_RED "%d" C_WHITE " 의 피해!", f.name.c_str(), f.attack_msg.c_str(), d);
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
		printf("\r\n  남은 사냥 " C_YELLOW "%d" C_WHITE "   체력 ", h.fights);
		hp_bar(h.hp, h.max_hp);
		printf("   소지금 " C_YELLOW "%s냥" C_WHITE "\r\n", money(h.gold).c_str());

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

			// 가끔 소모품을 떨어뜨린다
			int drop = -1;
			int dr = rnd(1, 100);
			if ( dr <= 3 ) drop = ELIXIR;
			else if ( dr <= 6 ) drop = POTION_L;
			else if ( dr <= 9 ) drop = SMOKE;
			else if ( dr <= 18 ) drop = POTION_S;
			if ( drop >= 0 && h.bag[drop] < MAX_CONSUMABLE ) h.bag[drop]++;
			else drop = -1;
			save_hero();

			print_header("승리");
			draw_art(banner_victory, 6, 22, C_CYAN);
			at(12, 1);
			printf("\r\n      " C_YELLOW "%s" C_WHITE " 을(를) 물리쳤습니다!\r\n", f.name.c_str());
			printf("\r\n      경험치  " C_CYAN "+%s" C_WHITE "   (%s", money(f.exp).c_str(), money(h.exp).c_str());
			if ( h.level < MAX_LEVEL ) printf(" / %s", money(exp_need[h.level]).c_str());
			printf(")");
			printf("\r\n      돈      " C_YELLOW "+%s 냥" C_WHITE "   (소지금 %s 냥)", money(f.gold).c_str(), money(h.gold).c_str());
			printf("\r\n      체력    ");
			hp_bar(h.hp, h.max_hp);
			printf("\r\n");
			if ( drop >= 0 ) {
				printf("\r\n      " C_GREEN "%s 이(가) %s 을(를) 떨어뜨렸습니다!" C_WHITE "\r\n", f.name.c_str(), consumables[drop].name);
			}
			if ( h.level < MAX_LEVEL && h.exp >= exp_need[h.level] ) {
				printf("\r\n      " C_MAGENTA "★ 사부님께 도전할 만큼 강해졌습니다! (마을에서 M)" C_WHITE "\r\n");
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
	f.max_hp = (int)(f.max_hp * 1.4);
	f.hp = f.max_hp;
	f.atk = (int)(f.atk * 1.2);

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

		print_header("레벨 업");
		draw_art(banner_levelup, 6, 22, C_MAGENTA);
		at(12, 1);
		printf("\r\n      " C_CYAN "%s" C_WHITE ": \"훌륭하구나! 이제 너는 " C_YELLOW "%d 레벨" C_WHITE "이다.\"\r\n", masters[h.level - 1], h.level);
		printf("\r\n      최대 체력 " C_GREEN "+%d" C_WHITE "   힘 " C_RED "+%d" C_WHITE "   방어 " C_CYAN "+%d" C_WHITE, add_hp, add_str, add_def);
		printf("\r\n      체력      ");
		hp_bar(h.hp, h.max_hp);
		printf("\r\n");
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
// 잡화점: 장신구와 소모품
// ------------------------------------------------------------------
static const char *art_store[] = {
	"      _______________",
	"     /{y}   잡  화  점  {X}\\",
	"    |  {r}[]{X}  {g}[]{X}  {c}[]{X}  {y}[]{X} |",
	"    |_________________|",
	NULL
};

void go_store(void)
{
	while (1) {
		print_header("잡화점");
		print_art(art_store, C_BROWN);
		printf("  소지금 " C_YELLOW "%s 냥" C_WHITE "   장신구 " C_CYAN "%s" C_WHITE "\r\n",
				money(h.gold).c_str(), accessories[h.acc].name);
		printf(C_GRAY " %s" C_WHITE "\r\n", repeat("─", 39).c_str());

		// 장신구 (두 단)
		int rows = (accessory_count - 1 + 1) / 2;
		for (int r=0; r<rows; r++) {
			for (int c=0; c<2; c++) {
				int i = 1 + c * rows + r;
				if ( i >= accessory_count ) break;
				const accessory &a = accessories[i];
				char eff[32] = "";
				if ( a.atk ) snprintf(eff + strlen(eff), sizeof(eff) - strlen(eff), "공%d ", a.atk);
				if ( a.def ) snprintf(eff + strlen(eff), sizeof(eff) - strlen(eff), "방%d ", a.def);
				if ( a.hp ) snprintf(eff + strlen(eff), sizeof(eff) - strlen(eff), "체%d ", a.hp);
				if ( eff[0] ) eff[strlen(eff) - 1] = 0;
				printf("%s%s%2d. %-12s %-14s%6s냥" C_WHITE,
						c > 0 ? " " : "",
						i == h.acc ? C_CYAN : (a.price <= h.gold ? C_WHITE : C_GRAY),
						i, a.name, eff, money_short(a.price).c_str());
			}
			printf("\r\n");
		}

		printf(C_GRAY " %s" C_WHITE "\r\n", repeat("─", 39).c_str());
		for (int i=0; i<CONSUMABLE_COUNT; i++) {
			printf("  " C_YELLOW "%c" C_WHITE ". %-10s %-22s %6s냥  (가진 것 %d)\r\n", 'A' + i,
					consumables[i].name, consumables[i].desc, money(consumables[i].price * h.level).c_str(), h.bag[i]);
		}

		std::string cmd = ask("\r\n 장신구 번호 / 소모품 글자 (Q:나가기) >> ");
		if ( !strcasecmp(cmd.c_str(), "q") || !strcasecmp(cmd.c_str(), "p") || cmd.empty() ) return;

		// 소모품
		if ( cmd.size() == 1 && toupper(cmd[0]) >= 'A' && toupper(cmd[0]) < 'A' + CONSUMABLE_COUNT ) {
			int n = toupper(cmd[0]) - 'A';
			int price = consumables[n].price * h.level;
			if ( h.bag[n] >= MAX_CONSUMABLE ) {
				printf("\r\n 더 가질 수 없습니다. (최대 %d 개)", MAX_CONSUMABLE);
			} else if ( h.gold < price ) {
				printf("\r\n " C_RED "돈이 모자랍니다." C_WHITE);
			} else {
				h.gold -= price;
				h.bag[n]++;
				save_hero();
				printf("\r\n " C_YELLOW "%s" C_WHITE " 을(를) 샀습니다.", consumables[n].name);
			}
			wait_enter();
			continue;
		}

		// 장신구 (가지고 있던 것은 값의 절반을 쳐줌)
		int n = atoi(cmd.c_str());
		if ( n < 1 || n >= accessory_count ) continue;
		if ( n == h.acc ) {
			printf("\r\n 이미 지니고 있습니다.");
			wait_enter();
			continue;
		}
		int refund = accessories[h.acc].price / 2;
		if ( h.gold + refund < accessories[n].price ) {
			printf("\r\n " C_RED "돈이 모자랍니다." C_WHITE);
			wait_enter();
			continue;
		}
		h.gold = h.gold + refund - accessories[n].price;
		// 최대 체력은 장신구만큼 바꿔 둔다
		h.max_hp += accessories[n].hp - accessories[h.acc].hp;
		if ( h.hp > h.max_hp ) h.hp = h.max_hp;
		h.acc = n;
		save_hero();
		printf("\r\n " C_YELLOW "%s" C_WHITE " 을(를) 샀습니다!", accessories[n].name);
		wait_enter();
	}
}

// 소지품 보기 / 회복약 쓰기
void show_bag(void)
{
	while (1) {
		print_header("소지품");
		printf("\r\n  무기   " C_CYAN "%-12s" C_WHITE " 공격 +%d\r\n", weapons[h.weapon].name, weapons[h.weapon].power);
		printf("  갑옷   " C_CYAN "%-12s" C_WHITE " 방어 +%d\r\n", armors[h.armor].name, armors[h.armor].power);
		const accessory &a = accessories[h.acc];
		printf("  장신구 " C_CYAN "%-12s" C_WHITE " 공격 +%d  방어 +%d  체력 +%d\r\n", a.name, a.atk, a.def, a.hp);
		printf("\r\n  체력 ");
		hp_bar(h.hp, h.max_hp);
		printf("\r\n\r\n");
		for (int i=0; i<CONSUMABLE_COUNT; i++) {
			printf("  " C_YELLOW "%d" C_WHITE ". %-10s %d 개   " C_GRAY "%s" C_WHITE "\r\n", i + 1,
					consumables[i].name, h.bag[i], consumables[i].desc);
		}

		std::string cmd = ask("\r\n 회복약 쓰기(1,2)  Q:나가기 >> ");
		if ( !strcasecmp(cmd.c_str(), "q") || !strcasecmp(cmd.c_str(), "p") || cmd.empty() ) return;
		int n = atoi(cmd.c_str()) - 1;
		if ( n != POTION_S && n != POTION_L ) continue;
		if ( h.dead ) {
			printf("\r\n 쓰러진 몸은 하룻밤 쉬어야 합니다.");
			wait_enter();
			continue;
		}
		if ( h.bag[n] <= 0 || h.hp >= h.max_hp ) continue;
		h.bag[n]--;
		int heal = (n == POTION_S) ? h.max_hp / 3 + 1 : h.max_hp;
		if ( heal > h.max_hp - h.hp ) heal = h.max_hp - h.hp;
		h.hp += heal;
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
	printf("  " C_CYAN "%s" C_WHITE "  Lv." C_YELLOW "%d" C_WHITE "  체력 ", string_truncate(h.name, 12, "").c_str(), h.level);
	hp_bar(h.hp, h.max_hp);
	printf("  공격 " C_RED "%d" C_WHITE "  방어 " C_CYAN "%d" C_WHITE "\r\n", attack_power(), defense_power());
	printf("  경험치 %s", money(h.exp).c_str());
	if ( h.level < MAX_LEVEL ) printf("/%s", money(exp_need[h.level]).c_str());
	printf("   소지금 " C_YELLOW "%s냥" C_WHITE "   남은 사냥 " C_GREEN "%d" C_WHITE "   영웅 " C_MAGENTA "%d" C_WHITE "번\r\n",
			money(h.gold).c_str(), h.fights, h.wins);
	printf("  " C_GRAY "%s / %s / %s   회복약 %d+%d  연막탄 %d  영약 %d" C_WHITE "\r\n", weapons[h.weapon].name, armors[h.armor].name,
			accessories[h.acc].name, h.bag[POTION_S], h.bag[POTION_L], h.bag[SMOKE], h.bag[ELIXIR]);
}

// ------------------------------------------------------------------
// 마을
// ------------------------------------------------------------------
void town(void)
{
	while (1) {
		check_new_day();
		print_header(C_YELLOW "용사의 전설" C_WHITE " - 하늘마을");
		print_art(art_town, C_BROWN);
		show_status();
		printf(C_GRAY " %s" C_WHITE "\r\n", repeat("─", 39).c_str());
		printf("  " C_YELLOW "[F]" C_GREEN " ♣" C_WHITE " 숲으로 사냥    " C_YELLOW "[M]" C_MAGENTA " ★" C_WHITE " 사부님께 도전  " C_YELLOW "[H]" C_RED " ♥" C_WHITE " 약방\r\n");
		printf("  " C_YELLOW "[W]" C_GRAY " ♠" C_WHITE " 대장간(무기)   " C_YELLOW "[A]" C_CYAN " ◆" C_WHITE " 갑옷 가게      " C_YELLOW "[B]" C_YELLOW " ●" C_WHITE " 전장(돈 맡기기)\r\n");
		printf("  " C_YELLOW "[G]" C_GREEN " ◎" C_WHITE " 잡화점         " C_YELLOW "[I]" C_CYAN " ▣" C_WHITE " 소지품         " C_YELLOW "[R]" C_YELLOW " ☆" C_WHITE " 용사 순위\r\n");
		printf("  " C_YELLOW "[N]" C_CYAN " ♪" C_WHITE " 마을 소식      " C_YELLOW "[Q]" C_GRAY " ◁" C_WHITE " 게임 끝내기\r\n");

		std::string cmd = ask(" 선택 >> ");
		const char *c = cmd.c_str();

		if ( !strcasecmp(c, "q") || !strcasecmp(c, "p") || !strcasecmp(c, "x") ) break;
		else if ( !strcasecmp(c, "f") ) go_forest();
		else if ( !strcasecmp(c, "m") ) go_master();
		else if ( !strcasecmp(c, "h") ) go_healer();
		else if ( !strcasecmp(c, "w") ) go_shop(true);
		else if ( !strcasecmp(c, "a") ) go_shop(false);
		else if ( !strcasecmp(c, "b") ) go_bank();
		else if ( !strcasecmp(c, "g") ) go_store();
		else if ( !strcasecmp(c, "i") ) show_bag();
		else if ( !strcasecmp(c, "r") ) show_rank();
		else if ( !strcasecmp(c, "n") ) show_news();
	}
}

void title_screen(bool first)
{
	print_header("용사의 전설");
	draw_art(art_title, 5, 24, C_YELLOW);
	draw_art(art_dragon, 10, 20, C_RED);
	at(20, 1);
	printf("              " C_RED "~ 붉은 용을 쓰러뜨릴 용사를 찾습니다 ~" C_WHITE "\r\n");
	if ( !first ) {
		printf("\r\n              다시 오셨군요, " C_CYAN "%s" C_WHITE " 님.", h.name.c_str());
		wait_enter();
		return;
	}
	wait_enter();

	print_header("용사의 전설 - 이야기");
	print_art(art_forest, C_GREEN);
	printf("\r\n  하늘마을 북쪽 숲에는 오래전부터 " C_RED "붉은 용" C_WHITE "이 살고 있습니다.\r\n");
	printf("  숲에서 몬스터를 물리쳐 경험을 쌓고, " C_CYAN "사부님" C_WHITE "을 이겨 실력을 올리세요.\r\n");
	printf("  " C_YELLOW "12 레벨" C_WHITE "이 되면 용의 둥지에 들어갈 수 있습니다.\r\n\r\n");
	printf("  하루에 숲 사냥은 " C_GREEN "%d 번" C_WHITE ". 쓰러지면 지닌 돈을 잃고 다음 날 깨어납니다.\r\n", FIGHTS_PER_DAY);
	printf("  돈은 " C_YELLOW "전장" C_WHITE "에 맡겨 두세요!\r\n");
	printf("  전투에서 " C_YELLOW "[S] 혼신의 일격" C_WHITE "은 빗나가기 쉽지만 맞으면 2.5 배 피해를 줍니다.\r\n");
	printf("  " C_YELLOW "잡화점" C_WHITE "에서 장신구와 회복약/연막탄/영약을 살 수 있습니다. (전투 중 [I])\r\n");
	printf("\r\n  새 용사 " C_CYAN "%s" C_WHITE " 의 모험이 시작됩니다.\r\n", h.name.c_str());
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

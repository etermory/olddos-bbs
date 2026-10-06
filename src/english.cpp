#include "main.h"

// ------------------------------------------------------------------
// 오늘의 영어 한 문장 : 날짜마다 바뀌는 쉬운 생활 영어, 뜻, 짧은 풀이
//   bin/english <호스트이름> <아이디> <tty>
//   bin/english --line : 오늘의 문장 한 줄 "문장<TAB>뜻" (화면 파일 태그용)
// ------------------------------------------------------------------

struct termio sys_term;

char title[1024] = "오늘의 영어 한 문장";

char tty[10];

char host_name[256];

#define E_W		"\033[=15F"
#define E_Y		"\033[=14F"
#define E_C		"\033[=11F"
#define E_G		"\033[=7F"

struct sentence { const char *en, *ko, *tip; };
static const sentence sentences[] = {
	{ "Long time no see.", "오랜만이에요.", "오랜만에 만난 사람에게. 친한 사이에서 많이 씁니다." },
	{ "What have you been up to?", "그동안 어떻게 지냈어요?", "be up to ~ : ~을 하며 지내다" },
	{ "I'm on my way.", "가는 중이에요.", "약속 장소로 가고 있을 때. on my way = 가는 길에" },
	{ "Take your time.", "천천히 하세요.", "서두르지 않아도 된다고 말할 때" },
	{ "It's up to you.", "당신 뜻대로 하세요.", "be up to someone : ~에게 달려 있다" },
	{ "I'll keep that in mind.", "명심할게요.", "keep in mind : 기억해 두다" },
	{ "Let me know if you need anything.", "필요한 것 있으면 말해요.", "let me know : 알려 주다" },
	{ "That makes sense.", "그렇군요. (말이 되네요)", "make sense : 이치에 맞다, 이해가 되다" },
	{ "I couldn't agree more.", "전적으로 동의해요.", "더 동의할 수 없을 만큼 = 완전히 동의" },
	{ "It's not a big deal.", "별일 아니에요.", "big deal : 대단한 일" },
	{ "Better late than never.", "늦더라도 안 하는 것보다 낫다.", "자주 쓰는 속담" },
	{ "Practice makes perfect.", "연습이 완벽을 만든다.", "타자 연습에도 딱 맞는 말" },
	{ "Every cloud has a silver lining.", "괴로움 뒤에는 기쁨이 있다.", "silver lining : 구름의 은빛 가장자리 = 희망" },
	{ "Actions speak louder than words.", "말보다 행동이 중요하다.", "speak louder : 더 크게 말하다 = 더 강하다" },
	{ "Don't put off until tomorrow what you can do today.", "오늘 할 일을 내일로 미루지 마라.", "put off : 미루다" },
	{ "Could you do me a favor?", "부탁 하나 들어줄래요?", "do someone a favor : 부탁을 들어주다" },
	{ "I'm looking forward to it.", "기대하고 있어요.", "look forward to ~ing / 명사" },
	{ "Sorry to keep you waiting.", "기다리게 해서 미안해요.", "keep someone waiting : 기다리게 하다" },
	{ "Help yourself.", "마음껏 드세요.", "음식을 권할 때" },
	{ "It's on me.", "제가 낼게요.", "계산할 때. on me = 내가 낸다" },
	{ "Let's call it a day.", "오늘은 여기까지 합시다.", "call it a day : 하루 일을 마치다" },
	{ "I'm all ears.", "잘 듣고 있어요.", "온몸이 귀 = 귀 기울여 듣다" },
	{ "Break a leg!", "행운을 빌어요!", "공연, 시험 전에 하는 응원" },
	{ "Hang in there.", "조금만 더 힘내요.", "힘든 사람을 응원할 때" },
	{ "You made my day.", "덕분에 정말 기분 좋아요.", "make someone's day : 하루를 즐겁게 하다" },
	{ "I didn't catch your name.", "성함을 못 들었어요.", "catch : (말을) 알아듣다" },
	{ "Could you say that again?", "다시 말씀해 주시겠어요?", "못 알아들었을 때 정중하게" },
	{ "How do you spell that?", "철자가 어떻게 되나요?", "아이디나 이름을 물어볼 때도" },
	{ "What do you do for fun?", "취미가 뭐예요?", "for fun : 재미로" },
	{ "I'm into old computers.", "옛날 컴퓨터에 빠져 있어요.", "be into ~ : ~에 푹 빠지다" },
	{ "My computer froze.", "컴퓨터가 멈췄어요.", "freeze (froze) : 얼다, 멈추다" },
	{ "Did you save the file?", "파일 저장했어요?", "save : 저장하다" },
	{ "The connection is slow today.", "오늘 접속이 느리네요.", "connection : 연결, 접속" },
	{ "Let's keep in touch.", "계속 연락하고 지내요.", "keep in touch : 연락을 이어가다" },
	{ "See you around.", "또 봐요.", "가볍게 헤어질 때" },
	{ "Have a good one.", "좋은 하루 보내요.", "one = day, night 등" },
	{ "No worries.", "걱정 마세요. 괜찮아요.", "고맙다/미안하다는 말에 답할 때" },
	{ "I'll get back to you.", "나중에 다시 연락드릴게요.", "get back to : 다시 연락하다" },
	{ "Fingers crossed.", "잘되길 빌어요.", "손가락을 꼬는 행운의 몸짓에서" },
	{ "It's a piece of cake.", "식은 죽 먹기예요.", "아주 쉬운 일" },
	{ "Easy does it.", "살살 해요. 천천히.", "조심스럽게 하라고 할 때" },
	{ "I'm running late.", "좀 늦을 것 같아요.", "run late : 늦어지다" },
	{ "What's the weather like today?", "오늘 날씨 어때요?", "GO WEATHER 로 확인해 보세요" },
	{ "It's raining cats and dogs.", "비가 억수같이 와요.", "아주 세게 오는 비" },
	{ "Time flies.", "시간 참 빠르네요.", "Time flies when you're having fun." },
	{ "Never mind.", "신경 쓰지 마세요.", "괜찮다고 말을 거둘 때" },
	{ "Same here.", "저도 그래요.", "Me too 와 비슷" },
	{ "You can say that again.", "맞아요, 정말 그래요.", "강하게 동의할 때" },
	{ "Let's get started.", "시작해 봅시다.", "get started : 시작하다" },
	{ "Mind your step.", "발밑 조심하세요.", "mind : 조심하다" },
	{ "What a small world!", "세상 참 좁네요!", "뜻밖의 곳에서 아는 사람을 만났을 때" },
	{ "Count me in.", "저도 끼워 주세요.", "모임, 놀이에 참여하겠다고 할 때 (반대: count me out)" },
	{ "I owe you one.", "신세 졌어요.", "owe : 빚지다" },
	{ "Keep up the good work.", "계속 잘해 주세요.", "칭찬과 격려" },
	{ "Let's play it by ear.", "상황 봐서 정해요.", "악보 없이 귀로 연주하다 = 그때그때" },
	{ "I'm not a morning person.", "저는 아침형 인간이 아니에요.", "morning person : 아침에 강한 사람" },
	{ "The early bird catches the worm.", "일찍 일어나는 새가 벌레를 잡는다.", "부지런하면 얻는 것이 있다" },
	{ "Rome wasn't built in a day.", "로마는 하루아침에 이루어지지 않았다.", "큰일은 시간이 걸린다" },
	{ "Where there's a will, there's a way.", "뜻이 있는 곳에 길이 있다.", "will : 의지" },
	{ "Two heads are better than one.", "백지장도 맞들면 낫다.", "둘이 생각하면 혼자보다 낫다" },
};
static const int sentence_count = sizeof(sentences) / sizeof(sentences[0]);

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

// 오늘의 문장 번호 (날짜마다 하나, 1970-01-01 부터 날 수)
static int today_index(void)
{
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	long days = (long)((t + tm->tm_gmtoff) / 86400);
	return (int)(days % sentence_count);
}

// 가운데 맞춰 한 줄 (80 칸)
static void center_line(const char *color, const std::string &s)
{
	int w = s.size();
	int pad = w < 78 ? (80 - w) / 2 : 1;
	printf("%s%s%s" E_W "\r\n", std::string(pad, ' ').c_str(), color, s.c_str());
}

int main(int argc, char **argv)
{
	if ( argc > 1 && !strcmp(argv[1], "--line") ) {
		const sentence &s = sentences[today_index()];
		printf("%s\t%s\n", s.en, s.ko);
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

	int today = today_index();
	int shift = 0;		// 오늘에서 며칠 앞/뒤 문장

	while ( 1 ) {
		int i = ((today + shift) % sentence_count + sentence_count) % sentence_count;
		const sentence &s = sentences[i];
		time_t t = time(NULL) + shift * 86400L;
		struct tm *tm = localtime(&t);
		static const char *w[] = { "일", "월", "화", "수", "목", "금", "토" };

		print_header(title);
		char head[64];
		snprintf(head, sizeof(head), "%d월 %d일 (%s)%s", tm->tm_mon + 1, tm->tm_mday, w[tm->tm_wday],
				shift == 0 ? " 오늘의 문장" : shift < 0 ? " 지난 문장" : " 다음 문장");
		printf("\r\n\r\n");
		center_line(E_G, head);
		printf("\r\n\r\n");
		center_line(E_Y, s.en);
		printf("\r\n");
		center_line(E_C, s.ko);
		printf("\r\n\r\n");
		center_line(E_G, std::string("─ ") + s.tip + " ─");
		printf("\r\n\r\n\r\n");
		printf("  " E_G "%d / %d" E_W "\r\n", i + 1, sentence_count);

		char cmd[16];
		printf(ESC_ENG);
		printf("\r\n이전(B) 다음(N) 오늘(T)  상위메뉴(P) 종료(X) >> ");
		line_input(cmd, 4);
		std::string c = trim(cmd);
		if ( !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "x") || !strcasecmp(c.c_str(), "q") ) break;
		if ( !strcasecmp(c.c_str(), "b") ) shift--;
		else if ( !strcasecmp(c.c_str(), "n") || c.empty() ) shift++;
		else if ( !strcasecmp(c.c_str(), "t") ) shift = 0;
	}

	host_close();
	return 0;
}

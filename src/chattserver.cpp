#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <iconv.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>

#include <string>
#include <vector>
#include <deque>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

// ------------------------------------------------------------------
// 대화방 서버 : bin/chattserver <포트> <최대 인원> <환영 메세지> [permanent]
//   permanent 이면 '만남의 광장' 처럼 늘 열려 있는 방 (방장 없음, 아무도 없어도 닫히지 않음)
// 명령: /SAY 귓속말, /LIST 접속자, /ME 행동, /주사위(/DICE), /끝말잇기(/WORD), /퀴즈(/QUIZ), /HELP
// 들어온 사람에게 최근 대화 10 줄을 보여 주고, 너무 빨리/같은 말을 되풀이하면 막는다.
// ------------------------------------------------------------------

#define MAX_LINE 	1024
#define MAX_CLIENT 	1000
#define HISTORY		10

#define C_WHITE		"\033[=15F"
#define C_YELLOW	"\033[=14F"
#define C_RED		"\033[=12F"
#define C_GREEN		"\033[=10F"
#define C_CYAN		"\033[=11F"
#define C_MAGENTA	"\033[=13F"
#define C_GRAY		"\033[=7F"

char greeting[1024];

int max_user;
int server_fd;
int server_port;
bool permanent = false;

struct client {
	bool author;
	char userid[256];
	char nickname[256];
	char ip[40];
	int socket;
	int port;
	time_t joined;			// 들어온 시각
	long last_ms;			// 마지막으로 말한 시각 (도배 막기)
	std::string last_text;	// 마지막으로 한 말
	int repeat;				// 같은 말을 연달아 한 횟수
	int color;				// 닉네임 색 (들어온 순서대로)
	bool time_on;			// 말한 시각 표시 (/TIME)
};

std::vector<client> clients;
std::deque<std::string> history;	// 최근 대화 (들어온 사람에게 보여 줌)
std::string room_title;				// 방장이 /TITLE 로 바꾼 주제 (없으면 처음 주제)
std::map<std::string, time_t> banned;	// /KICK 당한 아이디 -> 다시 들어올 수 있는 시각
static int next_color = 0;

// 닉네임 색 (파랑 바탕에서 잘 보이는 것만)
static const char *name_colors[] = { "\033[=14F", "\033[=11F", "\033[=10F", "\033[=13F", "\033[=12F", "\033[=15F" };
#define NAME_COLORS	6

int send_msg(int socket, const char *msg);
int recv_msg(int socket, char *msg, int bufsize);
void write_room_info(void);

std::vector<std::string> split_string(std::string str, char delimiter)
{
	std::stringstream test(str);
	std::string segment;
	std::vector<std::string> result;

	while(std::getline(test, segment, delimiter))
	   result.push_back(segment);

	return result;
}

//앞에 있는 개행 문자 제거
std::string ltrim(std::string s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
				std::not1(std::ptr_fun<int, int>(std::isspace))));
	return s;
}

//뒤에 있는 개행 문자 제거
std::string rtrim(std::string s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(),
				std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	return s;
}

//양쪽 끝의 개행 문자 제거
std::string trim(std::string s)
{
	return ltrim(rtrim(s));
}

// 제어문자(ESC 등) 제거 - ANSI 코드 삽입 방지
std::string strip_control(const std::string &s)
{
	std::string r;
	for (unsigned int i=0; i<s.size(); i++) {
		unsigned char ch = (unsigned char)s[i];
		if (ch < 0x20 || ch == 0x7f) continue;
		r += s[i];
	}
	return r;
}

static long now_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000L + tv.tv_usec / 1000;
}

static std::string itos(long v)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%ld", v);
	return buf;
}

// 색 코드를 뺀 글 (대화 기록용)
static std::string strip_codes(const std::string &s)
{
	std::string r;
	for ( std::string::size_type i = 0; i < s.size(); i++ ) {
		if ( s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[' ) {
			i += 2;
			while ( i < s.size() && !isalpha((unsigned char)s[i]) ) i++;
			continue;
		}
		if ( s[i] == '\r' ) continue;
		r += s[i];
	}
	return r;
}

// ------------------------------------------------------------------
// 대화 기록: chatt/log/YYYY-MM-DD.log (7 일 보관, 귓속말은 남기지 않음)
// ------------------------------------------------------------------
static void log_line(const std::string &msg)
{
	char dir[1024], path[1100], day[16], hms[16];
	snprintf(dir, sizeof(dir), "%s/chatt/log", getenv("HANULSO"));
	mkdir(dir, 0755);
	time_t t = time(NULL);
	strftime(day, sizeof(day), "%Y-%m-%d", localtime(&t));
	strftime(hms, sizeof(hms), "%H:%M:%S", localtime(&t));
	snprintf(path, sizeof(path), "%s/%s.log", dir, day);
	FILE *fp = fopen(path, "a");
	if ( fp == NULL ) return;
	std::vector<std::string> lines = split_string(strip_codes(msg), '\n');
	for ( unsigned int i = 0; i < lines.size(); i++ ) {
		if ( trim(lines[i]).empty() ) continue;
		fprintf(fp, "[%s] [%d] %s\n", hms, server_port, lines[i].c_str());
	}
	fclose(fp);
}

// 7 일 지난 기록은 지운다 (서버를 켤 때와 하루에 한 번)
static void clean_logs(void)
{
	char dir[1024];
	snprintf(dir, sizeof(dir), "%s/chatt/log", getenv("HANULSO"));
	DIR *d = opendir(dir);
	if ( d == NULL ) return;
	time_t limit = time(NULL) - 7 * 86400;
	char oldest[16];
	strftime(oldest, sizeof(oldest), "%Y-%m-%d", localtime(&limit));
	struct dirent *e;
	while ( (e = readdir(d)) != NULL ) {
		std::string n = e->d_name;
		// YYYY-MM-DD.log 이고 날짜가 7 일 전보다 앞이면
		if ( n.size() == 14 && n.compare(10, 4, ".log") == 0 && n.substr(0, 10) < oldest ) {
			unlink((std::string(dir) + "/" + n).c_str());
		}
	}
	closedir(d);
}

// 말한 시각을 붙인다 (/TIME 을 켠 사람에게만). 메세지는 "\r\n..." 꼴
static std::string stamp(const std::string &msg)
{
	char hm[16];
	time_t t = time(NULL);
	strftime(hm, sizeof(hm), "%H:%M", localtime(&t));
	std::string s = std::string(C_GRAY "[") + hm + "]" C_WHITE " ";
	if ( msg.compare(0, 2, "\r\n") == 0 ) return "\r\n" + s + msg.substr(2);
	return s + msg;
}

// 모두에게 보낸다 (keep 이면 최근 대화에도, 대화 기록에도 남김)
void broadcast(const std::string &msg, bool keep)
{
	std::string stamped = stamp(msg);
	for (unsigned int i=0; i<clients.size(); i++) {
		send_msg(clients[i].socket, (clients[i].time_on ? stamped : msg).c_str());
	}
	if ( keep ) {
		history.push_back(msg);
		while ( history.size() > HISTORY ) history.pop_front();
		log_line(msg);
	}
}

// 닉네임 표 (사람마다 색, 반전)
static std::string name_tag(const client &c)
{
	return std::string(name_colors[c.color % NAME_COLORS]) + "\033[7m" + c.nickname + "(" + c.userid + ")\033[0m" C_WHITE;
}

// 화면에 맞게 줄을 나눈다 (띄어쓰기에서 나누고 다음 줄은 두 칸 들여서).
// 색 코드가 없는 글에 쓴다. 완성형 한글은 두 바이트가 두 칸.
// 시각 표시 "[21:30] " (8 칸) 가 붙어도 80 칸을 넘지 않게 71 칸에서 나눈다.
std::string wrap79(const std::string &text)
{
	const unsigned int W = 71;
	std::string out, line;
	std::string::size_type i = 0;
	while ( i < text.size() ) {
		// 다음 낱말 (띄어쓰기 포함)
		std::string::size_type j = text.find(' ', i);
		std::string word = (j == std::string::npos) ? text.substr(i) : text.substr(i, j - i + 1);
		i = (j == std::string::npos) ? text.size() : j + 1;
		if ( !line.empty() && line.size() + rtrim(word).size() > W ) {
			out += rtrim(line) + "\r\n";
			line = "  ";
		}
		// 낱말 하나가 한 줄보다 길면 글자 단위로 자른다
		while ( line.size() + word.size() > W && word.size() > 1 ) {
			unsigned int k = 0, room = W - line.size();
			while ( k < word.size() ) {
				unsigned int w = ((unsigned char)word[k] >= 0x80 && k + 1 < word.size()) ? 2 : 1;
				if ( k + w > room ) break;
				k += w;
			}
			out += line + word.substr(0, k) + "\r\n";
			word = word.substr(k);
			line = "  ";
		}
		line += word;
	}
	return out + rtrim(line);
}

// 안내 (노란 글씨, 모두에게)
void notice(const std::string &text)
{
	broadcast("\r\n" C_YELLOW + wrap79(text) + C_WHITE "\r\n", true);
}

// 한 사람에게만
void tell(const client &c, const std::string &text)
{
	send_msg(c.socket, ("\r\n" C_GRAY + wrap79(text) + C_WHITE "\r\n").c_str());
}

client push_client(int socket, char *userid, char* nickname, char* ip, int port)
{
	client c;
	// 첫번째 접속자가 방 개설자이다. (늘 열린 방은 방장이 없다)
	c.author = (!permanent && clients.size() == 0);
	snprintf(c.userid, sizeof(c.userid), "%s", userid);
	snprintf(c.nickname, sizeof(c.nickname), "%s", nickname);
	snprintf(c.ip, sizeof(c.ip), "%s", ip);
	c.port = port;
	c.socket = socket;
	c.joined = time(NULL);
	c.last_ms = 0;
	c.repeat = 0;
	c.color = next_color++ % NAME_COLORS;
	c.time_on = false;
	clients.push_back(c);
	return c;
}

int pop_client(client c)
{
	std::vector<client> res;

	for(unsigned int i=0; i<clients.size(); i++) {
		client c2 = clients[i];
		if ( c.socket != c2.socket ) {
			res.push_back(c2);
		}
	}

	clients = res;
	close(c.socket);

	return 0;
}

void constr_func(client c2, client c)
{
	char buf1[MAX_LINE];

	memset(buf1, 0, sizeof(buf1));
	snprintf(buf1, sizeof(buf1), "\r\n\033[7m%s(%s) 님이 입장 하셨습니다.\033[0m\r\n", c.nickname, c.userid);

	send_msg(c2.socket, buf1);
}

void quit_func(client c)
{
	char buf1[MAX_LINE];

	memset(buf1, 0, sizeof(buf1));
	printf("%s is leaved at %s\r\n", c.userid, c.ip);

	snprintf(buf1, sizeof(buf1), "\r\n\033[7m%s(%s) 님이 퇴장 하셨습니다.\033[0m\r\n", c.nickname, c.userid);

	if ( c.author == true ) {
		for(unsigned int i=0; i<clients.size(); i++) {
			client c2 = clients[i];
			if (c.socket != c2.socket) {
				// 방장이 퇴장하면 다음 사용자에게 위임한다.
				clients[i].author = true;
				// 같은 버퍼를 원본/대상으로 쓰지 않도록 별도 버퍼 사용
				char buf2[MAX_LINE];
				snprintf(buf2, sizeof(buf2), "%s\033[7m%s(%s) 님이 방장을 위임받았습니다.\033[0m\r\n",
					buf1, c2.nickname, c2.userid);
				strcpy(buf1, buf2);
				break;
			}
		}
	}
	log_line(buf1);

	for(unsigned int i=0; i<clients.size(); i++) {
		client c2 = clients[i];
		if (c.socket != c2.socket) {
			send_msg(c2.socket, buf1);
		}
	}
}

// /LIST : 닉네임(아이디), 방장, 들어온 지 몇 분째
void list_func(client c)
{
	char buf1[MAX_LINE];

	snprintf(buf1, sizeof(buf1), "\r\n\033[7m대화방 접속인원은 %d명 입니다.\033[0m\r\n", (int)clients.size());
	send_msg(c.socket, buf1);

	for(unsigned int i=0; i<clients.size(); i++) {
		client c2 = clients[i];
		long min = (time(NULL) - c2.joined) / 60;
		// 메세지마다 앞의 \r\n 으로 새 줄을 연다 (없으면 클라이언트가 앞 줄을 덮어쓴다)
		snprintf(buf1, sizeof(buf1), "\r\n  %s(%s)%s  " C_GRAY "%s" C_WHITE "\r\n", c2.nickname, c2.userid,
				c2.author ? C_YELLOW "  방장" C_WHITE : "",
				min < 1 ? "방금 들어옴" : (itos(min) + "분째").c_str());
		send_msg(c.socket, buf1);
	}
}

int say_func(client from, client to, char *msg)
{
	// 클라이언트 수신 버퍼(1024)를 넘지 않도록 MAX_LINE 으로 제한
	char buf[MAX_LINE];
	snprintf(buf, sizeof(buf), "\r\n\033[7m!%s(%s)\033[0m : %s\r\n", from.nickname, from.userid, msg);

	// 귓속말 받을 회원에게 메세지 전달
	send_msg(to.socket, buf);

	// 귓속말 보낸 사람에게도 보냄
	send_msg(from.socket, buf);
	return 0;
}

// ------------------------------------------------------------------
// 도배 막기: 0.8 초 안에 또 말하거나, 같은 말을 세 번 연달아 하면 막는다
// ------------------------------------------------------------------
bool flood_check(client &c, const std::string &text)
{
	long now = now_ms();
	if ( now - c.last_ms < 800 ) {
		tell(c, "[알림] 너무 빨리 말하고 있습니다. 천천히 말해 주세요.");
		return false;
	}
	if ( text == c.last_text ) {
		if ( ++c.repeat >= 2 ) {
			tell(c, "[알림] 같은 말을 되풀이할 수 없습니다.");
			return false;
		}
	} else {
		c.repeat = 0;
	}
	c.last_ms = now;
	c.last_text = text;
	return true;
}

// ------------------------------------------------------------------
// 한글 (완성형 두 바이트 <-> 유니코드) : 끝말잇기 두음법칙
// ------------------------------------------------------------------
static int to_unicode(const std::string &syl)
{
	iconv_t cd = iconv_open("UCS-2BE", "CP949");
	if ( cd == (iconv_t)-1 ) return -1;
	char in[3] = { syl[0], syl[1], 0 }, out[4];
	char *pi = in, *po = out;
	size_t li = 2, lo = sizeof(out);
	size_t r = iconv(cd, &pi, &li, &po, &lo);
	iconv_close(cd);
	if ( r == (size_t)-1 || lo != 2 ) return -1;
	return ((unsigned char)out[0] << 8) | (unsigned char)out[1];
}

static std::string from_unicode(int code)
{
	iconv_t cd = iconv_open("CP949", "UCS-2BE");
	if ( cd == (iconv_t)-1 ) return "";
	char in[2] = { (char)(code >> 8), (char)(code & 0xff) }, out[8];
	char *pi = in, *po = out;
	size_t li = 2, lo = sizeof(out);
	size_t r = iconv(cd, &pi, &li, &po, &lo);
	iconv_close(cd);
	if ( r == (size_t)-1 ) return "";
	return std::string(out, sizeof(out) - lo);
}

// 한글 낱말 (2~8 글자, 띄어쓰기 없이 한글만)
static bool hangul_word(const std::string &w)
{
	if ( w.size() < 4 || w.size() > 16 || w.size() % 2 ) return false;
	for ( unsigned int i = 0; i < w.size(); i += 2 ) {
		int u = to_unicode(w.substr(i, 2));
		if ( u < 0xAC00 || u > 0xD7A3 ) return false;
	}
	return true;
}

// 두음법칙으로 바꾼 글자 (예: 력 -> 역, 녀 -> 여, 라 -> 나). 없으면 ""
static std::string dueum(const std::string &syl)
{
	int u = to_unicode(syl);
	if ( u < 0xAC00 || u > 0xD7A3 ) return "";
	int s = u - 0xAC00, l = s / 588, v = (s % 588) / 28, t = s % 28;
	bool iy = (v == 2 || v == 6 || v == 7 || v == 12 || v == 17 || v == 20);	// ㅑㅕㅖㅛㅠㅣ
	int nl = -1;
	if ( l == 5 ) nl = iy ? 11 : 2;				// ㄹ -> ㅇ / ㄴ
	else if ( l == 2 && (v == 6 || v == 12 || v == 17 || v == 20) ) nl = 11;	// ㄴ -> ㅇ
	if ( nl < 0 ) return "";
	return from_unicode(0xAC00 + (nl * 21 + v) * 28 + t);
}

// ------------------------------------------------------------------
// 놀이: 끝말잇기, 퀴즈
// ------------------------------------------------------------------
enum { G_NONE, G_WORD, G_QUIZ };
static int game = G_NONE;
static time_t game_deadline = 0;
static std::map<std::string, int> scores;			// 아이디 -> 점수
static std::map<std::string, std::string> score_names;

static std::string word_last, word_last_user;
static std::vector<std::string> word_used;

static const char *start_words[] = {
	"사과", "기차", "하늘", "바다", "컴퓨터", "모뎀", "통신", "나무", "자전거", "고양이",
	"우산", "편지", "도서관", "운동장", "라디오", "무지개", "소나기", "연필", "책상", "거울", NULL
};

struct quiz { const char *q, *a; };
static const quiz quizzes[] = {
	{ "우리나라의 수도는?", "서울" },
	{ "한글을 만든 임금은?", "세종대왕|세종" },
	{ "하이텔, 천리안, 나우누리처럼 전화선으로 접속하던 서비스를 무엇이라 했나요?", "PC통신|피씨통신" },
	{ "1 바이트는 몇 비트?", "8" },
	{ "MS-DOS 에서 화면을 지우는 명령은?", "CLS" },
	{ "MS-DOS 에서 디렉터리 목록을 보는 명령은?", "DIR" },
	{ "독도는 어느 바다에 있나요?", "동해" },
	{ "대한민국에서 가장 높은 산은?", "한라산" },
	{ "물이 끓는 온도는 섭씨 몇 도?", "100" },
	{ "무지개는 몇 가지 색?", "7|일곱" },
	{ "태양계에서 가장 큰 행성은?", "목성" },
	{ "지구에서 가장 가까운 별(항성)은?", "태양" },
	{ "임진왜란 때 거북선으로 싸운 장군은?", "이순신" },
	{ "광복절은 몇 월 며칠?", "8월15일|8월 15일|815" },
	{ "한글날은 몇 월 며칠?", "10월9일|10월 9일|109" },
	{ "1 킬로바이트는 몇 바이트? (옛날 기준)", "1024" },
	{ "모뎀 속도 단위로, 초당 비트 수를 뜻하는 말은?", "bps|BPS" },
	{ "자료를 받을 때 쓰던 Z모뎀, Y모뎀, X모뎀 중 가장 빠른 것은?", "Z모뎀|z모뎀|zmodem" },
	{ "윈도우 95 가 나온 해는?", "1995" },
	{ "사람의 뼈는 대략 몇 개? (성인)", "206" },
	{ "바나나의 색은? (익었을 때)", "노란색|노랑|노랑색" },
	{ "한 시간은 몇 분?", "60" },
	{ "일주일은 며칠?", "7|칠" },
	{ "남극에 사는, 날지 못하는 새는?", "펭귄" },
	{ "판소리 '춘향가'의 주인공 춘향의 연인은?", "이몽룡" },
	{ "세계에서 가장 긴 강은? (보통 알려진 답)", "나일강|나일" },
	{ "오목에서 이기려면 돌 몇 개를 나란히 놓아야 할까요?", "5|다섯" },
	{ "86, 386, 486, 펜티엄... 이것들은 무엇의 이름?", "CPU|씨피유|중앙처리장치" },
	{ "플로피 디스켓 3.5인치의 보통 용량은? (MB)", "1.44|1.44MB" },
	{ "대한민국 국기의 이름은?", "태극기" },
	{ "4 곱하기 8 은?", "32" },
	{ "하루는 몇 시간?", "24" },
	{ "가장 작은 소수(素數)는?", "2" },
	{ "지구의 위성은?", "달" },
	{ "물의 화학식은?", "H2O|h2o" },
	{ "피아노 건반은 모두 몇 개?", "88" },
	{ NULL, NULL }
};
static int quiz_count(void) { int n = 0; while ( quizzes[n].q ) n++; return n; }

#define WORD_SEC	30
#define QUIZ_SEC	30
#define QUIZ_ROUNDS	5
static int quiz_now = -1, quiz_round = 0;
static bool quiz_hint = false;
static time_t quiz_started = 0, quiz_next_at = 0;
static std::vector<int> quiz_asked;

static void add_score(const client &c)
{
	scores[c.userid]++;
	score_names[c.userid] = c.nickname;
}

static std::string score_text(void)
{
	// 점수 높은 순
	std::vector<std::pair<int, std::string> > v;
	for ( std::map<std::string, int>::iterator it = scores.begin(); it != scores.end(); ++it ) {
		v.push_back(std::make_pair(-it->second, it->first));
	}
	std::sort(v.begin(), v.end());
	std::string s;
	for ( unsigned int i = 0; i < v.size() && i < 5; i++ ) {
		if ( !s.empty() ) s += ", ";
		s += score_names[v[i].second] + " " + itos(-v[i].first) + "점";
	}
	return s.empty() ? "점수를 얻은 사람이 없습니다." : s;
}

static void end_game(const std::string &why)
{
	const char *name = (game == G_WORD) ? "끝말잇기" : "퀴즈";
	notice(std::string("[") + name + "] " + why + " 결과: " + score_text());
	game = G_NONE;
	scores.clear();
	score_names.clear();
}

static std::string next_syllable(void)
{
	std::string last = word_last.substr(word_last.size() - 2);
	std::string d = dueum(last);
	return d.empty() ? "'" + last + "'" : "'" + last + "' 또는 '" + d + "'";
}

static void start_word(const client &c)
{
	int n = 0;
	while ( start_words[n] ) n++;
	game = G_WORD;
	scores.clear();
	score_names.clear();
	word_last = start_words[rand() % n];
	word_last_user = "";
	word_used.clear();
	word_used.push_back(word_last);
	game_deadline = time(NULL) + WORD_SEC;
	notice(std::string("[끝말잇기] ") + c.nickname + " 님이 시작했습니다. 첫 낱말은 '" + word_last + "'!");
	notice("[끝말잇기] " + next_syllable() + "(으)로 시작하는 낱말을 30초 안에 말하세요. 두음법칙 됨, 그만두려면 /끝말잇기 그만");
}

// 끝말잇기 중에 한 말. 낱말이면 처리
static void word_answer(client &c, const std::string &text)
{
	if ( !hangul_word(text) ) return;
	std::string first = text.substr(0, 2), last = word_last.substr(word_last.size() - 2);
	if ( first != last && first != dueum(last) ) return;
	if ( std::find(word_used.begin(), word_used.end(), text) != word_used.end() ) {
		notice("[끝말잇기] '" + text + "' 은(는) 이미 나온 낱말입니다.");
		return;
	}
	if ( word_last_user == c.userid ) {
		tell(c, "[끝말잇기] 연달아 이을 수는 없습니다. 다른 사람 차례를 기다리세요.");
		return;
	}
	word_used.push_back(text);
	word_last = text;
	word_last_user = c.userid;
	add_score(c);
	game_deadline = time(NULL) + WORD_SEC;
	notice(std::string("[끝말잇기] ") + c.nickname + " +1  다음은 " + next_syllable());
}

static void quiz_ask(void)
{
	int n = quiz_count();
	int q;
	do { q = rand() % n; } while ( (int)quiz_asked.size() < n && std::find(quiz_asked.begin(), quiz_asked.end(), q) != quiz_asked.end() );
	quiz_asked.push_back(q);
	quiz_now = q;
	quiz_round++;
	quiz_hint = false;
	quiz_started = time(NULL);
	quiz_next_at = 0;
	notice("[퀴즈 " + itos(quiz_round) + "/" + itos(QUIZ_ROUNDS) + "] " + quizzes[q].q + " (30초)");
}

static void start_quiz(const client &c)
{
	game = G_QUIZ;
	scores.clear();
	score_names.clear();
	quiz_round = 0;
	quiz_asked.clear();
	notice(std::string("[퀴즈] ") + c.nickname + " 님이 퀴즈를 시작했습니다. 먼저 맞히는 사람이 1점! (/퀴즈 그만)");
	quiz_ask();
}

// 띄어쓰기를 빼고 비교
static std::string squeeze(const std::string &s)
{
	std::string r;
	for ( unsigned int i = 0; i < s.size(); i++ ) {
		if ( s[i] != ' ' ) r += (s[i] >= 'A' && s[i] <= 'Z') ? s[i] - 'A' + 'a' : s[i];
	}
	return r;
}

static void quiz_answer(client &c, const std::string &text)
{
	if ( quiz_now < 0 || quiz_next_at ) return;
	std::vector<std::string> answers = split_string(quizzes[quiz_now].a, '|');
	for ( unsigned int i = 0; i < answers.size(); i++ ) {
		if ( squeeze(text) == squeeze(answers[i]) ) {
			add_score(c);
			notice(std::string("[퀴즈] 정답! ") + c.nickname + " +1  (정답: " + answers[0] + ")");
			quiz_now = -1;
			quiz_next_at = time(NULL) + 3;
			return;
		}
	}
}

// 1 초마다: 시간이 다 됐는지
static void game_tick(void)
{
	time_t now = time(NULL);
	if ( game == G_WORD && now > game_deadline ) {
		end_game("30초 동안 아무도 잇지 못해 끝났습니다.");
	} else if ( game == G_QUIZ ) {
		if ( quiz_next_at ) {
			if ( now >= quiz_next_at ) {
				if ( quiz_round >= QUIZ_ROUNDS ) end_game("다섯 문제가 끝났습니다.");
				else quiz_ask();
			}
		} else if ( quiz_now >= 0 ) {
			std::string a = split_string(quizzes[quiz_now].a, '|')[0];
			if ( !quiz_hint && now - quiz_started >= QUIZ_SEC / 2 ) {
				quiz_hint = true;
				std::string first = ((unsigned char)a[0] >= 0x80 && a.size() >= 2) ? a.substr(0, 2) : a.substr(0, 1);
				notice("[퀴즈] 힌트: '" + first + "' (으)로 시작합니다.");
			} else if ( now - quiz_started >= QUIZ_SEC ) {
				notice("[퀴즈] 아무도 못 맞혔습니다. 정답은 '" + a + "'");
				quiz_now = -1;
				quiz_next_at = now + 3;
			}
		}
	}
}

static void help_func(const client &c)
{
	const char *lines[] = {
		"── 대화방 명령 ──",
		"/SAY 아이디 말       귓속말",
		"/LIST                들어와 있는 사람",
		"/ME 행동             예) /ME 웃는다  -> * 하늘소 님이 웃는다",
		"/주사위 [N]          주사위 (1~6, 또는 1~N)   /DICE",
		"/끝말잇기            끝말잇기 시작 (/끝말잇기 그만)   /WORD",
		"/퀴즈                퀴즈 다섯 문제 (/퀴즈 그만)   /QUIZ",
		"/INVITE 아이디       대화방 밖의 접속자에게 이 방으로 오라는 전보",
		"/TIME                말한 시각 표시 켜기/끄기",
		"/BYE                 나가기",
		"── 방장만 ──",
		"/TITLE 주제          방 주제 바꾸기",
		"/KICK 아이디         내보내기 (10분 동안 다시 못 들어옴)",
		"/MAX 인원            허용 인원 바꾸기 (2~50)",
		"/OWNER 아이디        방장 넘기기",
		NULL
	};
	std::string s = "\r\n";
	for ( int i = 0; lines[i]; i++ ) s += std::string(C_GRAY) + lines[i] + C_WHITE "\r\n";
	send_msg(c.socket, s.c_str());
}

static client *find_client(const std::string &id)
{
	for ( unsigned int i = 0; i < clients.size(); i++ ) {
		if ( id == clients[i].userid ) return &clients[i];
	}
	return NULL;
}

void remove_client(client c);

// 방장 명령. 처리했으면 true
static bool owner_command(client &c, const std::string &cmd, const std::string &rest)
{
	if ( cmd != "/title" && cmd != "/kick" && cmd != "/max" && cmd != "/owner" ) return false;
	if ( permanent ) {
		tell(c, "[알림] 만남의 광장에는 방장이 없어서 이 명령을 쓸 수 없습니다.");
		return true;
	}
	if ( !c.author ) {
		tell(c, "[알림] 방장만 쓸 수 있는 명령입니다.");
		return true;
	}
	if ( cmd == "/title" ) {
		std::string t = rest;
		if ( t.empty() ) { tell(c, "[알림] /TITLE 새 주제"); return true; }
		// 40 바이트까지 (한글이 반으로 잘리지 않게)
		unsigned int k = 0;
		while ( k < t.size() ) {
			unsigned int w = ((unsigned char)t[k] >= 0x80 && k + 1 < t.size()) ? 2 : 1;
			if ( k + w > 40 ) break;
			k += w;
		}
		room_title = t.substr(0, k);
		write_room_info();
		notice(std::string("[알림] 방장 ") + c.nickname + " 님이 주제를 '" + room_title + "'(으)로 바꾸었습니다.");
	} else if ( cmd == "/max" ) {
		int n = atoi(rest.c_str());
		if ( n < 2 || n > 50 ) { tell(c, "[알림] /MAX 인원 (2~50)"); return true; }
		if ( n < (int)clients.size() ) { tell(c, "[알림] 지금 들어와 있는 사람보다 적게 할 수 없습니다."); return true; }
		max_user = n;
		write_room_info();
		notice("[알림] 허용 인원을 " + itos(n) + "명으로 바꾸었습니다.");
	} else {
		client *t = find_client(rest);
		if ( rest.empty() || t == NULL ) { tell(c, "[알림] 이 방에 '" + rest + "' 아이디가 없습니다. (/LIST)"); return true; }
		if ( t->socket == c.socket ) { tell(c, "[알림] 자기 자신에게는 쓸 수 없습니다."); return true; }
		if ( cmd == "/owner" ) {
			c.author = false;
			for ( unsigned int i = 0; i < clients.size(); i++ ) {
				if ( clients[i].socket == c.socket ) clients[i].author = false;
			}
			t->author = true;
			write_room_info();
			notice(std::string("[알림] ") + c.nickname + " 님이 " + t->nickname + " 님에게 방장을 넘겼습니다.");
		} else {
			client kicked = *t;
			banned[kicked.userid] = time(NULL) + 600;
			send_msg(kicked.socket, "\r\n" C_YELLOW "[알림] 방장이 이 방에서 내보냈습니다. 10분 동안 다시 들어올 수 없습니다." C_WHITE "\r\n");
			send_msg(kicked.socket, "/quit");
			notice(std::string("[알림] 방장 ") + c.nickname + " 님이 " + kicked.nickname + " 님을 내보냈습니다.");
			remove_client(kicked);
		}
	}
	return true;
}

// '/' 로 시작하는 명령. 처리했으면 true
bool command(client &c, const std::string &line)
{
	std::vector<std::string> args = split_string(trim(line), ' ');
	if ( args.size() == 0 ) return false;
	std::string cmd = args[0];
	for ( unsigned int i = 0; i < cmd.size(); i++ ) if ( cmd[i] >= 'A' && cmd[i] <= 'Z' ) cmd[i] += 'a' - 'A';
	std::string rest;
	std::string::size_type sp = trim(line).find(' ');
	if ( sp != std::string::npos ) rest = trim(strip_control(trim(line).substr(sp + 1)));

	if ( owner_command(c, cmd, rest) ) {
		return true;
	} else if ( cmd == "/help" || cmd == "/도움말" ) {
		help_func(c);
	} else if ( cmd == "/time" || cmd == "/시각" ) {
		c.time_on = !c.time_on;
		tell(c, c.time_on ? "[알림] 말한 시각을 보여 줍니다. (/TIME 으로 끄기)" : "[알림] 말한 시각을 보여 주지 않습니다.");
	} else if ( cmd == "/me" ) {
		if ( rest.empty() || !flood_check(c, line) ) return true;
		broadcast("\r\n" C_MAGENTA + wrap79(std::string("* ") + c.nickname + " 님이 " + rest) + C_WHITE "\r\n", true);
	} else if ( cmd == "/dice" || cmd == "/주사위" ) {
		if ( !flood_check(c, line) ) return true;
		int n = atoi(rest.c_str());
		if ( n < 2 || n > 1000 ) n = 6;
		notice(std::string("[주사위] ") + c.nickname + " 님이 주사위(1~" + itos(n) + ")를 굴려 " + itos(1 + rand() % n) + " 이(가) 나왔습니다.");
	} else if ( cmd == "/word" || cmd == "/끝말잇기" ) {
		if ( rest == "그만" || rest == "stop" ) {
			if ( game == G_WORD ) end_game(std::string(c.nickname) + " 님이 그만두었습니다.");
		} else if ( game != G_NONE ) {
			tell(c, "[알림] 지금 다른 놀이를 하고 있습니다.");
		} else {
			start_word(c);
		}
	} else if ( cmd == "/quiz" || cmd == "/퀴즈" ) {
		if ( rest == "그만" || rest == "stop" ) {
			if ( game == G_QUIZ ) end_game(std::string(c.nickname) + " 님이 그만두었습니다.");
		} else if ( game != G_NONE ) {
			tell(c, "[알림] 지금 다른 놀이를 하고 있습니다.");
		} else {
			start_quiz(c);
		}
	} else {
		tell(c, "[알림] 모르는 명령입니다. /HELP 로 명령을 볼 수 있습니다.");
	}
	return true;
}

// len 바이트를 모두 읽는다. EOF/에러(타임아웃 포함)면 -1
int read_full(int socket, char *buf, int len)
{
	int got = 0;
	while (got < len) {
		int n = read(socket, buf+got, len-got);
		if (n < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		// 상대방 연결 종료
		if (n == 0) return -1;
		got += n;
	}
	return got;
}

// len 바이트를 모두 쓴다. 에러면 -1
int write_full(int socket, const char *buf, int len)
{
	int done = 0;
	while (done < len) {
		int n = write(socket, buf+done, len-done);
		if (n < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		if (n == 0) return -1;
		done += n;
	}
	return done;
}

// 메세지 수신. 연결 종료/에러/잘못된 길이면 -1
int recv_msg(int socket, char *msg, int bufsize)
{
	int size;
	msg[0] = '\0';

	if (read_full(socket, (char*)&size, sizeof(int)) < 0) return -1;

	// 상대가 보낸 길이를 그대로 믿지 않는다
	if (size < 0 || size >= bufsize) return -1;

	if (size > 0 && read_full(socket, msg, size) < 0) return -1;
	msg[size] = '\0';

	return size;
}

int send_msg(int socket, const char *msg)
{
	int size = strlen(msg);
	if (write_full(socket, (const char*)&size, sizeof(int)) < 0) return -1;
	if (write_full(socket, msg, size) < 0) return -1;
	return 0;
}

int server_close (void)
{
	for (unsigned int i=0; i<clients.size(); i++) {
		client c = clients[i];

		send_msg(c.socket, (char*)"/quit");
		usleep(1000*500);
	}

	close(server_fd);

	char buf[1024];
	sprintf(buf, "%s/chatt/%d.room", getenv("HANULSO"), server_port);
	unlink(buf);

	sleep(1);

	exit(0);
}

// 대화방 정보 파일 업데이트
void write_room_info()
{
	char buf[1024];
	sprintf(buf, "%s/chatt/%d.room", getenv("HANULSO"), server_port);

	std::string author;
	for (unsigned int i=0; i<clients.size(); i++) {
		client c = clients[i];
		if ( c.author == true ) {
			author = c.userid;
		}
	}

	FILE *fp = fopen(buf, "w");
	if (fp == NULL) return;
	// 방장,접속인원수
	// 방장,접속인원수,허용인원 (그리고 /TITLE 로 바꾼 주제가 있으면 다음 줄에)
	fprintf(fp, "%s,%d,%d", author.c_str(), (int)clients.size(), max_user);
	if ( !room_title.empty() ) fprintf(fp, "\n%s", room_title.c_str());
	fclose(fp);
}

// 접속자 퇴장 처리 (/bye 또는 연결 끊김)
void remove_client(client c)
{
	quit_func(c);
	pop_client(c);

	// 대화방 접속 인원수 파일 업데이트
	write_room_info();

	// 대화방에 아무도 없으면 방 종료 (늘 열린 방은 그대로)
	if ( clients.size() == 0 ) {
		if ( !permanent ) server_close();
		if ( game != G_NONE ) { game = G_NONE; scores.clear(); score_names.clear(); }
	}
}

// 새 접속자 처리
void accept_client(void)
{
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	int client_fd = accept(server_fd,(struct sockaddr *)&client_addr, &client_len);
	if (client_fd < 0) return;

	// select() 로 감시할 수 없는 fd 는 받지 않는다
	if (client_fd >= FD_SETSIZE) {
		close(client_fd);
		return;
	}

	// userid 를 보내지 않는 클라이언트가 대화방 전체를 멈추지 않도록 수신 타임아웃(5초)
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	char userid[256];
	char nickname[256];
	char ip[256];
	int port;

	memset(userid, 0, sizeof(userid));
	memset(nickname, 0, sizeof(nickname));
	memset(ip, 0, sizeof(ip));

	// 접속 사용자ID, 닉네임 받음
	if (recv_msg(client_fd, userid, sizeof(userid)) < 0 ||
		recv_msg(client_fd, nickname, sizeof(nickname)) < 0) {
		close(client_fd);
		return;
	}
	strcpy(userid, strip_control(userid).c_str());
	strcpy(nickname, strip_control(nickname).c_str());

	// 방장이 내보낸 사람은 10 분 동안 못 들어온다
	std::map<std::string, time_t>::iterator ban = banned.find(userid);
	if (ban != banned.end()) {
		if (time(NULL) < ban->second) {
			send_msg(client_fd, "\r\n\033[7m방장이 내보내서 잠시 이 방에 들어올 수 없습니다.\033[0m\r\n");
			send_msg(client_fd, "/quit");
			close(client_fd);
			return;
		}
		banned.erase(ban);
	}

	// 허용 인원 초과
	if (max_user > 0 && (int)clients.size() >= max_user) {
		send_msg(client_fd, "\r\n\033[7m대화방 허용 인원이 꽉 찼습니다.\033[0m\r\n");
		close(client_fd);
		return;
	}

	// 접속 사용자 IP 주소
	inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
	// 접속 사용자 포트 번호
	port = ntohs(client_addr.sin_port);

	// 클라이언트 추가
	client c = push_client(client_fd, userid, nickname, ip, port);
	printf("%s is connected from %s\r\n", c.userid, c.ip);

	// 같은 아이디로 이미 들어와 있으면 (다른 창, 끊긴 줄 모르고 남은 접속) 예전 접속을 끊는다
	for (int i=0; i<(int)clients.size(); i++) {
		client old = clients[i];
		if (old.socket != c.socket && !strcmp(old.userid, c.userid)) {
			send_msg(old.socket, "\r\n" C_YELLOW "[알림] 같은 아이디로 다른 곳에서 들어와서 이 접속은 끊습니다." C_WHITE "\r\n");
			send_msg(old.socket, "/quit");
			remove_client(old);
			i--;
		}
	}

	// 대화방 접속 인원수 파일 업데이트
	write_room_info();

	// 접속자에게 환영 메세지 보내기
	send_msg(client_fd, greeting);

	// 최근 대화 (무슨 얘기 중이었는지)
	if ( history.size() > 0 ) {
		// 남겨 둔 메세지는 앞뒤에 \r\n 이 있으므로 끝의 \r\n 을 떼고 이어 붙인다 (빈 줄이 끼지 않게)
		std::string h = "\r\n" C_GRAY "─── 최근 대화 ───" C_WHITE;
		for ( unsigned int i = 0; i < history.size(); i++ ) {
			std::string m = history[i];
			while ( m.size() >= 2 && m.compare(m.size() - 2, 2, "\r\n") == 0 ) m.erase(m.size() - 2);
			h += m;
		}
		h += "\r\n" C_GRAY "─────────────────" C_WHITE "\r\n";
		send_msg(client_fd, h.c_str());
	}
	if ( game == G_WORD ) tell(c, "[끝말잇기] 하는 중입니다. 다음은 " + next_syllable());
	else if ( game == G_QUIZ && quiz_now >= 0 ) tell(c, std::string("[퀴즈] 하는 중입니다. 문제: ") + quizzes[quiz_now].q);

	// 다른 접속자들에게 접속 사실을 알림
	for (unsigned int i=0; i<clients.size(); i++) {
		client c2 = clients[i];
		if (c2.socket != c.socket) {
			constr_func(c2, c);
		}
	}
	log_line(std::string(c.nickname) + "(" + c.userid + ") 님이 입장 하셨습니다.");
}

int main(int argc,char *argv[])
{
	struct sockaddr_in server_addr;

	int max_fd = 0;
	fd_set read_fds;

	if (argc < 4)
	{
		printf("## HANULSO BBS ##\n");
		printf("Chatting server program\n");
		printf("usage: %s port max_user greeting [permanent]\n",argv[0]);
		exit(-1);
	}

	server_port = atoi(argv[1]);
	max_user = atoi(argv[2]);
	snprintf(greeting, sizeof(greeting), "\r\n## \033[7m%s\033[0m\r\n" C_GRAY "   명령은 /HELP" C_WHITE "\r\n", argv[3]);
	permanent = (argc > 4 && !strcmp(argv[4], "permanent"));
	srand(time(NULL) ^ getpid());

    signal(SIGQUIT, (__sighandler_t)server_close);
    signal(SIGINT, (__sighandler_t)server_close);
    signal(SIGTERM, (__sighandler_t)server_close);
    signal(SIGHUP, (__sighandler_t)server_close);
    signal(SIGSEGV, (__sighandler_t)server_close);
    signal(SIGBUS, (__sighandler_t)server_close);
	// 끊어진 소켓에 write 시 프로세스가 죽지 않도록
    signal(SIGPIPE, SIG_IGN);

	server_fd=socket(AF_INET,SOCK_STREAM,0);
	memset(&server_addr,0,sizeof(server_addr));
	// chattclient 는 127.0.0.1 로만 접속한다
	server_addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
	server_addr.sin_family=AF_INET;
	server_addr.sin_port=htons(server_port);

	// prevent bind error
	int sockopt = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));

	if(bind(server_fd,(struct sockaddr *) &server_addr,sizeof(server_addr))==-1){
		printf("Can not Bind\n");
		return -1;
	}

	if(listen(server_fd, MAX_CLIENT)==-1){
		printf("listen Fail\n");
		return -1;
	}

	// 늘 열린 방은 아무도 없어도 목록에 0 명으로 보이게
	write_room_info();
	clean_logs();
	time_t last_clean = time(NULL);

	char buf1[MAX_LINE];

	memset(buf1, 0, sizeof(buf1));
	inet_ntop(AF_INET, &server_addr.sin_addr, buf1, sizeof(buf1));
	printf("[server address is %s : %d]\r\n", buf1, ntohs(server_addr.sin_port));

	for (;;)
	{
		FD_ZERO(&read_fds);
		FD_SET(server_fd, &read_fds);

		max_fd = server_fd;

		for (unsigned int i=0; i<clients.size(); i++) {
			client c = clients[i];

            //if valid socket descriptor then add to read list
            if (c.socket > 0)
                FD_SET(c.socket, &read_fds);

            //highest file descriptor number, need it for the select function
            if(c.socket > max_fd)
                max_fd = c.socket;
		}

		// 놀이 시간을 재려고 1 초마다 깨어난다
		struct timeval tick;
		tick.tv_sec = 1;
		tick.tv_usec = 0;
		int ready = select(max_fd+1, &read_fds, NULL, NULL, &tick);
		if (ready < 0)
		{
			if (errno == EINTR) continue;
			printf("Select error\n");
			exit(1);
		}

		game_tick();
		if (time(NULL) - last_clean > 86400) {
			clean_logs();
			last_clean = time(NULL);
		}
		if (ready == 0) continue;

		// 클라이언트 접속 감지
		if (FD_ISSET(server_fd, &read_fds))
		{
			accept_client();
		}

		// 루프 중 pop_client 로 원소가 지워지므로 int 인덱스를 쓰고 삭제 후 i-- 한다
		for (int i=0; i<(int)clients.size(); i++) {
			client c = clients[i];

			if (FD_ISSET(c.socket, &read_fds)) {
				char line[MAX_LINE];
				memset(line, 0, sizeof(line));

				int n = recv_msg(c.socket, line, sizeof(line));

				// 연결 끊김 또는 잘못된 메세지
				if (n < 0) {
					remove_client(c);
					i--;
					continue;
				}

				if (n > 0) {
					std::vector<std::string> args = split_string(trim(line), ' ');

					// 빈 줄
					if (args.size() == 0) continue;

					// 접속자 로그아웃
					if (!strcasecmp(args[0].c_str(), "/bye")) {
						remove_client(c);
						i--;
						continue;
					}

					// 방장으로부터 방폭파 메세지가 왔을때
					if (!strcasecmp(args[0].c_str(), "/quit")) {
						printf("destroy requested from %s\n", c.userid);

						if ( permanent ) {
							tell(c, "[알림] 늘 열려 있는 방은 닫을 수 없습니다. 나가려면 /BYE");
						} else if ( c.author == true ) {
							// 방장인지 아닌지 검사
							// 모든 접속자에게 메세지 전달
							for(unsigned int j=0; j<clients.size(); j++) {
								client c2 = clients[j];
								char msg[MAX_LINE];
								snprintf(msg, sizeof(msg), "\r\n\033[7m%s(%s) 님으로부터 대화방이 종료 되었습니다.\033[0m\r\n",
										c.nickname, c.userid);
								send_msg(c2.socket, msg);
							}

							server_close();

						} else {
							char msg[MAX_LINE];
							sprintf(msg, "\r\n\033[7m방장만 대화방을 종료 가능합니다.\033[0m\r\n");
							send_msg(c.socket, msg);
						}

						continue;
					}

					// 요청 접속자에게 접속자 목록 전달
					if (!strcasecmp(args[0].c_str(), "/list")) {
						list_func(c);
						continue;
					}

					// 특정 접속자에게 개인 메세지 전달
					if (!strcasecmp(args[0].c_str(), "/say")) {
						/*
						 /say olddos 안녕 모두~
						*/
						if (args.size() < 2) continue;

						char say[1024];
						memset(say, 0, sizeof(say));

						// '안녕 모두~' 를 찾는다.
						int space_count=0;
						for(unsigned int j=0; j<strlen(line); j++) {
							if ( line[j] == ' ' ) space_count++;
							if ( space_count == 2 ) {
								snprintf(say, sizeof(say), "%s", strip_control(trim(line+j)).c_str());
								break;
							}
						}

						for(unsigned int j=0; j<clients.size(); j++) {
							client c2 = clients[j];
							if ( !strcmp(c2.userid, args[1].c_str()) ) {
								say_func(c, c2, say);
							}
						}

						continue;
					}

					// 그 밖의 명령 (/ME, /주사위, /끝말잇기, /퀴즈, /HELP ...)
					if (line[0] == '/') {
						command(clients[i], line);
						continue;
					}

					// 클라이언트가 만든 "닉네임(ID)" 머리말은 버리고 서버가 다시 만든다.
					// (닉네임 위장 및 ANSI 코드 삽입 방지)
					std::string prefix = std::string("\r\n\033[7m") + c.nickname + "(" + c.userid + ")\033[0m ";
					std::string body = line;
					if (body.compare(0, prefix.size(), prefix) == 0) {
						body = body.substr(prefix.size());
					}
					std::string text = trim(strip_control(body));
					if (text.empty()) continue;

					// 도배 막기
					if (!flood_check(clients[i], text)) continue;

					// 사람마다 다른 색의 닉네임 + 말
					std::string out = "\r\n" + name_tag(c) + " " + text + "\r\n";

					// 모든 접속자에게 메세지 전달 (최근 대화에도 남김)
					broadcast(out, true);

					// 놀이 중이면 답인지 본다
					if (game == G_WORD) word_answer(clients[i], text);
					else if (game == G_QUIZ) quiz_answer(clients[i], text);
				}
			}
		}
	}

	return 0;
}

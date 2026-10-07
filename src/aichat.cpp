#include "main.h"

// ------------------------------------------------------------------
// AI 와 이야기 : 인터넷의 AI 와 한 줄씩 묻고 답한다
//   bin/aichat <호스트이름> <아이디> <tty>
//
// OpenAI 방식 (POST .../chat/completions 꼴) 이면 어느 곳이든 쓸 수 있다.
// 기본은 구글 Gemini (무료 키는 https://aistudio.google.com 의 Get API key).
// hanulso.cfg 에 키를 넣는다 (key 말고는 빼도 된다):
//   <ai>
//     <key>AIza...</key>       <!-- Authorization: Bearer 로 보낸다 -->
//     <url>https://generativelanguage.googleapis.com/v1beta/openai/chat/completions</url>
//     <model>gemini-2.5-flash</model>
//     <daily>50</daily>        <!-- 한 사람이 하루에 물을 수 있는 수, 0 이면 제한 없음 -->
//   </ai>
//
// 대화 내용은 저장하지 않는다. 하루 물은 수만 $HANULSO/data/aichat/<아이디> 에 둔다.
// ------------------------------------------------------------------

struct termio sys_term;

char title[1024] = "AI 와 이야기";

char tty[10];

char host_name[256];

char user_id[64];

#define A_W		"\033[=15F"
#define A_Y		"\033[=14F"
#define A_C		"\033[=11F"
#define A_G		"\033[=7F"
#define A_R		"\033[=12F"
#define A_GR	"\033[=10F"

static std::string ai_url = "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions";
static std::string ai_key;
static std::string ai_model = "gemini-2.5-flash";
static int ai_daily = 50;

#define KEEP_MESSAGES	8		// AI 에게 함께 보내는 지난 대화 (묻고 답한 것 4 번)
#define ANSWER_WIDTH	74		// 답을 늘어놓는 폭 (앞에 5 칸 들여쓰기)
#define INPUT_LEN		70		// 한 번에 물을 수 있는 길이 (바이트)

struct chat_msg { std::string role, text; };	// text 는 UTF-8

static chat_msg make_msg(const std::string &role, const std::string &text)
{
	chat_msg m;
	m.role = role;
	m.text = text;
	return m;
}

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

// ------------------------------------------------------------------
// 설정, 하루 물은 수
// ------------------------------------------------------------------
static std::string hanulso_dir(void)
{
	return getenv("HANULSO") ? getenv("HANULSO") : ".";
}

static void read_ai_settings(void)
{
	pugi::xml_document doc;
	if ( !doc.load_file((hanulso_dir() + "/hanulso.cfg").c_str()) ) return;
	pugi::xml_node root = doc.first_element_by_path("/hanulso");
	snprintf(host_name, sizeof(host_name), "%s", root.child("name").child_value());
	pugi::xml_node ai = root.child("ai");
	if ( ai.empty() ) return;
	std::string v;
	v = trim(ai.child("url").child_value());	if ( !v.empty() ) ai_url = v;
	v = trim(ai.child("key").child_value());	if ( !v.empty() ) ai_key = v;
	v = trim(ai.child("model").child_value());	if ( !v.empty() ) ai_model = v;
	v = trim(ai.child("daily").child_value());	if ( !v.empty() ) ai_daily = atoi(v.c_str());
}

static std::string today_str(void)
{
	time_t t = time(NULL);
	struct tm tm;
	localtime_r(&t, &tm);
	char b[16];
	snprintf(b, sizeof(b), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	return b;
}

static std::string count_path(void)
{
	return hanulso_dir() + "/data/aichat/" + user_id;
}

// 오늘 물은 수
static int used_today(void)
{
	char day[16] = "";
	int n = 0;
	FILE *fp = fopen(count_path().c_str(), "r");
	if ( fp == NULL ) return 0;
	if ( fscanf(fp, "%15s %d", day, &n) != 2 ) n = 0;
	fclose(fp);
	return today_str() == day ? n : 0;
}

static void set_used_today(int n)
{
	mkdir((hanulso_dir() + "/data").c_str(), 0755);
	mkdir((hanulso_dir() + "/data/aichat").c_str(), 0755);
	FILE *fp = fopen(count_path().c_str(), "w");
	if ( fp == NULL ) return;
	fprintf(fp, "%s %d\n", today_str().c_str(), n);
	fclose(fp);
}

// ------------------------------------------------------------------
// 글자 바꾸기
// ------------------------------------------------------------------

// from -> to. 바꿀 수 없는 글자 (그림 글자 따위) 는 버린다
static std::string convert(const std::string &in, const char *to, const char *from)
{
	iconv_t cd = iconv_open(to, from);
	if ( cd == (iconv_t)-1 ) return in;
	std::string out;
	char *src = (char*)in.data();
	size_t left = in.size();
	char buf[4096];
	while ( left > 0 ) {
		char *dst = buf;
		size_t room = sizeof(buf);
		size_t r = iconv(cd, &src, &left, &dst, &room);
		out.append(buf, dst - buf);
		if ( r == (size_t)-1 && errno != E2BIG ) { src++; left--; }
	}
	iconv_close(cd);
	return out;
}

static std::string to_utf8(const std::string &s) { return convert(s, "UTF-8", "CP949"); }

static void put_utf8(std::string &out, unsigned int cp)
{
	if ( cp < 0x80 ) out += (char)cp;
	else if ( cp < 0x800 ) { out += (char)(0xC0 | cp >> 6); out += (char)(0x80 | (cp & 0x3F)); }
	else if ( cp < 0x10000 ) { out += (char)(0xE0 | cp >> 12); out += (char)(0x80 | (cp >> 6 & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
	else { out += (char)(0xF0 | cp >> 18); out += (char)(0x80 | (cp >> 12 & 0x3F)); out += (char)(0x80 | (cp >> 6 & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
}

// AI 의 답 (UTF-8) 을 화면에 맞게: 꾸밈 따옴표는 ASCII 로, 마크다운 기호는 빼고, 완성형으로
static std::string answer_to_cp949(const std::string &u)
{
	static const char *from[] = { "\xE2\x80\x98", "\xE2\x80\x99", "\xE2\x80\x9C", "\xE2\x80\x9D", "\xE2\x80\x93",
		"\xE2\x80\x94", "\xE2\x80\xA6", "\xC2\xA0", "\xE2\x80\xAF", "\xE2\x80\x8B", "\xEF\xB8\x8F", "**", "__", "`", NULL };
	static const char *to[] = { "'", "'", "\"", "\"", "-", "-", "...", " ", " ", "", "", "", "", "" };
	std::string r = u;
	for ( int i = 0; from[i]; i++ ) r = replace_all(r, from[i], to[i]);

	// 줄 앞의 "# 제목", "* 항목" 정리
	std::vector<std::string> lines = split_string(r, '\n');
	std::string out;
	for ( unsigned int i = 0; i < lines.size(); i++ ) {
		std::string l = lines[i];
		unsigned int k = 0;
		while ( k < l.size() && l[k] == ' ' ) k++;
		if ( k < l.size() && l[k] == '#' ) {
			while ( k < l.size() && (l[k] == '#' || l[k] == ' ') ) k++;
			l = l.substr(k);
		} else if ( k + 1 < l.size() && l[k] == '*' && l[k + 1] == ' ' ) {
			l = l.substr(0, k) + "- " + l.substr(k + 2);
		}
		if ( i ) out += "\n";
		out += l;
	}
	return convert(out, "CP949", "UTF-8");
}

// ------------------------------------------------------------------
// JSON
// ------------------------------------------------------------------
static std::string json_escape(const std::string &s)
{
	std::string r;
	for ( unsigned int i = 0; i < s.size(); i++ ) {
		unsigned char c = s[i];
		if ( c == '"' ) r += "\\\"";
		else if ( c == '\\' ) r += "\\\\";
		else if ( c == '\n' ) r += "\\n";
		else if ( c < 0x20 ) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); r += b; }
		else r += c;
	}
	return r;
}

// "key" : "..." 의 값 (UTF-8, 줄바꿈은 그대로). 없으면 false
static bool json_string(const std::string &j, const char *key, std::string &out)
{
	std::string k = std::string("\"") + key + "\"";
	std::string::size_type p = 0;
	while ( (p = j.find(k, p)) != std::string::npos ) {
		p += k.size();
		while ( p < j.size() && isspace((unsigned char)j[p]) ) p++;
		if ( p >= j.size() || j[p] != ':' ) continue;
		p++;
		while ( p < j.size() && isspace((unsigned char)j[p]) ) p++;
		if ( p >= j.size() || j[p] != '"' ) continue;
		p++;
		out.clear();
		while ( p < j.size() && j[p] != '"' ) {
			char c = j[p++];
			if ( c != '\\' || p >= j.size() ) { out += c; continue; }
			char e = j[p++];
			if ( e == 'n' ) out += '\n';
			else if ( e == 't' ) out += ' ';
			else if ( e == 'r' ) ;
			else if ( e == 'u' && p + 4 <= j.size() ) {
				unsigned int cp = strtoul(j.substr(p, 4).c_str(), NULL, 16);
				p += 4;
				if ( cp >= 0xD800 && cp <= 0xDBFF && p + 6 <= j.size() && j[p] == '\\' && j[p + 1] == 'u' ) {
					unsigned int lo = strtoul(j.substr(p + 2, 4).c_str(), NULL, 16);
					p += 6;
					cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
				}
				put_utf8(out, cp);
			}
			else out += e;
		}
		return true;
	}
	return false;
}

// ------------------------------------------------------------------
// AI 에게 묻기
// ------------------------------------------------------------------
static std::string system_prompt(void)
{
	std::string s = std::string("당신은 1990년대 한국 PC통신 BBS '") + host_name + "' 의 AI 도우미입니다. "
		"사용자는 옛날 통신 프로그램의 80칸 글자 화면으로 대화합니다. "
		"한국어로 짧고 친근하게 답하세요. 보통 서너 문장, 길어도 열 줄을 넘기지 마세요. "
		"마크다운, 표, 굵은 글씨, 이모지, 그림 문자는 쓰지 마세요. 목록이 필요하면 '1.' 이나 '-' 로만 쓰세요. "
		"사용자가 영어로 물으면 영어로 답해도 됩니다.";
	return to_utf8(s);
}

static bool retry_ok;		// 다시 해 볼 만한 실패인지 (붐빔, 연결 끊김)

// 묻고 답 (UTF-8) 을 받는다. 못 받으면 false, err 에 까닭
static bool ask_once(const std::vector<chat_msg> &hist, std::string &answer, std::string &err)
{
	std::string body = "{\"model\":\"" + json_escape(ai_model) + "\",\"messages\":[";
	body += "{\"role\":\"system\",\"content\":\"" + json_escape(system_prompt()) + "\"}";
	for ( unsigned int i = 0; i < hist.size(); i++ ) {
		body += ",{\"role\":\"" + hist[i].role + "\",\"content\":\"" + json_escape(hist[i].text) + "\"}";
	}
	body += "]}";

	// 보낼 글과 curl 설정 (키를 명령줄에 드러내지 않도록 설정 파일로)
	char body_path[] = "/tmp/aichat_body.XXXXXX";
	char conf_path[] = "/tmp/aichat_conf.XXXXXX";
	int bfd = mkstemp(body_path);
	int cfd = mkstemp(conf_path);
	if ( bfd < 0 || cfd < 0 ) {
		if ( bfd >= 0 ) { close(bfd); unlink(body_path); }
		if ( cfd >= 0 ) { close(cfd); unlink(conf_path); }
		err = "임시 파일을 만들지 못했습니다";
		return false;
	}
	write(bfd, body.data(), body.size());
	close(bfd);
	std::string conf = "header = \"Content-Type: application/json\"\n";
	if ( !ai_key.empty() ) conf += "header = \"Authorization: Bearer " + ai_key + "\"\n";
	write(cfd, conf.data(), conf.size());
	close(cfd);

	std::string cmd = "curl -s -k --max-time 90 -A 'OldDosBBS/1.0' -K " + std::string(conf_path)
		+ " --data-binary @" + body_path + " -w '\\n%{http_code}' " + shell_quote(ai_url) + " 2>/dev/null";
	std::string out;
	FILE *fp = popen(cmd.c_str(), "r");
	if ( fp != NULL ) {
		char buf[4096];
		size_t n;
		while ( (n = fread(buf, 1, sizeof(buf), fp)) > 0 ) out.append(buf, n);
		pclose(fp);
	}
	unlink(body_path);
	unlink(conf_path);

	// 맨 끝 줄이 HTTP 상태
	std::string::size_type nl = out.rfind('\n');
	int code = atoi(nl == std::string::npos ? out.c_str() : out.c_str() + nl + 1);
	std::string json = nl == std::string::npos ? "" : out.substr(0, nl);
	retry_ok = false;
	if ( code != 200 ) {
		if ( code == 0 ) { err = "AI 에 연결하지 못했습니다. 잠시 뒤에 다시 물어 주세요"; retry_ok = true; }
		else if ( code == 429 ) err = "AI 가 붐빕니다. 잠시 뒤에 다시 물어 주세요";
		else if ( code == 400 || code == 401 || code == 403 ) err = "AI 키나 설정이 맞지 않습니다 (" + TO_STRING(code) + "). 운영자에게 알려 주세요";
		else if ( code == 404 ) err = "AI 모델 이름이나 주소가 맞지 않습니다 (404). 운영자에게 알려 주세요";
		else { err = "AI 가 답하지 못했습니다 (" + TO_STRING(code) + "). 잠시 뒤에 다시 물어 주세요"; retry_ok = code >= 500; }
		return false;
	}
	if ( !json_string(json, "content", answer) || trim(answer).empty() ) {
		err = "AI 의 답을 읽지 못했습니다. 다시 물어 주세요";
		return false;
	}
	return true;
}

// 연결이 끊기거나 서버 오류면 몇 번 다시 해 본다
static bool ask(const std::vector<chat_msg> &hist, std::string &answer, std::string &err)
{
	for ( int t = 0; t < 3; t++ ) {
		if ( t > 0 ) {
			printf(".");
			fflush(stdout);
			sleep(2 + t);
		}
		if ( ask_once(hist, answer, err) ) return true;
		if ( !retry_ok ) break;
	}
	return false;
}

// ------------------------------------------------------------------
// 화면
// ------------------------------------------------------------------

// 완성형 글을 width 칸에 맞춰 자른다 (띄어쓰기에서, 한글은 두 바이트를 함께)
static std::vector<std::string> wrap_text(const std::string &text, int width)
{
	std::vector<std::string> out;
	std::vector<std::string> paras = split_string(text, '\n');
	for ( unsigned int p = 0; p < paras.size(); p++ ) {
		std::string s = paras[p];
		while ( !s.empty() && (s[s.size() - 1] == ' ' || s[s.size() - 1] == '\r') ) s.erase(s.size() - 1);
		if ( s.empty() ) { out.push_back(""); continue; }
		while ( (int)s.size() > width ) {
			// width 안에서 글자 경계
			int cut = 0;
			while ( cut < (int)s.size() ) {
				int w = ((unsigned char)s[cut] >= 0x81) ? 2 : 1;
				if ( cut + w > width ) break;
				cut += w;
			}
			// 띄어쓰기에서 끊을 수 있으면 거기서
			int sp = cut;
			while ( sp > 0 && s[sp] != ' ' ) sp--;
			if ( sp > width / 3 ) cut = sp;
			out.push_back(s.substr(0, cut));
			s = s.substr(cut);
			while ( !s.empty() && s[0] == ' ' ) s.erase(0, 1);
		}
		if ( !s.empty() ) out.push_back(s);
	}
	// 앞뒤 빈 줄, 겹친 빈 줄 정리
	std::vector<std::string> r;
	for ( unsigned int i = 0; i < out.size(); i++ ) {
		if ( out[i].empty() && (r.empty() || r.back().empty()) ) continue;
		r.push_back(out[i]);
	}
	while ( !r.empty() && r.back().empty() ) r.pop_back();
	return r;
}

// 답을 늘어놓는다. 20 줄마다 쉰다
static void show_answer(const std::string &cp949)
{
	std::vector<std::string> lines = wrap_text(cp949, ANSWER_WIDTH);
	printf("\r\n" A_C "AI" A_W " > ");
	for ( unsigned int i = 0; i < lines.size(); i++ ) {
		if ( i > 0 && i % 20 == 0 ) {
			printf("\r\n     " A_G "-- 계속하려면 [Enter] --" A_W);
			press_enter();
			printf("\r\033[K");
		} else if ( i > 0 ) {
			printf("\r\n     ");
		}
		printf("%s", lines[i].c_str());
	}
	printf("\r\n");
}

static void show_intro(int left)
{
	print_header(title);
	printf("\r\n");
	printf("  " A_GR "AI 와 글로 이야기를 나눕니다." A_W " 궁금한 것을 묻거나 아무 이야기나 해 보세요.\r\n");
	printf("  " A_G "물은 글은 구글 Gemini 로 보내집니다. 개인 정보는 쓰지 마세요.\r\n");
	printf("  AI 의 답은 틀릴 수 있습니다." A_W);
	if ( ai_daily > 0 ) printf(A_G " 오늘 남은 질문 " A_Y "%d" A_G " 번" A_W, left);
	printf("\r\n\r\n");
	printf("  " A_Y "/n" A_W " 새 대화   " A_Y "/c" A_W " 화면 지우기   " A_Y "/q" A_W " 또는 " A_Y "." A_W " 끝내기\r\n");
	printf("  " A_G "%s" A_W "\r\n", repeat("─", 38).c_str());
}

int main(int argc, char **argv)
{
	snprintf(host_name, sizeof(host_name), "%s", argc > 1 ? argv[1] : "");
	snprintf(user_id, sizeof(user_id), "%s", argc > 2 ? argv[2] : "guest");
	snprintf(tty, sizeof(tty), "%s", argc > 3 ? argv[3] : "");
	// 아이디는 파일 이름에 쓰므로 글자/숫자/_ 만
	for ( char *p = user_id; *p; p++ ) if ( !isalnum((unsigned char)*p) && *p != '_' ) *p = '_';

	read_ai_settings();
	if ( argc > 1 ) snprintf(host_name, sizeof(host_name), "%s", argv[1]);

    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGHUP, (__sighandler_t)host_close);
    signal(SIGSEGV, (__sighandler_t)host_close);
    signal(SIGBUS, (__sighandler_t)host_close);
    signal(SIGPIPE, SIG_IGN);

    ioctl(0,TCGETA, &sys_term);
	raw_mode();
    umask(0022);

	std::vector<chat_msg> hist;
	show_intro(ai_daily - used_today());
	if ( ai_key.empty() ) {
		printf("\r\n  " A_R "아직 AI 키가 설정되지 않았습니다. 운영자에게 알려 주세요." A_W "\r\n");
		printf("  " A_G "(운영자: hanulso.cfg 에 <ai><key>...</key></ai>)" A_W "\r\n");
		printf("\r\n [Enter] 를 누르세요.");
		press_enter();
		host_close();
	}

	while ( 1 ) {
		char buf[INPUT_LEN + 8];
		printf(ESC_HAN);
		printf("\r\n" A_Y "나" A_W " > ");
		line_input(buf, INPUT_LEN);
		printf(ESC_ENG);
		std::string q = trim(buf);
		if ( q.empty() ) continue;

		if ( q == "." || !strcasecmp(q.c_str(), "/q") || q == "/끝" ) break;
		if ( !strcasecmp(q.c_str(), "/n") || q == "/새" ) {
			hist.clear();
			printf("\r\n  " A_G "새 대화를 시작합니다. 앞의 이야기는 잊었습니다." A_W "\r\n");
			continue;
		}
		if ( !strcasecmp(q.c_str(), "/c") ) {
			show_intro(ai_daily - used_today());
			continue;
		}

		int used = used_today();
		if ( ai_daily > 0 && used >= ai_daily ) {
			printf("\r\n  " A_R "오늘은 %d 번을 다 물었습니다. 내일 다시 이야기해요." A_W "\r\n", ai_daily);
			continue;
		}

		hist.push_back(make_msg("user", to_utf8(q)));
		while ( hist.size() > KEEP_MESSAGES + 1 ) hist.erase(hist.begin());

		printf("\r\n" A_G "     AI 가 생각하는 중입니다" A_W);
		fflush(stdout);
		std::string answer, err;
		bool ok = ask(hist, answer, err);
		printf("\r\033[K");
		if ( !ok ) {
			hist.pop_back();
			printf("\r\n  " A_R "%s." A_W "\r\n", err.c_str());
			continue;
		}

		set_used_today(used + 1);
		// 지난 대화로 보낼 답은 너무 길지 않게 (UTF-8 글자 경계에서)
		std::string keep = answer;
		if ( keep.size() > 2000 ) {
			std::string::size_type cut = 2000;
			while ( cut > 0 && ((unsigned char)keep[cut] & 0xC0) == 0x80 ) cut--;
			keep = keep.substr(0, cut);
		}
		hist.push_back(make_msg("assistant", keep));
		show_answer(answer_to_cp949(answer));
		if ( ai_daily > 0 && ai_daily - used - 1 <= 3 ) {
			printf("  " A_G "(오늘 남은 질문 %d 번)" A_W "\r\n", ai_daily - used - 1);
		}
	}

	host_close();
	return 0;
}

#include "main.h"

// ------------------------------------------------------------------
// 쪽지 : bin/memo <tty> <아이디>
//   받은 쪽지함 / 보낸 쪽지함, 읽기, 쓰기, 답장, 지우기
//   보낸 사람과 받는 사람이 각자 지울 수 있고, 둘 다 지우면 DB 에서 지운다.
// ------------------------------------------------------------------

void _line_input(char *str, char *init_str, int len, int echo);

struct termio sys_term;

char tty[10];

static std::string user_id;

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

void print_header(const char *title, int total_count, int page_count, int page_no)
{
	printf(ESC_CLEAR);
    printf("\033[1;1H");
	printf("\033[=9F\033[=1G%s\033[=15F\033[=1G", repeat("─", 40).c_str());
    printf("\033[1;1H");
	printf("\033[1A\033[7m%s\033[0m", host_name);
	int center = (80-strlen(strip_ansi_codes(title)))/2;
	if ( center < 0 ) center = 0;
    printf("\033[2;1H");
	printf("\r\033[%dC%s", center, title);

	if ( page_count > 0 ) {
		char buf[128];
		snprintf(buf, sizeof(buf), "%4d/%4d (총 %4d통) ", page_no, page_count, total_count);
		printf("\r\033[%dC%s", (int)(79 - strlen(buf)), buf);
	}

    printf("\033[3;1H");
	printf("\033[=0F\033[=1G%s\033[=15F\033[=1G", repeat("━", 40).c_str());
    printf("\033[4;1H");
}

void wait_enter(void)
{
	printf("\r\n[Enter] 를 누르세요.");
	press_enter();
}

// 아이디의 닉네임 (없으면 탈퇴회원)
std::string nick_of(const std::string &id)
{
	bool exist;
	std::map<std::string, std::string> u = database::user_info((char*)id.c_str(), &exist);
	if ( !exist ) return "탈퇴회원";
	std::string n = display_text(u["NICK_NAME"]);
	return n.empty() ? display_text(id) : n;
}

// 아이디나 닉네임으로 받는 사람 아이디 찾기 (없으면 "")
std::string find_recipient(const std::string &who)
{
	bool exist;
	database::user_info((char*)who.c_str(), &exist);
	if ( exist ) return who;
	std::map<std::string, std::string> u = database::user_info_by_nick_name((char*)who.c_str(), &exist);
	if ( exist ) return u["USER_ID"];
	return "";
}

// 쪽지 목록 (sent: 보낸 쪽지함)
std::vector<std::map<std::string, std::string> > fetch_memos(bool sent)
{
	std::string q;
	std::string id = database::escape(user_id.c_str());
	if ( sent ) {
		q = "SELECT * FROM memo WHERE SENDER_USER_ID='" + id + "' AND SENDER_DELETED=0 ORDER BY NO DESC";
	} else {
		q = "SELECT * FROM memo WHERE RECIPIENT_USER_ID='" + id + "' AND RECIPIENT_DELETED=0 ORDER BY NO DESC";
	}
	return database::fetch_rows((char*)q.c_str());
}

// 쪽지 지우기: 내 쪽에서만 지우고, 양쪽 모두 지웠으면 DB 에서 지운다
void delete_memo(const std::string &no, bool sent)
{
	char q[256];
	snprintf(q, sizeof(q), "UPDATE memo SET %s=1 WHERE NO=%d",
			sent ? "SENDER_DELETED" : "RECIPIENT_DELETED", atoi(no.c_str()));
	mysql_query(mysql, q);
	snprintf(q, sizeof(q), "DELETE FROM memo WHERE NO=%d AND SENDER_DELETED=1 AND RECIPIENT_DELETED=1",
			atoi(no.c_str()));
	mysql_query(mysql, q);
}

// 화면에 보이는 폭 (ESC 색 코드는 빼고, 완성형 한글은 2 칸)
static int text_width(const std::string &s)
{
	int w = 0;
	for (unsigned int i=0; i<s.size(); i++) {
		if ( s[i] == '\033' ) {
			while ( i < s.size() && !isalpha((unsigned char)s[i]) ) i++;
			continue;
		}
		w++;
	}
	return w;
}

// 쪽지 상자 한 줄: │ 왼쪽 ....... 오른쪽 │  (안쪽 74 칸)
static void box_line(const std::string &left, const std::string &right)
{
	int space = 72 - text_width(left) - text_width(right);
	if ( space < 1 ) space = 1;
	printf(" \033[=11F│\033[=15F %s%s%s \033[=11F│\033[=15F\r\n",
			left.c_str(), std::string(space, ' ').c_str(), right.c_str());
}

// 상자 아래 안내 줄 (8 번째 줄)
static void notice(const std::string &msg)
{
	printf("\033[8;1H%s\r   %s\033[=15F", std::string(79, ' ').c_str(), msg.c_str());
}

// 쪽지 보내기. to 가 비어 있으면 받는 사람을 묻는다
void write_memo(std::string to, std::string title)
{
	print_header("쪽지 쓰기", 0, 0, 0);

	// 읽기 화면과 같은 봉투 모양 상자에 바로 입력한다 (입력 칸은 19 번째 칸부터)
	printf(" \033[=11F┌%s┐\033[=15F\r\n", repeat("─", 37).c_str());
	box_line("\033[=14F◆ 받는 사람\033[=15F  "
			+ (to.empty() ? std::string("") : nick_of(to) + " \033[=7F(" + display_text(to) + ")\033[=15F"), "");
	box_line("\033[=14F◆ 제     목\033[=15F  " + title, "");
	printf(" \033[=11F└%s┘\033[=15F\r\n", repeat("─", 37).c_str());

	// 받는 사람
	while ( to.empty() ) {
		notice("\033[=7F받는 사람의 아이디나 닉네임을 입력하세요. (그냥 Enter 는 취소)");
		char buf[64];
		printf("\033[5;19H%s\033[5;19H", std::string(20, ' ').c_str());
		printf(ESC_HAN);
		_line_input(buf, (char*)"", 20, 1);
		std::string who = trim(buf);
		if ( who.empty() ) return;
		to = find_recipient(who);
		if ( to.empty() ) {
			notice("\033[=12F'" + who + "' 회원을 찾을 수 없습니다. 다시 입력하세요.");
			printf("\r\n");
			press_enter();
		}
	}
	printf("\033[5;1H");
	box_line("\033[=14F◆ 받는 사람\033[=15F  " + nick_of(to) + " \033[=7F(" + display_text(to) + ")\033[=15F", "");

	// 제목
	notice("\033[=7F제목을 입력하세요. (그냥 Enter 는 취소)");
	char tbuf[128];
	snprintf(tbuf, sizeof(tbuf), "%s", title.c_str());
	printf("\033[6;19H");
	printf(ESC_HAN);
	_line_input(tbuf, tbuf, 50, 1);
	std::string t = trim(tbuf);
	if ( t.empty() ) {
		notice("\033[=12F쪽지 쓰기를 취소했습니다.");
		wait_enter();
		return;
	}

	// 내용 (줄 편집기)
	notice("\033[=7F내용을 입력하세요.");
	std::vector<std::string> lines;
	line_editor_layout(3, 74);		// 상자 안쪽 폭에 맞춘다
	bool ok = line_editor(lines, false);
	line_editor_layout(0, 78);
	if ( !ok || lines.size() == 0 ) {
		printf("\r\n\r\n   \033[=12F쪽지 쓰기를 취소했습니다.\033[=15F");
		wait_enter();
		return;
	}

	std::string content;
	for (unsigned int i=0; i<lines.size(); i++) {
		content += lines[i];
		content += "\n";
	}

	std::string q = "INSERT INTO memo (SENDER_USER_ID, RECIPIENT_USER_ID, CREATION_DATETIME, TITLE, CONTENT) VALUES ('"
		+ database::escape(user_id.c_str()) + "', '" + database::escape(to.c_str()) + "', NOW(), '"
		+ database::escape(t.c_str()) + "', '" + database::escape(content.c_str()) + "')";
	if ( mysql_query(mysql, q.c_str()) != 0 ) {
		printf("\r\n\r\n   \033[=12F쪽지를 보내지 못했습니다. (%s)\033[=15F", mysql_error(mysql));
	} else {
		printf("\r\n\r\n   \033[=14F%s\033[=15F 님에게 쪽지를 보냈습니다.", nick_of(to).c_str());
		// 받는 사람이 대화방 같은 곳에 있으면 거기서 알린다 (프롬프트에서는 새 쪽지 알림이 따로 나옴)
		notify_online(to, "◆ 새 쪽지 ─ " + nick_of(user_id) + " 님이 쪽지를 보냈습니다. (MEMO 로 읽기)", false);
	}
	wait_enter();
}

// 쪽지 읽기. 지웠으면 true
bool read_memo(std::map<std::string, std::string> memo, bool sent)
{
	// 받은 쪽지를 처음 읽으면 읽은 시각을 적는다
	if ( !sent && memo["CONFIRMATION_DATETIME"].empty() ) {
		char q[256];
		snprintf(q, sizeof(q), "UPDATE memo SET CONFIRMATION_DATETIME=NOW() WHERE NO=%d", atoi(memo["NO"].c_str()));
		mysql_query(mysql, q);
	}

	std::vector<std::string> lines = split_string_with_width(memo["CONTENT"], '\n', 74);
	// 끝의 빈 줄은 빼고
	while ( lines.size() > 0 && trim(lines.back()).empty() ) lines.pop_back();
	unsigned int offset = 0;
	unsigned int page_count = (lines.size() + show_max_line - 1) / show_max_line;
	if ( page_count < 1 ) page_count = 1;

	std::string who_id = sent ? memo["RECIPIENT_USER_ID"] : memo["SENDER_USER_ID"];
	std::string who = string_truncate(nick_of(who_id), 20, "");
	std::string date = memo["CREATION_DATETIME"].substr(0, 16);

	while (1) {
		print_header(sent ? "보낸 쪽지" : "받은 쪽지", 0, 0, 0);

		// 봉투 모양 머리 상자
		printf(" \033[=11F┌%s┐\033[=15F\r\n", repeat("─", 37).c_str());
		box_line(std::string(sent ? "\033[=14F◆ 받는 사람\033[=15F  " : "\033[=14F◆ 보낸 사람\033[=15F  ")
				+ who + " \033[=7F(" + display_text(who_id) + ")\033[=15F",
				"\033[=7F" + date + "\033[=15F");

		std::string state;
		if ( sent ) {
			if ( memo["CONFIRMATION_DATETIME"].empty() ) state = "\033[=12F아직 안 읽음\033[=15F";
			else state = "\033[=10F읽음 " + memo["CONFIRMATION_DATETIME"].substr(5, 11) + "\033[=15F";
		}
		box_line("\033[=14F◆ 제     목\033[=15F  " + string_truncate(memo["TITLE"], 46, ""), state);
		printf(" \033[=11F└%s┘\033[=15F\r\n", repeat("─", 37).c_str());

		// 본문 (빈 줄로 채워 아래 줄 위치를 고정)
		for (unsigned int i=offset; i<offset+show_max_line; i++) {
			if ( i < lines.size() ) printf("   %s\r\n", lines[i].c_str());
			else printf("\r\n");
		}

		// 아래 줄: 쪽 번호
		if ( page_count > 1 ) {
			printf("%s %2d/%2d 쪽 %s\r\n", repeat("━", 33).c_str(),
					offset / show_max_line + 1, page_count, repeat("━", 2).c_str());
		} else {
			printf("%s\r\n", repeat("━", 40).c_str());
		}

		char cmd[64];
		printf(ESC_ENG);
		if ( sent ) printf("목록(P) 다음(N) 이전(B) 지우기(DD)  선택 >> ");
		else printf("목록(P) 다음(N) 이전(B) 답장(RE) 지우기(DD)  선택 >> ");
		line_input(cmd, 30);
		std::string c = trim(cmd);

		if ( c.empty() ) {
			if ( offset + show_max_line >= lines.size() ) return false;
			offset += show_max_line;
		} else if ( !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "x") ) {
			return false;
		} else if ( !strcasecmp(c.c_str(), "n") ) {
			if ( offset + show_max_line < lines.size() ) offset += show_max_line;
		} else if ( !strcasecmp(c.c_str(), "b") ) {
			offset = (offset >= show_max_line) ? offset - show_max_line : 0;
		} else if ( !strcasecmp(c.c_str(), "re") && !sent ) {
			std::string t = memo["TITLE"];
			if ( strncmp(t.c_str(), "Re: ", 4) ) t = "Re: " + t;
			write_memo(memo["SENDER_USER_ID"], t);
		} else if ( !strcasecmp(c.c_str(), "dd") ) {
			printf("\r\n이 쪽지를 지울까요? (y/N) ");
			if ( yesno(NO) == YES ) {
				delete_memo(memo["NO"], sent);
				return true;
			}
		}
	}
}

void show_memos(void)
{
	bool sent = false;
	int offset = 0;

	while (1) {
		std::vector<std::map<std::string, std::string> > rows = fetch_memos(sent);
		int total = rows.size();
		int page_count = (total + show_max_line - 1) / show_max_line;
		if ( page_count < 1 ) page_count = 1;
		if ( offset >= total && offset > 0 ) offset = ((total - 1) / show_max_line) * show_max_line;
		if ( offset < 0 ) offset = 0;

		print_header(sent ? "보낸 쪽지함" : "받은 쪽지함", total, page_count, offset / show_max_line + 1);

		printf("%5s %s %s  %s\r\n",
				"번호",
				centered(sent ? "받는 사람" : "보낸 사람", 12).c_str(),
				centered("날짜", 11).c_str(),
				"제목");
		printf("%s\r\n", repeat("─", 40).c_str());

		if ( total == 0 ) {
			printf("%s\r\n", centered("쪽지가 없습니다.", 80).c_str());
		}
		for (int i=offset; i<total && i<offset+(int)show_max_line; i++) {
			std::map<std::string, std::string> &m = rows[i];
			bool unread = m["CONFIRMATION_DATETIME"].empty();
			std::string who = nick_of(sent ? m["RECIPIENT_USER_ID"] : m["SENDER_USER_ID"]);
			std::string date = m["CREATION_DATETIME"].size() >= 16 ? m["CREATION_DATETIME"].substr(5, 11) : m["CREATION_DATETIME"];
			// 안 읽은 쪽지는 노란색 (보낸 쪽지함에서는 상대가 아직 안 읽은 쪽지)
			printf("%s%5d %s %s  %s\033[=15F\r\n", unread ? "\033[=14F" : "", i + 1,
					centered(string_truncate(who, 10, ""), 12).c_str(),
					date.c_str(),
					string_truncate(m["TITLE"], 46, "").c_str());
		}
		printf("%s\r\n", repeat("━", 40).c_str());

		char cmd[64];
		printf(ESC_ENG);
		printf("읽기(번호) 쓰기(W) %s 지우기(DD 번호) 다음(N) 이전(B) 나가기(P)\r\n선택 >> ",
				sent ? "받은쪽지함(R)" : "보낸쪽지함(S)");
		line_input(cmd, 30);
		std::vector<std::string> args = split_string(trim(cmd), ' ');

		if ( args.size() == 0 ) {
			if ( offset + (int)show_max_line < total ) offset += show_max_line;
			continue;
		}
		const char *a = args[0].c_str();

		if ( !strcasecmp(a, "p") || !strcasecmp(a, "x") || !strcasecmp(a, "q") ) {
			break;
		} else if ( !strcasecmp(a, "w") ) {
			write_memo("", "");
		} else if ( !strcasecmp(a, "s") ) {
			sent = true; offset = 0;
		} else if ( !strcasecmp(a, "r") ) {
			sent = false; offset = 0;
		} else if ( !strcasecmp(a, "n") ) {
			if ( offset + (int)show_max_line < total ) offset += show_max_line;
		} else if ( !strcasecmp(a, "b") ) {
			offset -= show_max_line;
			if ( offset < 0 ) offset = 0;
		} else if ( !strcasecmp(a, "dd") ) {
			int n = (args.size() > 1) ? atoi(args[1].c_str()) : 0;
			if ( n < 1 || n > total ) {
				printf("\r\n지울 쪽지 번호를 함께 입력하세요. (예: DD 3)");
				wait_enter();
				continue;
			}
			printf("\r\n%d 번 쪽지를 지울까요? (y/N) ", n);
			if ( yesno(NO) == YES ) delete_memo(rows[n-1]["NO"], sent);
		} else if ( is_number(args[0]) ) {
			int n = atoi(a);
			if ( n >= 1 && n <= total ) read_memo(rows[n-1], sent);
		}
	}
}

int main(int argc, char **argv)
{
	if ( argc < 3 ) {
		printf("usage: %s <tty> <user_id> [recipient]\n", argv[0]);
		return 1;
	}
	snprintf(tty, sizeof(tty), "%s", argv[1]);
	user_id = argv[2];

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

	// DB open ...
	if ( database::open() == false )
		exit(1);

	// 쪽지 테이블 생성
	database::create_memo();

	// bin/memo <tty> <아이디> <받는 사람> : 그 사람에게 바로 쓰고 돌아간다
	if ( argc > 3 && argv[3][0] ) {
		std::string to = find_recipient(argv[3]);
		if ( !to.empty() ) {
			write_memo(to, "");
			database::close();
			ioctl(0, TCSETAF, &sys_term);
			return 0;
		}
		print_header("쪽지", 0, 0, 0);
		printf("\r\n  '%s' 아이디나 닉네임을 찾을 수 없습니다. 쪽지함을 엽니다.\r\n", display_text(argv[3]).c_str());
		wait_enter();
	}

	show_memos();

	database::close();
    ioctl(0, TCSETAF, &sys_term);
	return 0;
}

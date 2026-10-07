#include "main.h"

// ------------------------------------------------------------------
// 줄 편집기 (글쓰기, 쪽지)
//   [S]등록 [Q]취소 [L]읽기 [E]고치기 [A]추가 [I]삽입 [D]지우기
//   한 줄이 차면 마지막 낱말을 다음 줄로 넘긴다. 첫 칸에 '.' 을 치면 쓰기를 끝낸다.
// ------------------------------------------------------------------

#define LE_WHITE	"\033[=15F"
#define LE_YELLOW	"\033[=14F"
#define LE_CYAN		"\033[=11F"
#define LE_GRAY		"\033[=7F"
#define LE_RED		"\033[=12F"

// 들여쓰기와 입력 폭 (쪽지처럼 상자 안에 맞출 때 바꾼다).
// 기본 76 칸: [L]읽기에서 줄 번호 3 칸을 붙여도 80 칸 안 (pico, 글 고치기와 같은 폭)
static int le_indent = 0;
static int le_width = 76;

void line_editor_layout(int indent, int width)
{
	le_indent = indent;
	le_width = width;
}

static std::string le_pad(void)
{
	return std::string(le_indent, ' ');
}

static void le_msg(const char *color, const std::string &msg)
{
	printf("\r\n%s%s%s" LE_WHITE, le_pad().c_str(), color, msg.c_str());
}

// 줄 번호 하나를 묻는다 (1 ~ max). 취소면 0
static int ask_line_no(const char *what, int max)
{
	char buf[16];
	printf(ESC_ENG);
	printf("\r\n%s%s (1~%d) : ", le_pad().c_str(), what, max);
	line_input(buf, 4);
	if ( strlen(buf) == 0 ) return 0;
	int n = atoi(buf);
	if ( !is_number(buf) || n < 1 || n > max ) {
		le_msg(LE_RED, std::string("그런 줄이 없습니다. (1~") + TO_STRING(max) + ")");
		return 0;
	}
	return n;
}

// 줄들을 받는다. 첫 칸에 '.' 이면 끝. 줄이 차면 마지막 낱말이 다음 줄로 넘어간다
static void read_lines(std::vector<std::string> &out)
{
	char buf[256];
	std::string carry;
	printf(ESC_HAN);
	while ( 1 ) {
		printf("%s", le_pad().c_str());
		std::string next;
		bool wrapped = line_input_wrap(buf, carry.c_str(), le_width, next);
		carry = next;
		if ( !wrapped && !strcmp(buf, ".") ) break;
		printf("\r\n");
		out.push_back(buf);
	}
	printf(ESC_ENG);
}

static void text_append(std::vector<std::string> &list)
{
	std::string ruler = "0---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----";
	printf("\r\n%s" LE_GRAY "줄이 차면 다음 줄로 저절로 넘어갑니다. 끝내려면 첫 칸에 " LE_YELLOW "." LE_GRAY " + Enter" LE_WHITE "\r\n",
			le_pad().c_str());
	printf("%s" LE_GRAY "%s" LE_WHITE "\r\n", le_pad().c_str(), ruler.substr(0, le_width).c_str());
	read_lines(list);
}

static void text_list(std::vector<std::string> &list)
{
	if ( list.empty() ) {
		le_msg(LE_GRAY, "아직 쓴 글이 없습니다.");
		return;
	}
	printf("\r\n%s" LE_GRAY "── 쓴 글 %d 줄 ──" LE_WHITE, le_pad().c_str(), (int)list.size());
	int shown = 0;
	for ( unsigned int l = 0; l < list.size(); l++ ) {
		if ( shown == 18 ) {
			char buf[4];
			printf(ESC_ENG);
			printf("\r\n%s" LE_GRAY "계속[Enter] 그만[P] : " LE_WHITE, le_pad().c_str());
			line_input(buf, 1);
			if ( buf[0] == 'p' || buf[0] == 'P' ) break;
			shown = 0;
		}
		printf("\r\n%s" LE_CYAN "%2d" LE_WHITE " %s", le_pad().c_str(), l + 1, list[l].c_str());
		shown++;
	}
}

// 한 줄 고치기: 원래 글을 넣어 둔 채로 고친다
static void text_edit(std::vector<std::string> &list)
{
	if ( list.empty() ) { le_msg(LE_GRAY, "고칠 줄이 없습니다."); return; }
	int l = ask_line_no("고칠 줄 번호", list.size());
	if ( l == 0 ) return;

	char buf[256];
	char init[256];
	snprintf(init, sizeof(init), "%s", list[l - 1].c_str());
	printf("\r\n%s" LE_GRAY "%d 번 줄을 고치세요 (그대로 두려면 Enter)" LE_WHITE "\r\n%s", le_pad().c_str(), l, le_pad().c_str());
	printf(ESC_HAN);
	line_input_edit(buf, init, le_width);
	printf(ESC_ENG);
	list[l - 1] = std::string(buf);
}

// 여러 줄 끼워 넣기: 그 줄 앞에 '.' 까지
static void text_insert(std::vector<std::string> &list)
{
	if ( list.empty() ) { text_append(list); return; }
	int l = ask_line_no("몇 번 줄 앞에 넣을까요", list.size());
	if ( l == 0 ) return;
	printf("\r\n%s" LE_GRAY "%d 번 줄 앞에 넣습니다. 끝내려면 첫 칸에 " LE_YELLOW "." LE_GRAY " + Enter" LE_WHITE "\r\n",
			le_pad().c_str(), l);
	std::vector<std::string> add;
	read_lines(add);
	list.insert(list.begin() + (l - 1), add.begin(), add.end());
	if ( !add.empty() ) le_msg(LE_GRAY, TO_STRING(add.size()) + " 줄을 넣었습니다.");
}

// 지우기: 3 또는 3-5
static void text_delete(std::vector<std::string> &list)
{
	if ( list.empty() ) { le_msg(LE_GRAY, "지울 줄이 없습니다."); return; }
	char buf[16];
	printf(ESC_ENG);
	printf("\r\n%s지울 줄 (예: 3 또는 3-5) : ", le_pad().c_str());
	line_input(buf, 9);
	if ( strlen(buf) == 0 ) return;
	int a = 0, b = 0;
	int n = sscanf(buf, "%d-%d", &a, &b);
	if ( n == 1 ) b = a;
	if ( n < 1 || a < 1 || b < a || b > (int)list.size() ) {
		le_msg(LE_RED, std::string("그런 줄이 없습니다. (1~") + TO_STRING(list.size()) + ")");
		return;
	}
	list.erase(list.begin() + (a - 1), list.begin() + b);
	le_msg(LE_GRAY, TO_STRING(b - a + 1) + " 줄을 지웠습니다.");
}

/* 줄 편집기 */
bool line_editor(std::vector<std::string> &lines, bool edit)
{
	char buf[4];
	std::vector<std::string> list;

	if ( !edit ) {
		text_append(list);		// 새로 쓰기
	} else {
		list = lines;			// 고치기: 지금 글을 먼저 보여 준다
		text_list(list);
	}

	while ( 1 ) {
		printf(ESC_ENG);
		printf("\r\n%s" LE_YELLOW "S" LE_WHITE ")등록 " LE_YELLOW "Q" LE_WHITE ")취소 " LE_YELLOW "L" LE_WHITE ")읽기 "
				LE_YELLOW "E" LE_WHITE ")고치기 " LE_YELLOW "A" LE_WHITE ")추가 " LE_YELLOW "I" LE_WHITE ")삽입 "
				LE_YELLOW "D" LE_WHITE ")지우기 " LE_GRAY "(%d줄)" LE_WHITE " : ", le_pad().c_str(), (int)list.size());
		line_input(buf, 2);
		char c = tolower(buf[0]);

		if ( c == 's' ) {
			if ( list.empty() ) { le_msg(LE_RED, "쓴 글이 없습니다. A 로 글을 쓰세요."); continue; }
			break;
		}
		else if ( c == 'l' ) text_list(list);
		else if ( c == 'a' ) text_append(list);
		else if ( c == 'e' ) text_edit(list);
		else if ( c == 'd' ) text_delete(list);
		else if ( c == 'i' ) text_insert(list);
		else if ( c == 'q' ) {
			printf("\r\n%s쓴 글을 버리고 그만둘까요? (y/N) ", le_pad().c_str());
			if ( yesno(NO) == YES ) return false;
		}
	}

	lines = list;
	return true;
}

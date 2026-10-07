#include "main.h"

// ------------------------------------------------------------------
// 글 고치기 (ED)
//   [1]줄 편집기 [2]화면 편집기(pico) [3]파일 올리기 [4]제목 [0]취소
//   Enter 만 누르면 줄 편집기로 바로 본문을 고친다 (1~3 은 글쓰기와 같은 번호)
// ------------------------------------------------------------------

static void cancel_msg(void)
{
	printf("\r\n취소 되었습니다.");
	printf("\r\n[Enter] 를 누르세요.");
	press_enter();
}

// 제목 고치기
static bool edit_title(char *table_name, int no, std::map<std::string, std::string> &row)
{
	char new_title[1024];
	std::string title = row["TITLE"];

	printf(ESC_HAN);
	printf("\r\n제목 : ");
	line_input_edit(new_title, (char*)title.c_str(), 60);
	printf(ESC_ENG);
	if ( strlen(trim(new_title).c_str()) == 0 ) {
		cancel_msg();
		return false;
	}
	if ( title == new_title ) return false;		// 그대로
	database::edit_article_title(table_name, no, new_title);
	return true;
}

// 줄 편집기로 본문 고치기
static bool edit_body_lines(char *table_name, int no, std::vector<std::string> lines)
{
	if ( !line_editor(lines, true) ) return false;		// Q 로 그만둠
	std::string str = string_convert(lines);
	if ( str.length() == 0 ) {
		printf("\r\n입력된 글이 없습니다.");
		printf("\r\n[Enter] 를 누르세요.");
		press_enter();
		return false;
	}
	database::edit_article_content(table_name, no, str);
	return true;
}

// 화면 편집기 (pico) 로 본문 고치기
static bool edit_body_screen(char *table_name, int no, const std::vector<std::string> &lines)
{
	char tmpfile[9072];
	char edit_dir[9072];
	char buf[9072];

	if ( !make_editor_tmpfile(edit_dir, tmpfile, sizeof(tmpfile)) ) {
		printf("\r\n임시 파일을 만들지 못했습니다.\r\n");
		press_enter();
		return false;
	}

	// 긴 줄은 pico 의 줄바꿈 폭(-r76) 에 맞춰 미리 나눠 둔다 (pico 는 불러온 줄을 다시 나누지 않는다)
	FILE *fp = fopen(tmpfile, "w");
	if ( fp != NULL ) {
		for ( unsigned int i = 0; i < lines.size(); i++ ) {
			std::vector<std::string> parts = wrap_words(trim(lines[i]), 76);
			for ( unsigned int k = 0; k < parts.size(); k++ ) {
				fprintf(fp, "%s\n", parts[k].c_str());
			}
		}
		fclose(fp);
	}
	std::string txt1 = read_file(tmpfile);

	snprintf(buf, sizeof(buf), "%s", screen_editor_command(edit_dir, tmpfile).c_str());
	ioctl(0, TCSETAF, &sys_term);
	system(buf);
	ioctl(0, TCSETAF, &curr_term);
	printf(ESC_RESET);

	std::string str = read_file(tmpfile);
	// 편집 폴더째 삭제 (파일만 지우면 빈 폴더가 tmp 에 쌓인다)
	remove_tmp_dir(edit_dir);

	if ( txt1 == str ) {
		cancel_msg();
		return false;
	}
	database::edit_article_content(table_name, no, str);
	return true;
}

// 파일을 올려서 본문 바꾸기 (Xmodem / Ymodem / Zmodem / Kermit)
static bool edit_body_upload(char *table_name, int no)
{
	int protocol = ask_upload_protocol();
	if ( protocol == 0 ) {
		cancel_msg();
		return false;
	}

	char *text = NULL;
	int length = 0;
	bool ok = false;
	if ( file_editor(&text, &length, protocol) && length > 0 ) {
		if ( is_binary(text, length) ) {
			printf("\r\n텍스트 파일만 지원합니다.");
			printf("\r\n[Enter] 를 누르세요.");
			press_enter();
		} else {
			database::edit_article_content(table_name, no, text);
			ok = true;
		}
	}
	if ( text != NULL ) free(text);
	return ok;
}

bool edit_article(char *table_name, int no)
{
	char cmd[8];
	char buf[256];

	if ( !login_user_is_admin && !database::check_same_author(table_name, no, login_user_id) ) {
		printf("\r\n작성자만 편집 할 수 있습니다.");
		printf("\r\n[Enter] 를 누르세요.");
		press_enter();
		return false;
	}

	snprintf(buf, sizeof(buf), "SELECT * FROM %s WHERE NO=%d", table_name, no);
	std::vector<std::map<std::string, std::string> > rows = database::fetch_rows(buf);
	if ( rows.size() == 0 ) {
		printf("\r\n게시글이 존재하지 않습니다.");
		printf("\r\n[Enter] 를 누르세요.");
		press_enter();
		return false;
	}
	std::map<std::string, std::string> row = rows.at(0);

	printf(ESC_ENG);
	printf("\r\n\033[=14F[1]\033[=15F줄 편집기 \033[=14F[2]\033[=15F화면 편집기(pico) \033[=14F[3]\033[=15F파일 올리기 "
			"\033[=14F[4]\033[=15F제목 \033[=14F[0]\033[=15F취소");
	printf("\r\n\033[=7F(Enter: 줄 편집기로 본문 고치기)\033[=15F >> ");
	line_input(cmd, 1);

	if ( !strcmp(cmd, "0") ) {
		cancel_msg();
		return false;
	}
	if ( !strcmp(cmd, "4") ) return edit_title(table_name, no, row);
	if ( strcmp(cmd, "") && strcmp(cmd, "1") && strcmp(cmd, "2") && strcmp(cmd, "3") ) {
		cancel_msg();
		return false;
	}

	if ( !strcmp(cmd, "3") ) return edit_body_upload(table_name, no);

	// 본문을 줄로 나눈다. 본문은 줄을 "\r\n" 으로 이어 저장한다 (string_convert): '\n' 으로만 나누면 줄 끝에 '\r' 이 남아
	// 고칠 때 커서가 줄 처음으로 가 버리고, 저장할 때마다 '\r' 이 하나씩 늘어난다
	std::vector<std::string> lines = split_string(row["CONTENT"], '\n');
	for ( unsigned int i = 0; i < lines.size(); i++ ) {
		while ( !lines[i].empty() && lines[i][lines[i].size() - 1] == '\r' ) {
			lines[i].erase(lines[i].size() - 1);
		}
	}

	if ( !strcmp(cmd, "2") ) return edit_body_screen(table_name, no, lines);
	return edit_body_lines(table_name, no, lines);
}

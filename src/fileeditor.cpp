#include "main.h"

// 함수를 빠져나갈 때 (실패해서 중간에 돌아가도) 임시 폴더를 지운다
struct file_editor_tmp_dir {
	std::string dir;
	~file_editor_tmp_dir() { if ( !dir.empty() ) remove_tmp_dir(dir.c_str()); }
};

// 글을 파일로 올릴 때 프로토콜 묻기. 1:Xmodem 2:Ymodem 3:Zmodem 4:Kermit, Enter 는 Zmodem, 취소는 0
int ask_upload_protocol(void)
{
	char buf[8];
	printf(ESC_ENG);
	printf("\r\n송신 프로토콜 (1:Xmodem 2:Ymodem 3:Zmodem 4:Kermit, Enter:Zmodem) : ");
	line_input(buf, 1);
	if ( strlen(buf) == 0 ) return 3;
	int p = atoi(buf);
	return (p >= 1 && p <= 4) ? p : 0;
}

// 텍스트 파일을 받아 글로 (protocol 은 ask_upload_protocol 의 값)
bool file_editor(char **lines, int *length, int protocol)
{
	char tmpdir[9072];
	char buf[9072];
		
	*length = 0;

	static const char *pname[] = { "", "Xmodem", "Ymodem", "Zmodem", "Kermit" };
	if ( protocol < 1 || protocol > 4 ) protocol = 3;
	printf("\r\n%s 로 텍스트 파일을 보내세요.\r\n", pname[protocol]);
	
	// ---------------------------------------------------
	// 우선 임시 폴더를 생성하여 업로드를 한다.
	sprintf(tmpdir, "%s/tmp", getenv("HANULSO"));
	sprintf(tmpdir, "%s", tempnam(tmpdir, "file"));

	if ( !mkdir2(tmpdir) ) {
		printf("\r\n[Enter] 를 누르세요.");
		press_enter();
		return false;
	}
	file_editor_tmp_dir guard;
	guard.dir = tmpdir;

	// 전송 중에 통신이 끊기면 접속 종료 처리(host_close)에서 폴더째 지우도록 적어 둔다
	add_user_tmpfile(tmpdir);

	// 폴더 변경
	chdir(tmpdir);

	// 프로토콜 실행 (자료실 올리기 file_upload 와 같은 방식)
	fflush(stdout);
	ioctl(0, TCSETAF, &sys_term);
	int a;
	if ( protocol == 1 ) {
		// 이름을 주지 않으면 lrzsz rz 는 Xmodem 이 아니라 Ymodem 묶음으로 받는다
		a = system("rz --xmodem -e xmodem.bin");
	} else if ( protocol == 2 ) {
		a = system("rz --ymodem -e");
	} else if ( protocol == 4 ) {
		sprintf(buf, KERMIT_PROG " -i -r", getenv("HANULSO"));
		a = system(buf);
	} else {
		a = system("rz --zmodem -e");
	}
	ioctl(0, TCSETAF, &curr_term);

	// 본래의 폴더로 복귀
	chdir(getenv("HANULSO"));

	// Xmodem 은 마지막 덩이를 ^Z 로 채워 보낸다
	if ( protocol == 1 && WEXITSTATUS(a) == 0 ) {
		snprintf(buf, sizeof(buf), "%s/xmodem.bin", tmpdir);
		strip_xmodem_pad(buf);
	}

	if ( WEXITSTATUS(a) != 0 ) {
		printf("\r\n파일 전송을 실패하였습니다.\r\n");
		printf("\r\n[Enter] 를 누르세요.");
		press_enter();
		return false;
	}

	// 전송된 텍스트 파일 검색
	sprintf(buf, "%s/*", tmpdir);
	std::vector<std::string> files = find_files_time_sorted(buf);
	if ( files.size() == 0 ) {
		printf("\r\n전송된 파일이 없습니다.");
		printf("\r\n[Enter] 를 누르세요.");
		press_enter();
		return false;
	}
	
	// 전송된 텍스트 파일을 unix2dos 를 수행한다.
	//sprintf(buf, "unix2dos --quiet %s", files[0].c_str());
	snprintf(buf, sizeof(buf), "%s/bin/unix2dos --quiet %s", getenv("HANULSO"), shell_quote(files[0]).c_str());
	a = system(buf);
	if ( WEXITSTATUS(a) != 0 ) {
		printf("\r\nunix2dos 실행을 실패하였습니다.\r\n");
		printf("\r\n[Enter] 를 누르세요.");
		press_enter();
		return false;
	}

	//sleep(1);

	// 전송된 파일의 텍스트 반환
	std::string text = read_file(files[0].c_str());

	// 조합형일 경우 완성형으로 변환
	if ( ks_conversion ) {
		int ks_count, kssm_count, eng_count;
		check_hangul_code((char*)text.c_str(), text.length(), &ks_count, &kssm_count, &eng_count);

		if ( ks_count != 0 || kssm_count != 0 ) {
			if (kssm_count > ks_count) {
				char out_file[1024];
				sprintf(out_file, "%s.out", files[0].c_str());

				snprintf(buf, sizeof(buf), "iconv -c -f JOHAB -t CP949//IGNORE//TRANSLIT %s > %s",
						shell_quote(files[0]).c_str(), shell_quote(out_file).c_str());
				a = system(buf);
				if ( WEXITSTATUS(a) != 0 ) {
					printf("\r\niconv 실행을 실패하였습니다.\r\n");
					printf("\r\n[Enter] 를 누르세요.");
					press_enter();
					return false;
				}

				text = read_file(out_file);
			}
		}
	}

	// 임시 폴더는 guard 가 지운다

#if 0
	*lines = (char*)malloc(text.length()+1);
	memcpy(*lines, text.c_str(), text.length());
	lines[text.length()] = '\0';
#else
	*lines = strdup(text.c_str());
#endif

	*length = text.length();

	printf("\r\n파일 전송을 성공하였습니다.\r\n");
	printf("\r\n[Enter] 를 누르세요.");
	fflush(stdout);
	press_enter();

	return true;
}

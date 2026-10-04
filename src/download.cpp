#include "main.h"

bool file_download(int protocol, char *tmp_filename, char *filename)
{
	char buf[9072];

	char tmpdir[1024];
	char path[1024];
	char path2[1024];

	// ---------------------------------------------------
	// 우선 임시 폴더를 생성하여 본래의 업로드 파일 이름으로 복사한다.
	sprintf(tmpdir, "%s/tmp", getenv("HANULSO"));
	sprintf(tmpdir, "%s", tempnam(tmpdir, "file"));
	if ( !mkdir2(tmpdir) ) {
		return false;
	}

	// 이전에 저장된 파일 이름에 위험한 문자가 있을 수 있으므로 / 는 막는다
	if ( strchr(tmp_filename, '/') || strchr(filename, '/') ) {
		return false;
	}

	// 임시 파일 이름으로 업로드된 패스
	snprintf(path, sizeof(path), "%s/file/%s", getenv("HANULSO"), tmp_filename);

	// 본래의 파일 이름으로 복사될 위치
	snprintf(path2, sizeof(path2), "%s/%s", tmpdir, filename);

	printf("\r\n파일 수신 준비 중입니다."); fflush(stdout);
	
	// 임시 파일 패스를 적어둔다. 
	// 파일 복사중 통신을 종료하면 쓰레기 파일로 남기 때문에 나중에 자동 종료시 삭제한다.
	add_user_tmpfile(path2);

	// 임시 폴더로 본래의 이름으로 복사
	snprintf(buf, sizeof(buf), "cp %s %s", shell_quote(path).c_str(), shell_quote(path2).c_str());
	int a = system(buf);

	if ( WEXITSTATUS(a) != 0 ) {
		return false;
	}
		
	// 임시 폴더로 이동
	chdir(tmpdir);
	
	printf("\r\n전송 프로토콜을 실행하세요.\r\n");
	fflush(stdout);

	// zmodem 프로토콜 실행
    std::string qname = shell_quote(filename);
    if (protocol == 1) {
        snprintf(buf, sizeof(buf), "sz --xmodem -e %s", qname.c_str());
    } else if(protocol == 2) {
        snprintf(buf, sizeof(buf), "sz --ymodem -e %s", qname.c_str());
    } else if(protocol == 3) {
        snprintf(buf, sizeof(buf), "sz --zmodem -e %s", qname.c_str());
    } else if(protocol == 4) {
        snprintf(buf, sizeof(buf), KERMIT_PROG " -i -s %s", getenv("HANULSO"), qname.c_str());   // Kermit: 바이너리로 보내기
    }
	//sprintf(buf, "%s/bin/sexyz sz \"%s\"", getenv("HANULSO"), filename);
	ioctl(0, TCSETAF, &sys_term);
	a = system(buf);
	ioctl(0, TCSETAF, &curr_term);

	chdir(getenv("HANULSO"));

	if ( WEXITSTATUS(a) != 0 ) {
		return false;
	}

	// 업로드된 임시 폴더를 삭제한다.
	snprintf(buf, sizeof(buf), "rm -rf %s", shell_quote(tmpdir).c_str());
	a = system(buf);

	if ( WEXITSTATUS(a) != 0 ) {
		return false;
	}

	return true;
}


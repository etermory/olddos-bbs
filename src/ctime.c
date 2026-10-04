/* 키 입력 시간 검사 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termio.h>

int isatty(int fd)
{
    struct termios term;
    return(ioctl(fd, TCGETA, &term) != -1);
}

int main(int argc, char **argv)
{

	char buf[1024], *tmp, *getty = NULL;
	FILE *fp;

	umask(0111);

	int ps;
	for(ps = 0; ps <= 10; ps++) {
		if(isatty(ps)) {
			getty = ttyname(ps);
			break;
		}
	}

	// 터미널을 찾지 못한 경우
	if ( getty == NULL || strlen(getty) < 9 ) {
		fprintf(stderr, "tty not found\n");
		return 1;
	}

	tmp = &getty[9];

	sprintf(buf, "tmp/%s.tty", tmp);
	fp = fopen(buf, "w");
	if ( fp != NULL ) {
		fclose(fp);
	}

	pid_t ps_parent = getpid();	// fork 뒤 자식에서 부모가 살아 있는지 확인용 (부모는 execl 로 main 이 되어도 pid 가 같다)
	ps = fork();
	if (ps) {
		execl("bin/main", "main", tmp, (char*)0);
	}
	else {
		while(1) {
			time_t current;
			struct stat statbuf;

			sleep(10);

			// BBS(부모) 가 끝났으면 감시도 끝낸다
			if ( getppid() != ps_parent ) exit(0);

			fstat(0, &statbuf);
			time(&current);

			// 60*10(10분) 동안 키 입력이 없으면 종료
			if(current - (statbuf.st_atime) >= 600) {
				ps = getppid();
				sleep(1);
				printf("\r\n\007키 입력이 없어서 자동으로 끊습니다.\r\n");
				kill(ps, SIGHUP);
				sleep(1);
				exit(0);
			}
		}
	}

	return 0;
}


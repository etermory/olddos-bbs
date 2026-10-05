#!/bin/bash
# 쥬라기공원 머드 업데이트: 받기 -> 빌드 -> 끄기 -> 설치 -> 시작 -> 확인
#   ./update.sh [설치 디렉터리] [빌드 디렉터리]
#     기본: /home/olddos/jurassic  /tmp/jurassic-build
#   NOPULL=1 ./update.sh   git pull 을 건너뜀
#   NOBUILD=1 ./update.sh  빌드를 건너뜀 (게임 내용만 바뀌었을 때, 빌드 디렉터리가 남아 있어야 함)
#
# 빌드는 머드를 끄기 전에 한다. 빌드가 실패하면 머드는 켜진 그대로 둔다.

# git pull 이 이 파일을 바꿔도 꼬이지 않도록 전체를 읽은 뒤 main 을 실행한다
main() {
	set -e

	SRC=$(cd "$(dirname "$0")" && pwd)
	DEST=${1:-/home/olddos/jurassic}
	BUILD=${2:-/tmp/jurassic-build}
	PORT=4444

	# 1) 받기
	if [ -z "$NOPULL" ]; then
		step "1/6 소스 받기 (git pull)"
		( cd "$SRC" && git pull --ff-only ) || { say "git pull 실패. 머드는 그대로입니다."; exit 1; }
	else
		step "1/6 소스 받기: 건너뜀"
	fi

	# 2) 빌드 (머드는 아직 켜져 있다)
	if [ -z "$NOBUILD" ]; then
		step "2/6 빌드: $BUILD"
		"$SRC/build.sh" "$BUILD" > "$BUILD.log" 2>&1 || {
			tail -20 "$BUILD.log"
			say "빌드 실패 (전체 기록: $BUILD.log). 머드는 그대로입니다."
			exit 1
		}
		say "빌드 완료 (기록: $BUILD.log)"
	else
		step "2/6 빌드: 건너뜀"
	fi
	[ -x "$BUILD/driver/driver" ] || { say "$BUILD/driver/driver 가 없습니다. NOBUILD 없이 다시 실행하세요."; exit 1; }

	# 3) 끄기
	step "3/6 머드 끄기"
	stop_mud

	# 4) 설치
	step "4/6 설치: $DEST"
	"$SRC/deploy.sh" "$DEST" "$BUILD"

	# 5) 시작
	step "5/6 시작"
	# startmud 는 스스로 자기 디렉터리로 옮긴다. 하위 셸 없이 바로 띄워야
	# 이 스크립트의 출력(터미널/파이프)을 붙잡고 남는 bash 가 생기지 않는다.
	nohup "$DEST/bin/startmud" > /dev/null 2>&1 < /dev/null &

	# 6) 확인: 포트가 열릴 때까지 최대 30 초
	step "6/6 확인: 127.0.0.1:$PORT"
	local i=0
	while [ $i -lt 30 ]; do
		if (exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null; then
			say "머드가 떴습니다. (startmud pid $(cat "$DEST/bin/startmud.pid"), 드라이버 pid $(cat "$DEST/bin/driver.pid"))"
			say "BBS 쪽 연결 프로그램이 바뀌었으면: cd $SRC/.. && make mudlink"
			exit 0
		fi
		sleep 1
		i=$((i + 1))
	done
	say "30 초 안에 $PORT 번 포트가 열리지 않았습니다. 로그를 보세요:"
	say "  $DEST/lib/log/driver/driver.err, startmud.log"
	exit 1
}

# 안내 문구: 터미널이 EUC-KR 이면 EUC-KR 로 바꿔 출력 (스크립트는 UTF-8)
say() {
	case "${LC_ALL:-${LANG:-}}" in
		*[Ee][Uu][Cc]*|*949*) printf '%s\n' "$*" | iconv -f UTF-8 -t CP949 2>/dev/null || printf '%s\n' "$*" ;;
		*) printf '%s\n' "$*" ;;
	esac
}

step() {
	echo
	say "=== $* ==="
}

# 이 설치본의 startmud / 드라이버 pid 들 (pid 파일이 틀리거나 여러 개 떠 있어도 찾는다)
#   startmud : 작업 디렉터리가 $DEST/bin 인 bash
#   드라이버 : 실행 파일이 $DEST/bin/driver (드라이버는 시작하면 lib 로 디렉터리를 옮긴다)
mud_pids() {
	local pid cwd exe cmd
	for pid in $(ls /proc | grep -E '^[0-9]+$'); do
		[ "$pid" = $$ ] && continue
		exe=$(readlink "/proc/$pid/exe" 2>/dev/null) || continue
		exe=${exe% (deleted)}
		if [ "$exe" = "$DEST/bin/driver" ]; then
			echo "D $pid"
			continue
		fi
		cwd=$(readlink "/proc/$pid/cwd" 2>/dev/null) || continue
		[ "$cwd" = "$DEST/bin" ] || continue
		cmd=$(tr '\0' ' ' < "/proc/$pid/cmdline" 2>/dev/null)
		case "$cmd" in
			*startmud*) echo "L $pid" ;;
		esac
	done
}

stop_mud() {
	local loops drvs pid i
	loops=$(echo $(mud_pids | awk '$1=="L"{print $2}'))
	drvs=$(echo $(mud_pids | awk '$1=="D"{print $2}'))
	if [ -z "$loops$drvs" ]; then
		say "실행 중인 머드가 없습니다."
		return 0
	fi
	# startmud 를 먼저 끄고 (다시 띄우지 않게), 드라이버를 끈다 (SIGTERM: 접속자 자료 저장 후 종료)
	[ -n "$loops" ] && kill $loops 2>/dev/null || true
	[ -n "$drvs" ] && kill $drvs 2>/dev/null || true
	for i in $(seq 1 30); do
		[ -z "$(mud_pids)" ] && break
		sleep 1
	done
	if [ -n "$(mud_pids)" ]; then
		say "30 초 안에 끝나지 않아 강제로 끕니다 (kill -9)."
		kill -9 $(mud_pids | awk '{print $2}') 2>/dev/null || true
		sleep 1
	fi
	rm -f "$DEST/bin/startmud.pid" "$DEST/bin/driver.pid"
	say "머드를 껐습니다. (startmud: ${loops:-없음} / 드라이버: ${drvs:-없음})"
}

main "$@"
exit

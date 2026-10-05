#!/bin/bash
# 무한대전 업데이트: 받기 -> 빌드 -> 끄기 -> 설치 -> 시작 -> 확인
#   ./update.sh [설치 디렉터리] [빌드 디렉터리]
#     기본: /home/olddos/muhan  /tmp/muhan-build
#   NOPULL=1 ./update.sh   git pull 을 건너뜀
#
# 빌드는 무한대전을 끄기 전에 한다. 빌드가 실패하면 켜진 그대로 둔다.
# 처음 실행하면 데이터를 풀어 새로 설치하고, 그다음부터는 실행 파일만 바꾼다 (deploy.sh).

# git pull 이 이 파일을 바꿔도 꼬이지 않도록 전체를 읽은 뒤 main 을 실행한다
main() {
	set -e

	SRC=$(cd "$(dirname "$0")" && pwd)
	DEST=${1:-/home/olddos/muhan}
	BUILD=${2:-/tmp/muhan-build}

	if [ -z "$NOPULL" ]; then
		step "1/6 소스 받기 (git pull)"
		( cd "$SRC" && git pull --ff-only ) || { say "git pull 실패. 무한대전은 그대로입니다."; exit 1; }
	else
		step "1/6 소스 받기: 건너뜀"
	fi

	step "2/6 빌드: $BUILD"
	"$SRC/build.sh" "$BUILD" "$DEST" > "$BUILD.log" 2>&1 || {
		tail -20 "$BUILD.log"
		say "빌드 실패 (전체 기록: $BUILD.log). 무한대전은 그대로입니다."
		exit 1
	}
	say "빌드 완료 (기록: $BUILD.log)"

	step "3/6 무한대전 끄기"
	stop_mud

	step "4/6 설치: $DEST"
	"$SRC/deploy.sh" "$DEST" "$BUILD"

	step "5/6 시작"
	# startmud 는 스스로 자기 디렉터리로 옮긴다. 하위 셸 없이 바로 띄워야
	# 이 스크립트의 출력(터미널/파이프)을 붙잡고 남는 bash 가 생기지 않는다.
	nohup "$DEST/bin/startmud" > /dev/null 2>&1 < /dev/null &

	PORT=$(cat "$DEST/bin/port" 2>/dev/null || echo 4100)
	step "6/6 확인: 127.0.0.1:$PORT"
	local i=0
	while [ $i -lt 30 ]; do
		if (exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null; then
			say "무한대전이 떴습니다. (startmud pid $(cat "$DEST/bin/startmud.pid"), 엔진 pid $(cat "$DEST/bin/frp.pid"))"
			exit 0
		fi
		sleep 1
		i=$((i + 1))
	done
	say "30 초 안에 $PORT 번 포트가 열리지 않았습니다. 로그를 보세요:"
	say "  $DEST/log/startmud.log, $DEST/log/frp.out, $DEST/log/log"
	exit 1
}

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

# 이 설치본의 startmud / 엔진 pid 들 (pid 파일이 틀리거나 여러 개 떠 있어도 찾는다)
#   startmud : 작업 디렉터리가 $DEST/bin 인 bash
#   엔진     : 실행 파일이 $DEST/bin/frp
mud_pids() {
	local pid cwd exe cmd
	for pid in $(ls /proc | grep -E '^[0-9]+$'); do
		[ "$pid" = $$ ] && continue
		exe=$(readlink "/proc/$pid/exe" 2>/dev/null) || continue
		exe=${exe% (deleted)}
		if [ "$exe" = "$DEST/bin/frp" ]; then
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
	local loops drvs i
	loops=$(echo $(mud_pids | awk '$1=="L"{print $2}'))
	drvs=$(echo $(mud_pids | awk '$1=="D"{print $2}'))
	if [ -z "$loops$drvs" ]; then
		say "실행 중인 무한대전이 없습니다."
		return 0
	fi
	# startmud 를 먼저 끄고, 엔진을 끈다 (SIGTERM: 접속자 저장 후 종료)
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
	rm -f "$DEST/bin/startmud.pid" "$DEST/bin/frp.pid"
	say "무한대전을 껐습니다. (startmud: ${loops:-없음} / 엔진: ${drvs:-없음})"
}

main "$@"
exit

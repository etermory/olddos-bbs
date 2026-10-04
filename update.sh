#!/bin/bash
# BBS 업데이트: 받기 -> 빌드 -> 반영
#   ./update.sh [빌드 디렉터리]      (기본: /tmp/olddos-bbs-build)
#   NOPULL=1 ./update.sh             git pull 을 건너뜀
#
# - 빌드는 임시 디렉터리에서 처음부터 한다 (makefile 에 헤더 의존성이 없어 .o 를 다시 쓰지 않는다).
# - 모두 성공했을 때만 bin/ 에 반영한다. 빌드가 실패하면 BBS 는 그대로다.
# - 실행 파일은 임시 이름으로 복사한 뒤 mv 로 바꾼다. 접속 중인 사용자는 기존 프로그램을 계속 쓰고
#   (Text file busy 없음), 다음 접속부터 새 버전이 실행된다. 따로 재시작할 것은 없다.
# - 쥬라기공원 머드는 src/jurassic/update.sh 로 따로 한다.

# git pull 이 이 파일을 바꿔도 꼬이지 않도록 전체를 읽은 뒤 main 을 실행한다
main() {
	set -e

	HOME_DIR=$(cd "$(dirname "$0")" && pwd)
	BUILD=${1:-/tmp/olddos-bbs-build}

	# 1) 받기
	if [ -z "$NOPULL" ]; then
		step "1/3 소스 받기 (git pull)"
		( cd "$HOME_DIR" && git pull --ff-only ) || { say "git pull 실패. BBS 는 그대로입니다."; exit 1; }
	else
		step "1/3 소스 받기: 건너뜀"
	fi

	# 2) 빌드: 임시 디렉터리에 src 를 복사해 처음부터 (.o 와 머드 소스는 빼고)
	step "2/3 빌드: $BUILD"
	rm -rf "$BUILD"
	mkdir -p "$BUILD/src" "$BUILD/bin"
	( cd "$HOME_DIR/src" && tar cf - --exclude=./jurassic --exclude='*.o' . ) | ( cd "$BUILD/src" && tar xf - )
	if ! ( cd "$BUILD/src" && make all ) > "$BUILD.log" 2>&1; then
		grep -E "error|Error|undefined" "$BUILD.log" | head -20
		say "빌드 실패 (전체 기록: $BUILD.log). BBS 는 그대로입니다."
		exit 1
	fi
	say "빌드 완료 (기록: $BUILD.log)"

	# 3) 반영
	step "3/3 반영: $HOME_DIR/bin"
	changed=""
	same=0
	local f
	for f in "$BUILD/bin"/*; do
		[ -f "$f" ] && install_prog "$f" "$HOME_DIR/bin"
	done
	# ctime (접속하면 runbbs 가 처음 실행하는 프로그램) 은 BBS 홈에 만들어진다
	[ -f "$BUILD/ctime" ] && install_prog "$BUILD/ctime" "$HOME_DIR"
	if [ -n "$changed" ]; then
		say "바뀐 프로그램:$changed"
	else
		say "바뀐 프로그램이 없습니다."
	fi
	say "그대로인 프로그램: $same 개"
	say "다음 접속부터 새 버전이 실행됩니다. (화면 파일 txt/, 메뉴 *.mnu 는 git pull 로 바로 반영)"
	case "$changed" in
		*mudlink*) say "mudlink 가 바뀌었습니다. 머드 쪽도 바뀌었으면 src/jurassic/update.sh 를 실행하세요." ;;
	esac
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

# 실행 파일 하나를 $2 디렉터리에 반영. 같으면 건너뛰고, 다르면 임시 이름으로 복사한 뒤 mv
# (실행 중인 프로그램도 바꿀 수 있고, 접속 중인 사용자는 기존 프로그램을 계속 쓴다)
install_prog() {
	local f=$1 dir=$2 name
	name=$(basename "$f")
	if [ -f "$dir/$name" ] && cmp -s "$f" "$dir/$name"; then
		same=$((same + 1))
		return 0
	fi
	cp "$f" "$dir/.$name.new"
	chmod 755 "$dir/.$name.new"
	mv -f "$dir/.$name.new" "$dir/$name"
	changed="$changed $name"
}

main "$@"
exit

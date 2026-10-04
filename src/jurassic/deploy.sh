#!/bin/bash
# 쥬라기공원 머드 설치 / 갱신
#   ./deploy.sh <설치 디렉터리> [빌드 디렉터리] [원본 lib 디렉터리]
#   예) ./build.sh /tmp/jurassic-build
#       ./deploy.sh /home/olddos/jurassic /tmp/jurassic-build
#
# 원본 머드 라이브러리는 같은 폴더의 jp2_v15.tgz (HanLP 쥬라기공원 2 복원판).
# 세 번째 인자나 src/jurassic/lib 가 없으면 이 압축 파일을 풀어서 쓴다.
#
# - 원본 lib 위에 libpatch/ (원본 MudOS 로 옮기며 고친 파일) 를 덮어쓴다.
# - 파일 이름은 EUC-KR 로 설치한다. 원본이 UTF-8 이름(윈도우에서 복사)이면 바꾸고,
#   이미 EUC-KR 이름(리눅스에서 푼 것)이면 그대로 쓴다. (파일 내용은 원래 EUC-KR)
# - 다시 설치해도 lib/data (사용자 자료), lib/log 의 기존 파일은 덮어쓰지 않는다.
set -e

# 파일 이름을 바이트 그대로 다룬다. UTF-8 로캘에서는 bash read 가 EUC-KR 이름을
# 깨진 UTF-8 로 보고 줄바꿈까지 삼켜 두 경로가 붙어 버린다.
TERM_LANG="${LC_ALL:-${LANG:-}}"
export LC_ALL=C

# 안내 문구: 터미널이 EUC-KR 이면 EUC-KR 로 바꿔 출력 (스크립트는 UTF-8)
say() {
	case "$TERM_LANG" in
		*[Ee][Uu][Cc]*|*949*) printf '%s\n' "$*" | iconv -f UTF-8 -t CP949 2>/dev/null || printf '%s\n' "$*" ;;
		*) printf '%s\n' "$*" ;;
	esac
}

# 실행 중인 파일도 바꿀 수 있도록 임시 이름으로 복사한 뒤 이름을 바꾼다
# (실행 중인 driver 위에 바로 cp 하면 'Text file busy')
install_file() {
	cp "$1" "$2.new.$$"
	mv -f "$2.new.$$" "$2"
}

SRC=$(cd "$(dirname "$0")" && pwd)
DEST=${1:?usage: $0 <설치 디렉터리> [빌드 디렉터리] [원본 lib 디렉터리]}
BUILD=${2:-/tmp/jurassic-build}
LIB=${3:-$SRC/lib}

if [ ! -x "$BUILD/driver/driver" ]; then
	say "드라이버가 없습니다: $BUILD/driver/driver (먼저 ./build.sh $BUILD)"
	exit 1
fi
if [ ! -f "$LIB/adm/master/simul_efun.c" ]; then
	if [ -f "$SRC/jp2_v15.tgz" ]; then
		# 저장소의 원본 압축 파일을 풀어서 쓴다 (리눅스에서 풀면 파일 이름이 EUC-KR 그대로)
		TMPLIB=$(mktemp -d /tmp/jurassic-lib.XXXXXX)
		trap 'rm -rf "$TMPLIB"' EXIT
		tar xzf "$SRC/jp2_v15.tgz" -C "$TMPLIB"
		LIB="$TMPLIB/HanLP/lib"
		say "원본 라이브러리: $SRC/jp2_v15.tgz"
	else
		say "원본 머드 라이브러리가 없습니다: $LIB ($SRC/jp2_v15.tgz 도 없음)"
		exit 1
	fi
fi

mkdir -p "$DEST/bin" "$DEST/lib"

# 파일 이름을 EUC-KR 로 (CONVERT=1 일 때만 UTF-8 -> EUC-KR)
CONVERT=0
euckr_name() {
	if [ "$CONVERT" = 1 ]; then
		printf '%s' "$1" | iconv -f UTF-8 -t CP949
	else
		printf '%s' "$1"
	fi
}

# 디렉터리 하나를 lib 에 복사 (keep_user_data=1 이면 data/log 기존 파일 보존)
# EUC-KR 바이트 중에는 우연히 올바른 UTF-8 인 것도 있으므로 이름마다가 아니라
# 트리 전체로 판단한다: 모든 이름이 UTF-8 이면 변환, 아니면 이미 EUC-KR 로 본다.
copy_tree() {
	local from=$1 keep=$2
	cd "$from"
	# (UTF-8 -> UTF-8 은 glibc 가 검사 없이 복사하므로 UTF-16 으로 바꿔 보며 검사)
	if find . | iconv -f UTF-8 -t UTF-16 > /dev/null 2>&1; then CONVERT=1; else CONVERT=0; fi
	find . -type d -print0 | while IFS= read -r -d '' d; do
		mkdir -p "$DEST/lib/$(euckr_name "$d")"
	done
	find . -type f -print0 | while IFS= read -r -d '' f; do
		n=$(euckr_name "$f")
		if [ "$keep" = 1 ]; then
			case "$f" in
				./data/*|./log/*) [ -e "$DEST/lib/$n" ] && continue ;;
			esac
		fi
		cp -p "$f" "$DEST/lib/$n"
	done
}

copy_tree "$LIB" 1
copy_tree "$SRC/libpatch" 0

# simul_efun 에 hanlp.c (원래 HanLP 드라이버에 있던 함수들) 를 맨 앞에 넣는다
SE="$DEST/lib/adm/master/simul_efun.c"
if ! grep -q 'efun/hanlp.c' "$SE"; then
	sed -i 's|^#include "/adm/master/efun/base_name.c"|#include "/adm/master/efun/hanlp.c"\n#include "/adm/master/efun/base_name.c"|' "$SE"
fi
grep -q 'efun/hanlp.c' "$SE" || { say "simul_efun.c 에 hanlp.c 를 넣지 못했습니다"; exit 1; }

# 드라이버가 쓰는 디렉터리
mkdir -p "$DEST/lib/log/driver" "$DEST/lib/adm/tmp"

# bin
install_file "$BUILD/driver/driver" "$DEST/bin/driver"
[ -x "$BUILD/driver/addr_server" ] && install_file "$BUILD/driver/addr_server" "$DEST/bin/addr_server"
install_file "$SRC/bin/config.jurassic" "$DEST/bin/config.jurassic"
install_file "$SRC/bin/startmud" "$DEST/bin/startmud"
install_file "$SRC/bin/killmud" "$DEST/bin/killmud"
chmod +x "$DEST/bin/driver" "$DEST/bin/startmud" "$DEST/bin/killmud"

say "설치 완료: $DEST"
say "실행: cd $DEST/bin && nohup ./startmud > /dev/null 2>&1 &"

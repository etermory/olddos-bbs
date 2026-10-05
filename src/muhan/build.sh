#!/bin/bash
# 무한대전 빌드 (64 비트)
#   ./build.sh [빌드 디렉터리] [설치 디렉터리]
#     기본: /tmp/muhan-build  /home/olddos/muhan
# 결과: <빌드 디렉터리>/frp
#
# 데이터 경로(MUDHOME)는 실행 파일에 들어가므로 설치 디렉터리를 함께 받는다.
# 필요한 패키지 (CentOS 6): gcc make
#
# 원본은 32 비트 리눅스에서 데이터 파일에 구조체를 그대로 저장했다.
# 64 비트에서는 disk32.c 가 읽고 쓸 때 32 비트 배치로 바꾼다 (tools/gen_disk32.py 로 만듦).
set -e

say() {
	case "${LC_ALL:-${LANG:-}}" in
		*[Ee][Uu][Cc]*|*949*) printf '%s\n' "$*" | iconv -f UTF-8 -t CP949 2>/dev/null || printf '%s\n' "$*" ;;
		*) printf '%s\n' "$*" ;;
	esac
}

SRC=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-/tmp/muhan-build}
DEST=${2:-/home/olddos/muhan}

rm -rf "$OUT"
mkdir -p "$OUT/bin"
cp -r "$SRC/game/src" "$OUT/src"
cd "$OUT/src"
# 윈도우에서 받은 경우 줄 끝 정리
sed -i 's/\r$//' makefile *.c *.h 2>/dev/null || true
touch ../bin/frp

# -std=gnu89 -fcommon: 1990 년대 C 코드를 그대로 받아들이게
# -DFOREGROUND: startmud 가 지켜보도록 백그라운드로 빠지지 않게
make CC="gcc -O2 -std=gnu89 -fcommon -w -DFOREGROUND -DMUDHOME=\\\"$DEST\\\"" frp.new
cp frp.new "$OUT/frp"
echo "$DEST" > "$OUT/mudhome"
echo
say "빌드 완료: $OUT/frp (데이터 경로 $DEST)"

#!/bin/bash
# 무한대전 설치
#   ./deploy.sh <설치 디렉터리> <빌드 디렉터리>
#     예) ./deploy.sh /home/olddos/muhan /tmp/muhan-build
#
# - 처음 설치(설치 디렉터리에 rooms 가 없을 때): muhan_data.tgz 를 푼다.
# - 다시 설치: 실행 파일과 스크립트만 바꾼다. 방/몬스터/물건(운영자가 게임 안에서 고칠 수 있음),
#   플레이어, 게시판, 편지, 로그는 그대로 둔다.
#   세계 데이터를 원본으로 되돌리려면: RESET_WORLD=1 ./deploy.sh ...  (rooms, objmon, help 만 다시 풂)
set -e
export LC_ALL=C

say() {
	case "${LANG:-}" in
		*[Ee][Uu][Cc]*|*949*) printf '%s\n' "$*" | iconv -f UTF-8 -t CP949 2>/dev/null || printf '%s\n' "$*" ;;
		*) printf '%s\n' "$*" ;;
	esac
}

SRC=$(cd "$(dirname "$0")" && pwd)
DEST=${1:?설치 디렉터리}
BUILD=${2:?빌드 디렉터리}

[ -x "$BUILD/frp" ] || { say "$BUILD/frp 가 없습니다. 먼저 build.sh 를 실행하세요."; exit 1; }
if [ "$(cat "$BUILD/mudhome" 2>/dev/null)" != "$DEST" ]; then
	say "빌드할 때의 설치 디렉터리($(cat "$BUILD/mudhome" 2>/dev/null))와 다릅니다."
	say "./build.sh $BUILD $DEST 로 다시 빌드하세요."
	exit 1
fi

mkdir -p "$DEST"
if [ ! -d "$DEST/rooms" ]; then
	say "처음 설치: 데이터를 풉니다."
	tar xzf "$SRC/muhan_data.tgz" -C "$DEST"
elif [ -n "$RESET_WORLD" ]; then
	say "세계 데이터(rooms, objmon, help)를 원본으로 되돌립니다."
	rm -rf "$DEST/rooms" "$DEST/objmon" "$DEST/help"
	tar xzf "$SRC/muhan_data.tgz" -C "$DEST" ./rooms ./objmon ./help
fi

# 플레이어 파일은 이름의 첫 글자(초성) 별 폴더에 저장된다: player/가 ... player/하, player/temp
# (원본 저장소에는 빈 폴더가 빠져 있다. 폴더 이름은 EUC-KR)
mkdir -p "$DEST/player" "$DEST/log" "$DEST/post" "$DEST/bin"
for n in '\260\241' '\263\252' '\264\331' '\266\363' '\270\266' '\271\331' '\273\347' \
         '\276\306' '\300\332' '\302\367' '\304\253' '\305\270' '\306\304' '\307\317' 'temp'; do
	mkdir -p "$DEST/player/$(printf "$n")"
done

# 실행 중이어도 바꿀 수 있게 임시 이름으로 복사한 뒤 mv
install_file() {
	cp "$1" "$2.new.$$"
	chmod 755 "$2.new.$$"
	mv -f "$2.new.$$" "$2"
}
install_file "$BUILD/frp" "$DEST/bin/frp"
install_file "$SRC/bin/startmud" "$DEST/bin/startmud"
install_file "$SRC/bin/killmud" "$DEST/bin/killmud"
[ -f "$DEST/bin/port" ] || echo 4100 > "$DEST/bin/port"

say "설치 완료: $DEST (포트 $(cat "$DEST/bin/port"))"
say "실행: cd $DEST/bin && nohup ./startmud > /dev/null 2>&1 &"

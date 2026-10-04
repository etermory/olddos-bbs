#!/bin/bash
# 쥬라기공원 머드 엔진(MudOS v22.2b14) 빌드
#   ./build.sh [빌드할 디렉터리]   (기본: /tmp/jurassic-build)
# 결과: <빌드 디렉터리>/driver/driver
#
# 기본은 64 비트로 빌드한다. (가입/이동/전투/저장까지 확인)
# 필요한 패키지 (CentOS 6): gcc bison
#                (Debian)  : build-essential bison
#
# 문제가 있으면 32 비트로: BITS=32 ./build.sh
#   (CentOS 6 은 glibc-devel.i686 libgcc.i686,
#    Debian 은 gcc-multilib libc6-dev-i386 libcrypt-dev:i386 가 더 필요)
set -e

SRC=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-/tmp/jurassic-build}

rm -rf "$OUT"
mkdir -p "$OUT"
cp -r "$SRC/driver" "$OUT/driver"
cd "$OUT/driver"

# 윈도우에서 받은 경우 줄 끝과 실행 권한 정리
find . -type f \( -name '*.c' -o -name '*.h' -o -name '*.y*' -o -name '*.pre' -o -name 'build.MudOS' -o -name 'local_options' -o -name 'options.h' -o -name 'Makefile*' -o -name 'GNUmakefile*' \) -exec sed -i 's/\r$//' {} +
chmod +x build.MudOS

# 새 gcc 에서도 옛 코드를 받아들이도록 하는 옵션
EXTRA="-fgnu89-inline -fcommon -Wno-implicit-int -Wno-return-type -Wno-int-conversion -Wno-incompatible-pointer-types"
if [ "${BITS:-64}" = 32 ]; then
	EXTRA="-m32 $EXTRA -Wno-implicit-function-declaration"
else
	# 64 비트: 선언 없이 쓴 함수는 int 를 돌려준다고 보아 포인터가 잘리므로 경고를 남긴다
	EXTRA="$EXTRA -Wimplicit-function-declaration"
fi
GCCVER=$(gcc -dumpversion | cut -d. -f1)
if [ "$GCCVER" -ge 14 ]; then
	EXTRA="$EXTRA -fpermissive -std=gnu89"
fi
sed -i "s|^CFLAGS=\"|CFLAGS=\"$EXTRA |" build.MudOS

./build.MudOS

# 요즘 배포판에 없는 옛 라이브러리(-ly, -lnsl)는 빼고 링크
make configure.h
sed -i -E 's/(^| )-ly($| )/\1\2/g; s/(^| )-lnsl($| )/\1\2/g' system_libs
make
echo
echo "빌드 완료: $OUT/driver/driver"

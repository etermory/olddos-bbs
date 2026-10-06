/*
 * han.c - 완성형(EUC-KR) 한글을 한 글자로 다루기 (도스박물관 BBS)
 *
 * pico 는 바이트 하나를 글자 하나로 다룬다. 완성형 한글은 두 바이트(첫 바이트
 * 0x81~0xFE)에 화면 두 칸이라, 바이트 수와 화면 칸 수는 같지만 커서를 한 바이트씩
 * 옮기거나 지우면 글자가 반으로 깨진다. 여기의 함수들은 두 바이트를 한 글자로 본다.
 *
 * forwchar/backchar/forwdel/backdel 은 줄바꿈, 찾기 같은 곳에서 바이트 단위로
 * 쓰이므로 그대로 두고, 키에 묶인 함수만 여기 것으로 바꾼다.
 */

#include	"headers.h"

#define	HAN_LEAD(c)	((c) >= 0x81 && (c) <= 0xFE)


/* 줄 lp 의 o 번째 바이트가 두 바이트 글자의 둘째 바이트인가 */
int
han_is_trail(lp, o)
LINE *lp;
int   o;
{
    int i = 0, len = llength(lp);

    if(o <= 0 || o >= len)
      return(FALSE);

    while(i < o){
	if(HAN_LEAD(lgetc(lp, i).c) && i + 1 < len)
	  i += 2;
	else
	  i++;
    }

    return(i > o);
}


/* o 에서 시작하는 글자의 바이트 수 (1 또는 2) */
int
han_len_at(lp, o)
LINE *lp;
int   o;
{
    if(o + 1 < llength(lp) && HAN_LEAD(lgetc(lp, o).c) && !han_is_trail(lp, o))
      return(2);

    return(1);
}


/* o 바로 앞 글자의 바이트 수 (1 또는 2) */
int
han_len_before(lp, o)
LINE *lp;
int   o;
{
    if(o >= 2 && han_is_trail(lp, o - 1))
      return(2);

    return(1);
}


/* 오프셋이 글자 가운데(둘째 바이트)면 글자 처음으로 */
int
han_align(lp, o)
LINE *lp;
int   o;
{
    return(han_is_trail(lp, o) ? o - 1 : o);
}


/*
 * fillcol 칸 안에 들어가는 마지막 글자 바로 다음 위치.
 * 공백 없이 긴 줄을 글자 단위로 나눌 때 쓴다 (한글을 반으로 자르지 않는다).
 */
int
han_hardbreak(lp)
LINE *lp;
{
    int j = 0, col = 0, len = llength(lp), k, w, c;

    while(j < len){
	c = lgetc(lp, j).c;
	k = (HAN_LEAD(c) && j + 1 < len) ? 2 : 1;
	w = (c == '\t') ? ((col | 0x07) + 1 - col) : k;
	if(col + w > fillcol)
	  break;

	col += w;
	j   += k;
    }

    return(j);
}


/* 줄 처음부터 오프셋 o 까지의 화면 칸 수 (탭 포함) */
int
han_col(lp, o)
LINE *lp;
int   o;
{
    int i, col = 0, c;

    for(i = 0; i < o && i < llength(lp); i++){
	c = lgetc(lp, i).c;
	if(c == '\t')
	  col |= 0x07;
	else if(c < 0x20 || c == 0x7F)
	  ++col;

	++col;
    }

    return(col);
}


/* 문자열(입력 줄) 에서: o 에서 시작하는 글자의 바이트 수 */
int
han_str_at(s, o)
char *s;
int   o;
{
    int i = 0;

    while(i < o && s[i]){
	if(HAN_LEAD((unsigned char) s[i]) && s[i+1])
	  i += 2;
	else
	  i++;
    }

    if(i == o && HAN_LEAD((unsigned char) s[o]) && s[o+1])
      return(2);

    return(1);
}


/* 문자열(입력 줄) 에서: o 바로 앞 글자의 바이트 수 */
int
han_str_before(s, o)
char *s;
int   o;
{
    int i = 0, last = 1;

    while(i < o && s[i]){
	if(HAN_LEAD((unsigned char) s[i]) && s[i+1] && i + 1 < o){
	    last = 2;
	    i += 2;
	}
	else{
	    last = 1;
	    i++;
	}
    }

    return(last);
}


/* 한 글자 앞으로 (^F, 오른쪽 화살표) */
int
forwhchar(f, n)
int f, n;
{
    if(n < 0)
      return(backhchar(f, -n));

    while(n-- > 0){
	int k = 1;

	if(curwp->w_doto < llength(curwp->w_dotp))
	  k = han_len_at(curwp->w_dotp, curwp->w_doto);

	if(forwchar(f, k) == FALSE)
	  return(FALSE);
    }

    return(TRUE);
}


/* 한 글자 뒤로 (^B, 왼쪽 화살표) */
int
backhchar(f, n)
int f, n;
{
    if(n < 0)
      return(forwhchar(f, -n));

    while(n-- > 0){
	int k = 1;

	if(curwp->w_doto > 0)
	  k = han_len_before(curwp->w_dotp, curwp->w_doto);

	if(backchar(f, k) == FALSE)
	  return(FALSE);
    }

    return(TRUE);
}


/* 커서 자리 글자 지우기 (^D, Del) */
int
forwhdel(f, n)
int f, n;
{
    if(n < 0)
      return(backhdel(f, -n));

    while(n-- > 0){
	int k = 1;

	if(curwp->w_doto < llength(curwp->w_dotp))
	  k = han_len_at(curwp->w_dotp, curwp->w_doto);

	if(forwdel(f, k) == FALSE)
	  return(FALSE);
    }

    return(TRUE);
}


/* 커서 앞 글자 지우기 (백스페이스) */
int
backhdel(f, n)
int f, n;
{
    if(n < 0)
      return(forwhdel(f, -n));

    while(n-- > 0){
	int k = 1;

	if(curwp->w_doto > 0)
	  k = han_len_before(curwp->w_dotp, curwp->w_doto);

	if(backdel(f, k) == FALSE)
	  return(FALSE);
    }

    return(TRUE);
}

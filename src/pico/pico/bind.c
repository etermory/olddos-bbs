#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: bind.c,v 4.23 2000/11/09 21:53:43 hubert Exp $";
#endif
/*
 * Program:	Key binding routines
 *
 *
 * Michael Seibel
 * Networks and Distributed Computing
 * Computing and Communications
 * University of Washington
 * Administration Builiding, AG-44
 * Seattle, Washington, 98195, USA
 * Internet: mikes@cac.washington.edu
 *
 * Please address all bugs and comments to "pine-bugs@cac.washington.edu"
 *
 *
 * Pine and Pico are registered trademarks of the University of Washington.
 * No commercial use of these trademarks may be made without prior written
 * permission of the University of Washington.
 * 
 * Pine, Pico, and Pilot software and its included text are Copyright
 * 1989-2000 by the University of Washington.
 * 
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this distribution.
 *
 */
/*	This file is for functions having to do with key bindings,
	descriptions, help commands, and command line execution.

	written 11-feb-86 by Daniel Lawrence
								*/

#include	"headers.h"

#ifdef	ANSI
    int arraylen(char **);
#else
    int arraylen();
#endif

/* 
 * help - help function for pico (UW pared down version of uemacs).
 *	  this function will intentionally garbage with helpful 
 *	  tips and then wait for a ' ' to be hit to then update the 
 *        screen.
 */


static char *helptext[] = {
    "\t스크린 편집기 (pico) 도움말",
    " ",
    "\t글자를 치면 커서 자리에 들어갑니다. 줄이 76 칸을 넘으면 저절로",
    "\t다음 줄로 넘어갑니다 (공백 없는 긴 줄도 나눕니다).",
    "~\t명령은 컨트롤(~C~t~r~l) 키와 함께 누릅니다. ~^~X 는 Ctrl 을 누른 채 X.",
    " ",
    "\t[ 옮기기 ]",
    "~\t~^~F  ~^~B      한 글자 오른쪽 / 왼쪽 (화살표 키도 됩니다)",
    "~\t~^~N  ~^~P      아랫줄 / 윗줄",
    "~\t~^~A  ~^~E      줄 처음 / 줄 끝",
    "~\t~^~V  ~^~Y      다음 쪽 / 앞 쪽",
    "~\t~^~W          글 찾기 (영문 대소문자는 가리지 않음)",
    "~\t~^~C          커서가 몇째 줄에 있는지 보기",
    "~\t~^~L          화면 다시 그리기",
    " ",
    "\t[ 고치기 ]",
    "~\t~^~D          커서 자리 글자 지우기",
    "\t백스페이스  커서 앞 글자 지우기",
    "~\t~^~K          한 줄 잘라내기 (블록을 표시했으면 그 블록)",
    "~\t~^~U          잘라낸 글을 커서 자리에 붙여넣기",
    "~\t~^~^          블록 시작 표시 (다시 누르면 풀림)",
    "\t\t  표시한 뒤 커서를 옮기면 반전되어 보이고, ^K 로 잘라냅니다.",
    "~\t~^~J          문단을 76 칸에 맞춰 다시 정리 (바로 ^U 하면 되돌림)",
    "\t\t  문단은 빈 줄이나 들여쓰기로 나뉩니다.",
    "~\t~^~I          탭 넣기",
    " ",
    "\t[ 마치기 ]",
    "~\t~^~X          저장하고 끝내기 (Y: 저장, N: 저장하지 않음)",
    " ",
    "\tPine 과 Pico 는 워싱턴 대학교(University of Washington)의",
    "\t상표입니다.",
    " ",
    "    도움말 끝.",
    " ",
    NULL
};


/*
 * arraylen - return the number of bytes in an array of char
 */
arraylen(array)
char **array;
{
    register int i=0;

    while(array[i++] != NULL) ;
    return(i);
}


/*
 * whelp - display help text for the composer and pico
 */
whelp(f, n)
     int f, n;
{
    if(term.t_mrow == 0){  /* blank keymenu in effect */
	if(km_popped == 0){
	    /* cause keymenu display */
	    km_popped = 2;
	    if(!Pmaster)
	      sgarbf = TRUE;

	    return(TRUE);
	}
    }

    if(Pmaster){
	VARS_TO_SAVE *saved_state;

	saved_state = save_pico_state();
	(*Pmaster->helper)(Pmaster->composer_help,
			   Pmaster->headents
			     ? "Help on the Pine Composer"
			     : "Help on Signature Editor",
			   1);
	if(saved_state){
	    restore_pico_state(saved_state);
	    free_pico_state(saved_state);
	}

	ttresize();
	picosigs();			/* restore any altered handlers */
	curwp->w_flag |= WFMODE;
	if(km_popped)  /* this will unpop us */
	  curwp->w_flag |= WFHARD;
    }
    else{
	int mrow_was_zero = 0;

	/* always want keyhelp showing during help */
	if(term.t_mrow == 0){
	    mrow_was_zero++;
	    term.t_mrow = 2;
	}

	pico_help(helptext, "스크린 편집기 도움말", 1);
	/* put it back the way it was */
	if(mrow_was_zero)
	  term.t_mrow = 0;
    }

    sgarbf = TRUE;
    return(FALSE);
}

static KEYMENU menu_scroll[] = {
    {NULL, NULL, KS_NONE},		{NULL, NULL, KS_NONE},
    {NULL, NULL, KS_NONE},		{NULL, NULL, KS_NONE},
    {NULL, NULL, KS_NONE},		{NULL, NULL, KS_NONE},
    {"^X", "도움말 끝", KS_NONE},	{NULL, NULL, KS_NONE},
    {NULL, NULL, KS_NONE},		{NULL, NULL, KS_NONE},
    {NULL, NULL, KS_NONE},		{NULL, NULL, KS_NONE}
};
#define	PREV_KEY	3
#define	NEXT_KEY	9


#define	OVERLAP	2		/* displayed page overlap */

/*
 * scrollw - takes beginning row and ending row to diplay an array
 *           of text lines.  returns either 0 if scrolling terminated
 *           normally or the value of a ctrl character typed to end it.
 *
 * updates - 
 *     01/11/89 - added stripe call if 1st char is tilde - '~'
 *
 */
wscrollw(begrow, endrow, textp, textlen)
int	begrow, endrow;
char	*textp[];
int	textlen;
{
    register int	loffset = 0; 
    register int	prevoffset = -1; 
    register int	dlines;
    register int	i;
    register int	cont;
    register int	done = 0;
    register char	*buf;
    int	 c;
     
    dlines = endrow - begrow - 1;
    while(!done) {
        /*
         * diplay a page loop ...
         */
	if(prevoffset != loffset){
       	    for(i = 0; i < dlines; i++){
                movecursor(i + begrow, 0);
                peeol();
                if((loffset+i) < textlen){
		    buf = &(textp[loffset+i][0]);
		    if(*buf == '~'){
			buf++;
			wstripe(begrow+i, 0, buf, '~');
		    }
		    else{
			pputs(buf, 0);
		    }
                }
            }
	    /*
	     * put up the options prompt
	     */
            movecursor(begrow + dlines, 0);
            cont = (loffset+dlines < textlen);
            if(cont){                               /* continue ? */
		menu_scroll[NEXT_KEY].name  = "^V";
		menu_scroll[NEXT_KEY].label = "뒤 쪽";
	    }
	    else
	      menu_scroll[NEXT_KEY].name = NULL;

	    if(loffset){
		menu_scroll[PREV_KEY].name  = "^Y";
		menu_scroll[PREV_KEY].label = "앞 쪽";
	    }
	    else
	      menu_scroll[PREV_KEY].name = NULL;

	    wkeyhelp(menu_scroll);
	}

	(*term.t_flush)();

        c = GetKey();

	prevoffset = loffset;
	switch(c){
	    case  (CTRL|'X') :		/* quit */
 	    case  F2  :
		done = 1;
		break;
	    case  (CTRL|'Y') :		/* prev page */
	    case  F7  :			/* prev page */
		if((loffset-dlines-OVERLAP) > 0){
               	    loffset -= (dlines-OVERLAP);
		}
	        else{
		    if(loffset != 0){
			prevoffset = -1;
		    }
		    else{
		    	(*term.t_beep)();
		    }
		    loffset = 0;
	        }
		break;
	    case  (CTRL|'V') :			/* next page */
 	    case  F8  :
		if(cont){
               	   loffset += (dlines-OVERLAP);
		}
		else{
		   (*term.t_beep)();
		}
		break;
	    case  '\016' :		/* prev-line */
	    case  (CTRL|'N') :
	      if(cont)
		loffset++;
	      else
		(*term.t_beep)();
	      break;
	    case  '\020' :		/* prev-line */
	    case  (CTRL|'P') :
	      if(loffset > 0)
		loffset--;
	      else
		(*term.t_beep)();
	      break;
	    case  '\014' :		/* refresh */
	    case  (CTRL|'L') :		/* refresh */
	        modeline(curwp);
		update();
		prevoffset = -1;
		break;
	    case  NODATA :
	        break;
	    default :
		emlwrite("모르는 명령입니다.", NULL);
		(*term.t_beep)();
		break;
	}
    }
    return(TRUE);
}


/*
 * normalize_cmd - given a char and list of function key to command key
 *		   mappings, return, depending on gmode, the right command.
 *		   The list is an array of (Fkey, command-key) pairs.
 *		   sc is the index in the array that means to ignore fkey
 *		   vs. command key mapping
 *
 *          rules:  1. if c not in table (either fkey or command), let it thru
 *                  2. if c matches, but in other mode, punt it
 */
normalize_cmd(c, list, sc)
int c, sc;
int list[][2];
{
    register int i;

    for(i=0; i < 12; i++){
	if(c == list[i][(FUNC&c) ? 0 : 1]){	/* in table? */
	    if(i == sc)				/* SPECIAL CASE! */
	      return(list[i][1]);

	    if(list[i][1] == NODATA)		/* no mapping ! */
	      return(c);

	    if(((FUNC&c) == FUNC) ^ ((gmode&MDFKEY) == MDFKEY))
	      return(BADESC);			/* keystroke not allowed! */
	    else
	      return(list[i][1]);		/* never return func keys */
	}
    }

#ifdef _WINDOWS
    /*
     * Menu's get alpha key commands assigned to them.  Well, if we are in PF
     * key mode then the alpha key commands don't work.  So, we flag them with
     * a bit (MENU defined in estruct.h) so we can recognize them as different
     * from a keyboard press.  Here we clear that flag so the command can be
     * processed.
     */
    c &= ~MENU;
#endif

    return(c);
}


/*
 * rebind - replace the first function with the second
 */
void
rebindfunc(a, b)
    int (*a) PROTO((int, int)), (*b) PROTO((int, int));
{
    KEYTAB *kp;

    kp = (Pmaster) ? &keytab[0] : &pkeytab[0];

    while(kp->k_fp != NULL){		/* go thru whole list, and */
	if(kp->k_fp == a)
	  kp->k_fp = b;			/* replace all occurances */
	kp++;
    }
}


/*
 * bindtokey - bind function f to command c
 */
bindtokey(c, f)
int c, (*f) PROTO((int, int));
{
    KEYTAB *kp, *ktab = (Pmaster) ? &keytab[0] : &pkeytab[0];

    for(kp = ktab; kp->k_fp; kp++)
      if(kp->k_code == c){
	  kp->k_fp = f;			/* set to new function */
	  break;
      }

    /* not found? create new binding */
    if(!kp->k_fp && kp < &ktab[NBINDS]){
	kp->k_code     = c;		/* assign new code and function */
	kp->k_fp       = f;
	(++kp)->k_code = 0;		/* and null out next slot */
	kp->k_fp       = NULL;
    }

    return(TRUE);
}

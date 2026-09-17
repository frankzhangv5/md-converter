%{
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.tab.h"
#include "html.h"

#define YY_USER_INIT BEGIN(LINE_START);

static int fence_len;
static char fence_delim;
static int tick_len;
static int saved_state;
static int eof_newline_sent;
static int eof_fence_closed;

static char *sbuf;
static size_t sbuf_len;
static size_t sbuf_cap;

static void die_lex(const char *msg)
{
    fprintf(stderr, "md-convert: %s\n", msg);
    exit(1);
}

static char *sdup(const char *s)
{
    char *p = strdup(s ? s : "");
    if (!p)
        die_lex("out of memory");
    return p;
}

static void sbuf_clear(void)
{
    sbuf_len = 0;
    if (sbuf)
        sbuf[0] = '\0';
}

static void sbuf_add(const char *s, size_t n)
{
    if (sbuf_len + n + 1 > sbuf_cap) {
        size_t cap = sbuf_cap ? sbuf_cap : 64;
        char *p;
        while (cap < sbuf_len + n + 1)
            cap *= 2;
        p = (char *)realloc(sbuf, cap);
        if (!p)
            die_lex("out of memory");
        sbuf = p;
        sbuf_cap = cap;
    }
    memcpy(sbuf + sbuf_len, s, n);
    sbuf_len += n;
    sbuf[sbuf_len] = '\0';
}

static char *sbuf_take(void)
{
    char *p = sdup(sbuf ? sbuf : "");
    sbuf_clear();
    return p;
}

static int count_char(const char *s, char c)
{
    int n = 0;
    while (*s == ' ' || *s == '\t')
        s++;
    while (*s == c) {
        n++;
        s++;
    }
    return n;
}

static char *parse_fence_info(const char *s)
{
    const char *p = s;
    char delim;
    while (*p == ' ' || *p == '\t')
        p++;
    delim = *p;
    while (*p == delim)
        p++;
    while (*p == ' ' || *p == '\t')
        p++;
    {
        const char *end = p;
        while (*end && *end != '\r' && *end != '\n')
            end++;
        while (end > p && (end[-1] == ' ' || end[-1] == '\t'))
            end--;
        {
            size_t n = (size_t)(end - p);
            char *info = (char *)malloc(n + 1);
            if (!info)
                die_lex("out of memory");
            memcpy(info, p, n);
            info[n] = '\0';
            return info;
        }
    }
}

static char *parse_aligns(const char *s)
{
    char out[64];
    int n = 0;
    const char *p = s;
    while (*p) {
        int left = 0, right = 0;
        while (*p && strchr(" \t|\r\n", *p))
            p++;
        if (!*p)
            break;
        if (*p == ':') {
            left = 1;
            p++;
        }
        if (*p != '-')
            break;
        while (*p == '-')
            p++;
        if (*p == ':') {
            right = 1;
            p++;
        }
        if (n < 63)
            out[n++] = (left && right) ? 'C' : (right ? 'R' : 'L');
    }
    out[n] = '\0';
    return sdup(out);
}

static int atx_level(const char *s)
{
    int n = 0;
    while (*s == ' ' || *s == '\t')
        s++;
    while (*s == '#') {
        n++;
        s++;
    }
    return n;
}

static int line_ahead_has(const char *pat);
%}

%option noyywrap yylineno nounistd never-interactive

%x LINE_START
%x INLINE
%x FENCE
%x IN_TICKS
%x AFTER_LIST
%x TABLE

/* ---- named patterns (readable constants for rule LHS) ---- */
WS              [ \t]
NL              \r?\n
INDENT          {WS}{0,3}
INDENT1_3       {WS}{1,3}
SPACES          {WS}*

TICKS           `+
TICKS3          `{3,}
TILDES3         ~{3,}
FENCE_TICKS     {INDENT}{TICKS3}{SPACES}[^\r\n`]*{NL}
FENCE_TILDES    {INDENT}{TILDES3}{SPACES}[^\r\n]*{NL}
FENCE_CLOSE_T   {INDENT}{TICKS3}{SPACES}{NL}
FENCE_CLOSE_N   {INDENT}{TILDES3}{SPACES}{NL}

ALIGN_CELL      {SPACES}:?-+:?{SPACES}
TABLE_SEP_LINE  {INDENT}\|{ALIGN_CELL}(\|{ALIGN_CELL})+\|?{SPACES}{NL}

HR_STAR         {INDENT}(\*{WS}*){3,}{SPACES}{NL}
HR_DASH         {INDENT}(-{WS}*){3,}{SPACES}{NL}
HR_UNDER        {INDENT}(_{WS}*){3,}{SPACES}{NL}

ATX_MARK        #{1,6}
ATX_SPACED      {INDENT}{ATX_MARK}{WS}+
ATX_HASHES      {INDENT}{ATX_MARK}

BLOCKQUOTE_MARK {INDENT}>{WS}?
UL_MARK_PAT     {INDENT}[-*+]{WS}+
OL_MARK_PAT     {INDENT}[0-9]{1,9}[.)]{WS}+

TASK_UNCHECKED  {SPACES}"["[ ]"]"{WS}+
TASK_CHECKED    {SPACES}"["[xX]"]"{WS}+

HARD_BREAK_SP   [ ]{2,}{NL}
HARD_BREAK_ESC  \\{NL}
ASCII_PUNCT     [\x21-\x2f\x3a-\x40\x5b-\x60\x7b-\x7e]
ESCAPE_SEQ      \\{ASCII_PUNCT}

WORD_UNDER      [a-zA-Z0-9]+(_[a-zA-Z0-9]+)+
AUTOLINK_ANGLE  "<"https?:\/\/[^>\r\n]+">"
AUTOLINK_BARE   https?:\/\/[a-zA-Z0-9._~:/?#@!$&'*+,;=%-]+
PLAIN_TEXT      [^\r\n\\`*_~\[\]()!<>| ]+

%%

<LINE_START>{SPACES}{NL}               { return BLANK; }

<LINE_START>{FENCE_TICKS} {
    fence_delim = '`';
    fence_len = count_char(yytext, '`');
    yylval.fence.delim = '`';
    yylval.fence.len = fence_len;
    yylval.fence.info = parse_fence_info(yytext);
    BEGIN(FENCE);
    return FENCE_OPEN;
}

<LINE_START>{FENCE_TILDES} {
    fence_delim = '~';
    fence_len = count_char(yytext, '~');
    yylval.fence.delim = '~';
    yylval.fence.len = fence_len;
    yylval.fence.info = parse_fence_info(yytext);
    BEGIN(FENCE);
    return FENCE_OPEN;
}

<LINE_START>{TABLE_SEP_LINE} {
    yylval.str = parse_aligns(yytext);
    return TABLE_SEP;
}

<LINE_START>{HR_STAR}                  { return HR; }
<LINE_START>{HR_DASH}                  { return HR; }
<LINE_START>{HR_UNDER}                 { return HR; }

<LINE_START>{ATX_SPACED} {
    yylval.num = atx_level(yytext);
    BEGIN(INLINE);
    return ATX;
}

<LINE_START>{ATX_HASHES}/{NL} {
    yylval.num = atx_level(yytext);
    BEGIN(INLINE);
    return ATX;
}

<LINE_START>{BLOCKQUOTE_MARK}          { return BLOCKQUOTE; }

<LINE_START>{UL_MARK_PAT} {
    BEGIN(AFTER_LIST);
    return UL_MARK;
}

<LINE_START>{OL_MARK_PAT} {
    BEGIN(AFTER_LIST);
    return OL_MARK;
}

<LINE_START>{INDENT}/\|                { BEGIN(TABLE); }

<LINE_START>{INDENT1_3}                { /* skip up to 3 spaces */ }

<LINE_START>.                          { yyless(0); BEGIN(INLINE); }

<AFTER_LIST>{TASK_UNCHECKED} {
    yylval.num = 0;
    BEGIN(INLINE);
    return TASK_MARK;
}

<AFTER_LIST>{TASK_CHECKED} {
    yylval.num = 1;
    BEGIN(INLINE);
    return TASK_MARK;
}

<AFTER_LIST>.|\n                       { yyless(0); BEGIN(INLINE); }

<FENCE>{FENCE_CLOSE_T} {
    if (fence_delim == '`' && count_char(yytext, '`') >= fence_len) {
        BEGIN(LINE_START);
        return FENCE_CLOSE;
    }
    yylval.str = sdup(yytext);
    {
        size_t n = strlen(yylval.str);
        while (n && (yylval.str[n - 1] == '\n' || yylval.str[n - 1] == '\r'))
            yylval.str[--n] = '\0';
    }
    return FENCE_LINE;
}

<FENCE>{FENCE_CLOSE_N} {
    if (fence_delim == '~' && count_char(yytext, '~') >= fence_len) {
        BEGIN(LINE_START);
        return FENCE_CLOSE;
    }
    yylval.str = sdup(yytext);
    {
        size_t n = strlen(yylval.str);
        while (n && (yylval.str[n - 1] == '\n' || yylval.str[n - 1] == '\r'))
            yylval.str[--n] = '\0';
    }
    return FENCE_LINE;
}

<FENCE>.*{NL} {
    yylval.str = sdup(yytext);
    {
        size_t n = strlen(yylval.str);
        while (n && (yylval.str[n - 1] == '\n' || yylval.str[n - 1] == '\r'))
            yylval.str[--n] = '\0';
    }
    return FENCE_LINE;
}

<FENCE>.* {
    yylval.str = sdup(yytext);
    return FENCE_LINE;
}

<FENCE><<EOF>> {
    if (!eof_fence_closed) {
        eof_fence_closed = 1;
        BEGIN(LINE_START);
        return FENCE_CLOSE;
    }
    return 0;
}

<INLINE,TABLE>{HARD_BREAK_SP} {
    BEGIN(LINE_START);
    return HARD_BREAK;
}

<INLINE,TABLE>{HARD_BREAK_ESC} {
    BEGIN(LINE_START);
    return HARD_BREAK;
}

<INLINE,TABLE>[ ] {
    yylval.str = sdup(" ");
    return TEXT;
}

<INLINE,TABLE>{NL} {
    BEGIN(LINE_START);
    return NEWLINE;
}

<INLINE,TABLE>{ESCAPE_SEQ} {
    char tmp[2] = { yytext[1], '\0' };
    yylval.str = sdup(tmp);
    return ESCAPE;
}

<INLINE,TABLE>"***"                    { return STAR3; }
<INLINE,TABLE>"**"                     { return STAR2; }
<INLINE,TABLE>"*"                      { return STAR; }
<INLINE,TABLE>"__"                     { return UNDER2; }
<INLINE,TABLE>{WORD_UNDER} {
    yylval.str = sdup(yytext);
    return TEXT;
}
<INLINE,TABLE>"_"                      { return UNDER; }
<INLINE,TABLE>"~~"                     { return TILDE2; }

<INLINE,TABLE>{TICKS} {
    tick_len = yyleng;
    saved_state = YY_START;
    sbuf_clear();
    BEGIN(IN_TICKS);
}

<INLINE,TABLE>"!["                     { yyless(1); return BANG; }
<INLINE,TABLE>"[" {
    if (line_ahead_has("]("))
        return LBRACK;
    yylval.str = sdup("[");
    return TEXT;
}
<INLINE,TABLE>"]"                      { return RBRACK; }
<INLINE,TABLE>"("                      { return LPAREN; }
<INLINE,TABLE>")"                      { return RPAREN; }
<INLINE,TABLE>{AUTOLINK_ANGLE} {
    yylval.str = sdup(yytext + 1);
    yylval.str[strlen(yylval.str) - 1] = '\0';
    return AUTOLINK;
}
<INLINE,TABLE>{AUTOLINK_BARE} {
    yylval.str = sdup(yytext);
    return AUTOLINK;
}
<INLINE,TABLE>"<"                      { return LT; }
<INLINE,TABLE>">"                      { return GT; }

<TABLE>"|"                             { return PIPE; }

<INLINE,TABLE>{PLAIN_TEXT} {
    yylval.str = sdup(yytext);
    return TEXT;
}

<INLINE>"|" {
    yylval.str = sdup("|");
    return TEXT;
}

<INLINE,TABLE>. {
    char tmp[2] = { yytext[0], '\0' };
    yylval.str = sdup(tmp);
    return TEXT;
}

<IN_TICKS>{TICKS} {
    if (yyleng == tick_len) {
        yylval.str = sbuf_take();
        BEGIN(saved_state);
        return CODE_SPAN;
    }
    sbuf_add(yytext, (size_t)yyleng);
}

<IN_TICKS>{NL}                         { sbuf_add(" ", 1); }

<IN_TICKS>.                            { sbuf_add(yytext, 1); }

<IN_TICKS><<EOF>> {
    char *ticks = (char *)malloc((size_t)tick_len + 1);
    int i;
    if (!ticks)
        die_lex("out of memory");
    for (i = 0; i < tick_len; i++)
        ticks[i] = '`';
    ticks[tick_len] = '\0';
    yylval.str = ticks;
    {
        char *rest = sbuf_take();
        yylval.str = (char *)realloc(yylval.str, strlen(ticks) + strlen(rest) + 1);
        if (!yylval.str)
            die_lex("out of memory");
        strcat(yylval.str, rest);
        free(rest);
    }
    BEGIN(LINE_START);
    return TEXT;
}

<INLINE,TABLE><<EOF>> {
    if (!eof_newline_sent) {
        eof_newline_sent = 1;
        BEGIN(LINE_START);
        return NEWLINE;
    }
    return 0;
}

<LINE_START><<EOF>>                    { return 0; }

%%

static int line_ahead_has(const char *pat)
{
    char buf[8192];
    int n = 0;
    int ch;
    int found;
    while (n < (int)sizeof(buf) - 1) {
        ch = input();
        if (ch == EOF)
            break;
        buf[n++] = (char)ch;
        if (ch == '\n')
            break;
    }
    buf[n] = '\0';
    found = strstr(buf, pat) != NULL;
    while (n > 0)
        unput((unsigned char)buf[--n]);
    return found;
}

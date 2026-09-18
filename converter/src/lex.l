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
/* After UL_MARK/OL_MARK: indented (1–3 spaces) lines are list continuation. */
static int after_list;
/* Last char of previous inline token; used for word-internal '_' flanking. */
static int prev_char = '\n';
/* Unmatched inline delimiter opens on the current line (closer bypasses lookahead). */
static int star_open;
static int star2_open;
static int star3_open;
static int under_open;
static int under2_open;
static int tilde2_open;

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
static void note_text(const char *s);
static int word_char(int c);
static int take_delim(int *open_flag, const char *lit, const char *close_pat, int tok);
static void reset_inline_delims(void);

/* Strip one indent level (4 spaces or 1 tab) and trailing CR/LF. */
static char *indent_code_text(const char *s)
{
    const char *p = s;
    size_t n;
    char *out;
    if (*p == '\t')
        p++;
    else {
        int i;
        for (i = 0; i < 4 && *p == ' '; i++)
            p++;
    }
    n = strlen(p);
    while (n && (p[n - 1] == '\n' || p[n - 1] == '\r'))
        n--;
    out = (char *)malloc(n + 1);
    if (!out)
        die_lex("out of memory");
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}
%}

%option noyywrap yylineno nounistd never-interactive

%x LINE_START
%x INLINE
%x FENCE
%x INDENT_CODE
%x IN_TICKS
%x AFTER_LIST
%x TABLE

/* ---- named patterns (readable constants for rule LHS) ---- */
WS              [ \t]
NL              \r?\n
INDENT          {WS}{0,3}
INDENT1_3       {WS}{1,3}
INDENT4         ([ ]{4}|\t)
SPACES          {WS}*
IC_LINE         {INDENT4}[^\r\n]*{NL}
IC_EOF          {INDENT4}[^\r\n]*

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

/* Snake_case / path underscores → TEXT; keep _em_ after CJK/punct working. */
WORD_UNDER      [a-zA-Z0-9./+-]+(_[a-zA-Z0-9./+-]+)+
AUTOLINK_ANGLE  "<"https?:\/\/[^>\r\n]+">"
AUTOLINK_BARE   https?:\/\/[a-zA-Z0-9._~:/?#@!$&'*+,;=%-]+
PLAIN_TEXT      [^\r\n\\`*_~\[\]()!<>| ]+

%%

<LINE_START>{SPACES}{NL}               { after_list = 0; return BLANK; }

<LINE_START>{FENCE_TICKS} {
    after_list = 0;
    fence_delim = '`';
    fence_len = count_char(yytext, '`');
    yylval.fence.delim = '`';
    yylval.fence.len = fence_len;
    yylval.fence.info = parse_fence_info(yytext);
    BEGIN(FENCE);
    return FENCE_OPEN;
}

<LINE_START>{FENCE_TILDES} {
    after_list = 0;
    fence_delim = '~';
    fence_len = count_char(yytext, '~');
    yylval.fence.delim = '~';
    yylval.fence.len = fence_len;
    yylval.fence.info = parse_fence_info(yytext);
    BEGIN(FENCE);
    return FENCE_OPEN;
}

<LINE_START>{TABLE_SEP_LINE} {
    after_list = 0;
    yylval.str = parse_aligns(yytext);
    return TABLE_SEP;
}

<LINE_START>{HR_STAR}                  { after_list = 0; return HR; }
<LINE_START>{HR_DASH}                  { after_list = 0; return HR; }
<LINE_START>{HR_UNDER}                 { after_list = 0; return HR; }

<LINE_START>{ATX_SPACED} {
    after_list = 0;
    yylval.num = atx_level(yytext);
    BEGIN(INLINE);
    return ATX;
}

<LINE_START>{ATX_HASHES}/{NL} {
    after_list = 0;
    yylval.num = atx_level(yytext);
    BEGIN(INLINE);
    return ATX;
}

<LINE_START>{BLOCKQUOTE_MARK}          { after_list = 0; return BLOCKQUOTE; }

<LINE_START>{UL_MARK_PAT} {
    after_list = 1;
    BEGIN(AFTER_LIST);
    return UL_MARK;
}

<LINE_START>{OL_MARK_PAT} {
    after_list = 1;
    BEGIN(AFTER_LIST);
    return OL_MARK;
}

<LINE_START>{INDENT}/\|                { after_list = 0; BEGIN(TABLE); }

<LINE_START>{IC_LINE} {
    after_list = 0;
    yylval.str = indent_code_text(yytext);
    BEGIN(INDENT_CODE);
    return INDENT_CODE_LINE;
}

<LINE_START>{IC_EOF} {
    after_list = 0;
    yylval.str = indent_code_text(yytext);
    BEGIN(INDENT_CODE);
    return INDENT_CODE_LINE;
}

<LINE_START>{INDENT1_3} {
    if (after_list) {
        BEGIN(INLINE);
        return LIST_CONT;
    }
    /* skip up to 3 spaces */
}

<LINE_START>.                          { yyless(0); after_list = 0; BEGIN(INLINE); }

<INDENT_CODE>{IC_LINE} {
    yylval.str = indent_code_text(yytext);
    return INDENT_CODE_LINE;
}

<INDENT_CODE>{IC_EOF} {
    yylval.str = indent_code_text(yytext);
    return INDENT_CODE_LINE;
}

<INDENT_CODE>{SPACES}{NL}/[ ]{4} {
    yylval.str = sdup("");
    return INDENT_CODE_LINE;
}

<INDENT_CODE>{SPACES}{NL}/\t {
    yylval.str = sdup("");
    return INDENT_CODE_LINE;
}

<INDENT_CODE>{SPACES}{NL} {
    BEGIN(LINE_START);
    yyless(0);
}

<INDENT_CODE>. {
    BEGIN(LINE_START);
    yyless(0);
}

<INDENT_CODE><<EOF>>                   { BEGIN(LINE_START); return 0; }

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
    reset_inline_delims();
    prev_char = '\n';
    BEGIN(LINE_START);
    return HARD_BREAK;
}

<INLINE,TABLE>{HARD_BREAK_ESC} {
    reset_inline_delims();
    prev_char = '\n';
    BEGIN(LINE_START);
    return HARD_BREAK;
}

<INLINE,TABLE>[ ] {
    yylval.str = sdup(" ");
    prev_char = ' ';
    return TEXT;
}

<INLINE,TABLE>{NL} {
    reset_inline_delims();
    prev_char = '\n';
    BEGIN(LINE_START);
    return NEWLINE;
}

<INLINE,TABLE>{ESCAPE_SEQ} {
    char tmp[2] = { yytext[1], '\0' };
    yylval.str = sdup(tmp);
    prev_char = (unsigned char)tmp[0];
    return ESCAPE;
}

<INLINE,TABLE>"***"                    { return take_delim(&star3_open, "***", "***", STAR3); }
<INLINE,TABLE>"**"                     { return take_delim(&star2_open, "**", "**", STAR2); }
<INLINE,TABLE>"*" {
    /* Glob-like md-* / html_*: do not open em across the line. */
    if (!star_open && (word_char(prev_char) || prev_char == '-' || prev_char == '_')) {
        yylval.str = sdup("*");
        note_text("*");
        return TEXT;
    }
    return take_delim(&star_open, "*", "*", STAR);
}
<INLINE,TABLE>"__" {
    int nxt = input();
    if (nxt != EOF)
        unput((unsigned char)nxt);
    if (word_char(prev_char) && word_char(nxt)) {
        yylval.str = sdup("__");
        prev_char = '_';
        return TEXT;
    }
    return take_delim(&under2_open, "__", "__", UNDER2);
}
<INLINE,TABLE>{WORD_UNDER} {
    yylval.str = sdup(yytext);
    note_text(yytext);
    return TEXT;
}
<INLINE,TABLE>"_" {
    int nxt = input();
    if (nxt != EOF)
        unput((unsigned char)nxt);
    /* snake_case after CJK/punct: PLAIN_TEXT may stop before '_'. */
    if (word_char(prev_char) && word_char(nxt)) {
        yylval.str = sdup("_");
        prev_char = '_';
        return TEXT;
    }
    return take_delim(&under_open, "_", "_", UNDER);
}
<INLINE,TABLE>"~~" {
    int nxt = input();
    if (nxt != EOF)
        unput((unsigned char)nxt);
    /* ~~~ is fence-ish literal in prose, not strike opener. */
    if (!tilde2_open && nxt == '~') {
        yylval.str = sdup("~~");
        note_text("~~");
        return TEXT;
    }
    return take_delim(&tilde2_open, "~~", "~~", TILDE2);
}

<INLINE,TABLE>{TICKS} {
    tick_len = yyleng;
    saved_state = YY_START;
    sbuf_clear();
    BEGIN(IN_TICKS);
}

<INLINE,TABLE>"!["                     { yyless(1); prev_char = '!'; return BANG; }
<INLINE,TABLE>"[" {
    if (line_ahead_has("](")) {
        prev_char = '[';
        return LBRACK;
    }
    yylval.str = sdup("[");
    prev_char = '[';
    return TEXT;
}
<INLINE,TABLE>"]"                      { prev_char = ']'; return RBRACK; }
<INLINE,TABLE>"("                      { prev_char = '('; return LPAREN; }
<INLINE,TABLE>")"                      { prev_char = ')'; return RPAREN; }
<INLINE,TABLE>{AUTOLINK_ANGLE} {
    yylval.str = sdup(yytext + 1);
    yylval.str[strlen(yylval.str) - 1] = '\0';
    note_text(yylval.str);
    return AUTOLINK;
}
<INLINE,TABLE>{AUTOLINK_BARE} {
    yylval.str = sdup(yytext);
    note_text(yytext);
    return AUTOLINK;
}
<INLINE,TABLE>"<"                      { prev_char = '<'; return LT; }
<INLINE,TABLE>">"                      { prev_char = '>'; return GT; }

<TABLE>"|"                             { prev_char = '|'; return PIPE; }

<INLINE,TABLE>{PLAIN_TEXT} {
    yylval.str = sdup(yytext);
    note_text(yytext);
    return TEXT;
}

<INLINE>"|" {
    yylval.str = sdup("|");
    prev_char = '|';
    return TEXT;
}

<INLINE,TABLE>. {
    char tmp[2] = { yytext[0], '\0' };
    yylval.str = sdup(tmp);
    prev_char = (unsigned char)yytext[0];
    return TEXT;
}

<IN_TICKS>{TICKS} {
    if (yyleng == tick_len) {
        yylval.str = sbuf_take();
        note_text(yylval.str);
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
        prev_char = '\n';
        BEGIN(LINE_START);
        return NEWLINE;
    }
    return 0;
}

<LINE_START><<EOF>>                    { return 0; }

%%

/* Reset start condition and EOF helpers so yyparse can run on another file. */
void lex_reset(void)
{
    BEGIN(LINE_START);
    fence_len = 0;
    fence_delim = 0;
    tick_len = 0;
    saved_state = 0;
    eof_newline_sent = 0;
    eof_fence_closed = 0;
    after_list = 0;
    prev_char = '\n';
    reset_inline_delims();
    sbuf_clear();
    yylineno = 1;
}

static void note_text(const char *s)
{
    if (s && s[0])
        prev_char = (unsigned char)s[strlen(s) - 1];
    else
        prev_char = '\n';
}

static int word_char(int c)
{
    return c != EOF && (isalnum((unsigned char)c) || c == '_');
}

static void reset_inline_delims(void)
{
    star_open = star2_open = star3_open = 0;
    under_open = under2_open = tilde2_open = 0;
}

static int take_delim(int *open_flag, const char *lit, const char *close_pat, int tok)
{
    if (*open_flag) {
        *open_flag = 0;
        prev_char = (unsigned char)lit[strlen(lit) - 1];
        return tok;
    }
    if (line_ahead_has(close_pat)) {
        *open_flag = 1;
        prev_char = (unsigned char)lit[0];
        return tok;
    }
    yylval.str = sdup(lit);
    note_text(lit);
    return TEXT;
}

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

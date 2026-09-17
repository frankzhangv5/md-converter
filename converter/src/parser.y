%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "html.h"

char *g_body = NULL;
int g_parse_ok = 1;

int yylex(void);
void yyerror(const char *s);

extern int yylineno;
%}

%code requires {
    #include "html.h"
}

%define parse.error verbose

/* Paragraph continuation vs next block; indent code line+; link ']' vs atom. Default shift is correct. */
%expect 22

%union {
    char *str;
    int num;
    Fence fence;
}

%token NEWLINE BLANK HARD_BREAK
%token <str> TEXT ESCAPE AUTOLINK FENCE_LINE INDENT_CODE_LINE CODE_SPAN TABLE_SEP
%token <num> ATX TASK_MARK
%token <fence> FENCE_OPEN
%token FENCE_CLOSE
%token BLOCKQUOTE UL_MARK OL_MARK HR
%token PIPE
%token STAR STAR2 STAR3 UNDER UNDER2 TILDE2
%token LBRACK RBRACK LPAREN RPAREN BANG LT GT

%type <str> document blocks block
%type <str> heading paragraph para_body
%type <str> ul_list ul_items ul_item
%type <str> ol_list ol_items ol_item
%type <str> blockquote bq_lines bq_line
%type <str> fence fence_body indent_code indent_code_lines
%type <str> table table_rows table_row cells opt_inlines
%type <str> inlines piece atoms atom
%type <str> strong em strike star3 link image dest

%start document

%%

document
    : blocks { g_body = $1; }
    ;

blocks
    : %empty            { $$ = str_dup(""); }
    | blocks block {
        if ($1[0] && $2[0])
            $$ = str_concat_sep($1, "\n", $2);
        else if ($2[0]) {
            free($1);
            $$ = $2;
        } else {
            free($2);
            $$ = $1;
        }
      }
    ;

block
    : heading
    | paragraph
    | ul_list
    | ol_list
    | blockquote
    | fence
    | indent_code
    | table
    | HR                { $$ = html_void("hr", NULL); }
    | BLANK             { $$ = str_dup(""); }
    ;

heading
    : ATX inlines NEWLINE { $$ = html_heading($1, $2); free($2); }
    | ATX NEWLINE         { $$ = html_heading($1, ""); }
    ;

paragraph
    : para_body NEWLINE { $$ = html_tag("p", $1); free($1); }
    ;

para_body
    : inlines
    | para_body NEWLINE inlines {
        $$ = str_concat_sep($1, " ", $3);
      }
    | para_body HARD_BREAK inlines {
        $$ = str_concat(str_concat($1, html_void("br", NULL)), $3);
      }
    ;

ul_list
    : ul_items { $$ = html_tag("ul", $1); free($1); }
    ;

ul_items
    : ul_item
    | ul_items ul_item { $$ = str_concat($1, $2); }
    ;

ul_item
    : UL_MARK inlines NEWLINE {
        $$ = html_tag("li", $2);
        free($2);
      }
    | UL_MARK TASK_MARK inlines NEWLINE {
        char *inner = str_concat_sep(html_checkbox($2), " ", $3);
        $$ = html_tag("task_item", inner);
        free(inner);
      }
    | UL_MARK NEWLINE {
        $$ = html_tag("li", "");
      }
    ;

ol_list
    : ol_items { $$ = html_tag("ol", $1); free($1); }
    ;

ol_items
    : ol_item
    | ol_items ol_item { $$ = str_concat($1, $2); }
    ;

ol_item
    : OL_MARK inlines NEWLINE {
        $$ = html_tag("li", $2);
        free($2);
      }
    | OL_MARK TASK_MARK inlines NEWLINE {
        char *inner = str_concat_sep(html_checkbox($2), " ", $3);
        $$ = html_tag("task_item", inner);
        free(inner);
      }
    | OL_MARK NEWLINE {
        $$ = html_tag("li", "");
      }
    ;

blockquote
    : bq_lines { $$ = html_tag("blockquote", $1); free($1); }
    ;

bq_prefix
    : BLOCKQUOTE
    | bq_prefix BLOCKQUOTE
    ;

bq_lines
    : bq_line
    | bq_lines bq_line { $$ = str_concat($1, $2); }
    ;

bq_line
    : bq_prefix inlines NEWLINE {
        $$ = html_tag("p", $2);
        free($2);
      }
    | bq_prefix ATX inlines NEWLINE {
        $$ = html_heading($2, $3);
        free($3);
      }
    | bq_prefix ATX NEWLINE {
        $$ = html_heading($2, "");
      }
    | bq_prefix UL_MARK inlines NEWLINE {
        char *li = html_tag("li", $3);
        $$ = html_tag("ul", li);
        free($3);
        free(li);
      }
    | bq_prefix NEWLINE { $$ = str_dup(""); }
    ;

fence
    : FENCE_OPEN fence_body FENCE_CLOSE {
        char *esc = html_escape($2);
        $$ = html_fence($1.info, esc);
        free($1.info);
        free($2);
        free(esc);
      }
    ;

fence_body
    : %empty                { $$ = str_dup(""); }
    | fence_body FENCE_LINE {
        if ($1[0])
            $$ = str_concat_sep($1, "\n", $2);
        else {
            free($1);
            $$ = $2;
        }
      }
    ;

indent_code
    : indent_code_lines {
        char *esc = html_escape($1);
        $$ = html_fence("", esc);
        free($1);
        free(esc);
      }
    ;

indent_code_lines
    : INDENT_CODE_LINE
    | indent_code_lines INDENT_CODE_LINE {
        $$ = str_concat_sep($1, "\n", $2);
      }
    ;

table
    : table_row TABLE_SEP table_rows {
        $$ = html_table($1, $2, $3);
        free($2);
      }
    ;

table_rows
    : %empty { $$ = str_dup(""); }
    | table_rows table_row {
        if ($1[0])
            $$ = str_concat_sep($1, "\x1e", $2);
        else {
            free($1);
            $$ = $2;
        }
      }
    ;

table_row
    : PIPE cells NEWLINE { $$ = $2; }
    ;

cells
    : opt_inlines
    | cells PIPE opt_inlines {
        $$ = str_concat_sep($1, "\x1f", $3);
      }
    ;

opt_inlines
    : %empty { $$ = str_dup(""); }
    | inlines
    ;

inlines
    : piece
    | inlines piece { $$ = str_concat($1, $2); }
    ;

piece
    : atom
    | strong
    | em
    | strike
    | star3
    | link
    | image
    ;

atoms
    : atom
    | atoms atom { $$ = str_concat($1, $2); }
    ;

atom
    : TEXT      { $$ = html_escape($1); free($1); }
    | ESCAPE    { $$ = html_escape($1); free($1); }
    | AUTOLINK  { $$ = html_link($1, $1); free($1); }
    | CODE_SPAN {
        char *esc = html_escape($1);
        $$ = html_tag("code", esc);
        free($1);
        free(esc);
      }
    | LT        { $$ = html_escape("<"); }
    | GT        { $$ = html_escape(">"); }
    | LPAREN    { $$ = html_escape("("); }
    | RPAREN    { $$ = html_escape(")"); }
    | BANG      { $$ = html_escape("!"); }
    ;

strong
    : STAR2 atoms STAR2 { $$ = html_tag("strong", $2); free($2); }
    | UNDER2 atoms UNDER2 { $$ = html_tag("strong", $2); free($2); }
    ;

em
    : STAR atoms STAR { $$ = html_tag("em", $2); free($2); }
    | UNDER atoms UNDER { $$ = html_tag("em", $2); free($2); }
    ;

strike
    : TILDE2 atoms TILDE2 { $$ = html_tag("del", $2); free($2); }
    ;

star3
    : STAR3 atoms STAR3 {
        char *inner = html_tag("em", $2);
        $$ = html_tag("strong", inner);
        free($2);
        free(inner);
      }
    ;

link
    : LBRACK atoms RBRACK LPAREN dest RPAREN {
        $$ = html_link($5, $2);
        free($2);
        free($5);
      }
    | LBRACK RBRACK LPAREN dest RPAREN {
        $$ = html_link($4, "");
        free($4);
      }
    ;

image
    : BANG LBRACK atoms RBRACK LPAREN dest RPAREN {
        char *alt = html_plain($3);
        $$ = html_image($6, alt);
        free($3);
        free($6);
        free(alt);
      }
    | BANG LBRACK RBRACK LPAREN dest RPAREN {
        $$ = html_image($5, "");
        free($5);
      }
    ;

dest
    : TEXT              { $$ = $1; }
    | AUTOLINK          { $$ = $1; }
    | dest TEXT         { $$ = str_concat($1, $2); }
    | dest ESCAPE       { $$ = str_concat($1, $2); }
    | dest AUTOLINK     { $$ = str_concat($1, $2); }
    ;

%%

void yyerror(const char *s)
{
    fprintf(stderr, "md-convert: parse error at line %d: %s\n", yylineno, s);
    g_parse_ok = 0;
}

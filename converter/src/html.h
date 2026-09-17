#ifndef MD_CONVERT_HTML_H
#define MD_CONVERT_HTML_H

#include <stddef.h>

typedef struct Fence {
    char *info;
    char delim;
    int len;
} Fence;

char *str_dup(const char *s);
char *str_concat(char *a, char *b);
char *str_concat_sep(char *a, const char *sep, char *b);
char *html_escape(const char *s);

/* type → <tag class="md-...">inner</tag>  (003 mapping) */
char *html_tag(const char *type, const char *inner);
char *html_tag_class(const char *type, const char *extra_class, const char *inner);
char *html_void(const char *type, const char *attrs);
char *html_heading(int level, const char *inner);
char *html_fence(const char *info, const char *body);
char *html_link(const char *href, const char *inner);
char *html_image(const char *src, const char *alt);
char *html_checkbox(int checked);
char *html_table(char *header_row, const char *aligns, char *body_rows);

char *html_document(const char *title, const char *css_href, const char *body,
                    const char *extra_js);
char *html_plain(const char *html);
char *html_first_h1_text(const char *body);
char *html_filename_title(const char *path);

#endif

#include "html.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void die(const char *msg)
{
    fprintf(stderr, "md-convert: %s\n", msg);
    exit(1);
}

static char *xmalloc(size_t n)
{
    char *p = (char *)malloc(n);
    if (!p)
        die("out of memory");
    return p;
}

char *str_dup(const char *s)
{
    size_t n;
    char *p;
    if (!s)
        s = "";
    n = strlen(s);
    p = xmalloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

char *str_concat(char *a, char *b)
{
    size_t na, nb;
    char *p;
    if (!a)
        a = str_dup("");
    if (!b)
        b = str_dup("");
    na = strlen(a);
    nb = strlen(b);
    p = xmalloc(na + nb + 1);
    memcpy(p, a, na);
    memcpy(p + na, b, nb + 1);
    free(a);
    free(b);
    return p;
}

char *str_concat_sep(char *a, const char *sep, char *b)
{
    size_t na, ns, nb;
    char *p;
    if (!sep)
        sep = "";
    if (!a)
        a = str_dup("");
    if (!b)
        b = str_dup("");
    na = strlen(a);
    ns = strlen(sep);
    nb = strlen(b);
    p = xmalloc(na + ns + nb + 1);
    memcpy(p, a, na);
    memcpy(p + na, sep, ns);
    memcpy(p + na + ns, b, nb + 1);
    free(a);
    free(b);
    return p;
}

static char *str_fmt3(const char *a, const char *b, const char *c)
{
    size_t na, nb, nc;
    char *p;
    if (!a)
        a = "";
    if (!b)
        b = "";
    if (!c)
        c = "";
    na = strlen(a);
    nb = strlen(b);
    nc = strlen(c);
    p = xmalloc(na + nb + nc + 1);
    memcpy(p, a, na);
    memcpy(p + na, b, nb);
    memcpy(p + na + nb, c, nc + 1);
    return p;
}

char *html_escape(const char *s)
{
    size_t extra = 0;
    const char *t;
    char *out, *p;
    if (!s)
        s = "";
    for (t = s; *t; t++) {
        if (*t == '&')
            extra += 4;
        else if (*t == '<' || *t == '>')
            extra += 3;
        else if (*t == '"')
            extra += 5;
    }
    out = xmalloc(strlen(s) + extra + 1);
    p = out;
    for (t = s; *t; t++) {
        if (*t == '&') {
            memcpy(p, "&amp;", 5);
            p += 5;
        } else if (*t == '<') {
            memcpy(p, "&lt;", 4);
            p += 4;
        } else if (*t == '>') {
            memcpy(p, "&gt;", 4);
            p += 4;
        } else if (*t == '"') {
            memcpy(p, "&quot;", 6);
            p += 6;
        } else {
            *p++ = *t;
        }
    }
    *p = '\0';
    return out;
}

typedef struct HtmlMap {
    const char *type;
    const char *tag;
    const char *cls;
    int void_el;
} HtmlMap;

static const HtmlMap MAP[] = {
    {"h1", "h1", "md-h1", 0},
    {"h2", "h2", "md-h2", 0},
    {"h3", "h3", "md-h3", 0},
    {"h4", "h4", "md-h4", 0},
    {"h5", "h5", "md-h5", 0},
    {"h6", "h6", "md-h6", 0},
    {"p", "p", "md-p", 0},
    {"br", "br", "md-br", 1},
    {"strong", "strong", "md-strong", 0},
    {"em", "em", "md-em", 0},
    {"del", "del", "md-del", 0},
    {"code", "code", "md-code", 0},
    {"pre", "pre", "md-pre", 0},
    {"code_block", "code", "md-code-block", 0},
    {"blockquote", "blockquote", "md-blockquote", 0},
    {"ul", "ul", "md-ul", 0},
    {"ol", "ol", "md-ol", 0},
    {"li", "li", "md-li", 0},
    {"task_item", "li", "md-li md-task", 0},
    {"checkbox", "input", "md-checkbox", 1},
    {"a", "a", "md-a", 0},
    {"img", "img", "md-img", 1},
    {"table", "table", "md-table", 0},
    {"thead", "thead", "md-thead", 0},
    {"tbody", "tbody", "md-tbody", 0},
    {"tr", "tr", "md-tr", 0},
    {"th", "th", "md-th", 0},
    {"td", "td", "md-td", 0},
    {"hr", "hr", "md-hr", 1},
    {NULL, NULL, NULL, 0}
};

static const HtmlMap *lookup(const char *type)
{
    int i;
    for (i = 0; MAP[i].type; i++) {
        if (strcmp(MAP[i].type, type) == 0)
            return &MAP[i];
    }
    die("unknown html type");
    return NULL;
}

static char *join4(const char *a, const char *b, const char *c, const char *d)
{
    size_t n = strlen(a) + strlen(b) + strlen(c) + strlen(d);
    char *p = xmalloc(n + 1);
    strcpy(p, a);
    strcat(p, b);
    strcat(p, c);
    strcat(p, d);
    return p;
}

char *html_tag(const char *type, const char *inner)
{
    return html_tag_class(type, NULL, inner);
}

char *html_tag_class(const char *type, const char *extra_class, const char *inner)
{
    const HtmlMap *m = lookup(type);
    const char *body = inner ? inner : "";
    char *cls;
    char *open;
    char *close;
    char *out;

    if (extra_class && extra_class[0]) {
        size_t n = strlen(m->cls) + 1 + strlen(extra_class) + 1;
        cls = xmalloc(n);
        snprintf(cls, n, "%s %s", m->cls, extra_class);
    } else {
        cls = str_dup(m->cls);
    }

    {
        size_t n = strlen(m->tag) + strlen(cls) + 32;
        open = xmalloc(n);
        snprintf(open, n, "<%s class=\"%s\">", m->tag, cls);
    }
    close = join4("</", m->tag, ">", "");
    out = str_fmt3(open, body, close);
    free(cls);
    free(open);
    free(close);
    return out;
}

char *html_void(const char *type, const char *attrs)
{
    const HtmlMap *m = lookup(type);
    char *out;
    size_t n;
    const char *a = attrs ? attrs : "";
    n = strlen(m->tag) + strlen(m->cls) + strlen(a) + 32;
    out = xmalloc(n);
    if (a[0])
        snprintf(out, n, "<%s class=\"%s\" %s>", m->tag, m->cls, a);
    else
        snprintf(out, n, "<%s class=\"%s\">", m->tag, m->cls);
    return out;
}

char *html_heading(int level, const char *inner)
{
    char type[4] = "h1";
    if (level < 1)
        level = 1;
    if (level > 6)
        level = 6;
    type[1] = (char)('0' + level);
    return html_tag(type, inner);
}

static int is_lang_char(char c)
{
    return isalnum((unsigned char)c) || c == '_' || c == '-';
}

static int fence_line_count(const char *body)
{
    int n;
    const char *p;
    if (!body || !*body)
        return 0;
    n = 1;
    for (p = body; *p; p++) {
        if (*p == '\n')
            n++;
    }
    return n;
}

/* "1\n2\n...\nN" for gutter; empty body → NULL */
static char *fence_line_numbers(int lines)
{
    char *buf;
    char *w;
    size_t cap;
    int i;
    int written;
    if (lines <= 0)
        return NULL;
    /* worst case: each line "2147483647\n" ≈ 11 chars */
    cap = (size_t)lines * 12 + 1;
    buf = xmalloc(cap);
    w = buf;
    for (i = 1; i <= lines; i++) {
        written = snprintf(w, (size_t)(buf + cap - w), i == lines ? "%d" : "%d\n", i);
        if (written < 0)
            die("snprintf failed");
        w += written;
    }
    return buf;
}

char *html_fence(const char *info, const char *body)
{
    char lang[64];
    char extra[80];
    char *code;
    char *nums;
    char *inner;
    char *pre;
    int i = 0;
    int lines;
    const char *p = info ? info : "";

    while (*p && isspace((unsigned char)*p))
        p++;
    lang[0] = '\0';
    extra[0] = '\0';
    if (*p && is_lang_char(*p)) {
        while (*p && is_lang_char(*p) && i < (int)sizeof(lang) - 1) {
            lang[i++] = (char)tolower((unsigned char)*p++);
        }
        lang[i] = '\0';
        snprintf(extra, sizeof(extra), "md-lang-%s", lang);
    }

    code = html_tag_class("code_block", extra[0] ? extra : NULL, body ? body : "");
    lines = fence_line_count(body);
    nums = fence_line_numbers(lines);
    if (nums) {
        size_t n = strlen(nums) + strlen(code) + 64;
        inner = xmalloc(n);
        snprintf(inner, n,
                 "<span class=\"md-line-numbers\" aria-hidden=\"true\">%s</span>%s",
                 nums, code);
        free(nums);
        free(code);
    } else {
        inner = code;
    }
    pre = html_tag("pre", inner);
    free(inner);
    return pre;
}

char *html_link(const char *href, const char *inner)
{
    char *eh = html_escape(href ? href : "");
    const HtmlMap *m = lookup("a");
    size_t n = strlen(m->cls) + strlen(eh) + strlen(inner ? inner : "") + 32;
    char *out = xmalloc(n);
    snprintf(out, n, "<a class=\"%s\" href=\"%s\">%s</a>", m->cls, eh, inner ? inner : "");
    free(eh);
    return out;
}

char *html_image(const char *src, const char *alt)
{
    char *es = html_escape(src ? src : "");
    char *ea = html_escape(alt ? alt : "");
    const HtmlMap *m = lookup("img");
    size_t n = strlen(m->cls) + strlen(es) + strlen(ea) + 40;
    char *out = xmalloc(n);
    snprintf(out, n, "<img class=\"%s\" src=\"%s\" alt=\"%s\">", m->cls, es, ea);
    free(es);
    free(ea);
    return out;
}

char *html_checkbox(int checked)
{
    if (checked)
        return html_void("checkbox", "type=\"checkbox\" disabled checked");
    return html_void("checkbox", "type=\"checkbox\" disabled");
}

static const char *align_class(char a)
{
    if (a == 'C')
        return "md-align-center";
    if (a == 'R')
        return "md-align-right";
    return "md-align-left";
}

static int is_ws_only(const char *s)
{
    if (!s)
        return 1;
    while (*s) {
        if (*s != ' ' && *s != '\t')
            return 0;
        s++;
    }
    return 1;
}

static void trim_inplace(char *s)
{
    char *e;
    char *p = s;
    if (!s)
        return;
    while (*p == ' ' || *p == '\t')
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t'))
        *--e = '\0';
}

static int split_cells(const char *row, char ***out)
{
    int n = 1, i, k = 0;
    const char *p;
    char **cells;
    if (!row)
        row = "";
    for (p = row; *p; p++) {
        if (*p == '\x1f')
            n++;
    }
    cells = (char **)malloc((size_t)n * sizeof(char *));
    if (!cells)
        die("out of memory");
    p = row;
    for (i = 0; i < n; i++) {
        const char *start = p;
        while (*p && *p != '\x1f')
            p++;
        cells[k] = xmalloc((size_t)(p - start) + 1);
        memcpy(cells[k], start, (size_t)(p - start));
        cells[k][(size_t)(p - start)] = '\0';
        trim_inplace(cells[k]);
        k++;
        if (*p == '\x1f')
            p++;
    }
    while (k > 0 && is_ws_only(cells[k - 1])) {
        free(cells[--k]);
    }
    *out = cells;
    return k;
}

static char *cell_tag(const char *type, char align, const char *inner)
{
    return html_tag_class(type, align_class(align), inner);
}

char *html_table(char *header_row, const char *aligns, char *body_rows)
{
    char **hcells = NULL;
    int hn, cols, i;
    char *thead_inner = str_dup("");
    char *tbody_inner = str_dup("");
    char *tr, *thead, *tbody, *table;
    const char *al = aligns ? aligns : "";

    hn = split_cells(header_row ? header_row : "", &hcells);
    cols = hn;
    for (i = 0; i < hn; i++) {
        char a = al[i] ? al[i] : 'L';
        char *cell = cell_tag("th", a, hcells[i]);
        thead_inner = str_concat(thead_inner, cell);
        free(hcells[i]);
    }
    free(hcells);
    free(header_row);

    if (body_rows && body_rows[0]) {
        char *save, *line;
        save = body_rows;
        for (line = strtok(body_rows, "\x1e"); line; line = strtok(NULL, "\x1e")) {
            char **cells = NULL;
            int cn = split_cells(line, &cells);
            char *rowi = str_dup("");
            int n = cn > cols ? cn : cols;
            for (i = 0; i < n; i++) {
                char a = al[i] ? al[i] : 'L';
                const char *inner = (i < cn) ? cells[i] : "";
                char *cell = cell_tag("td", a, inner);
                rowi = str_concat(rowi, cell);
            }
            for (i = 0; i < cn; i++)
                free(cells[i]);
            free(cells);
            tr = html_tag("tr", rowi);
            free(rowi);
            tbody_inner = str_concat(tbody_inner, tr);
        }
        free(save);
    } else {
        free(body_rows);
    }

    tr = html_tag("tr", thead_inner);
    free(thead_inner);
    thead = html_tag("thead", tr);
    free(tr);
    tbody = html_tag("tbody", tbody_inner);
    free(tbody_inner);
    {
        char *inner = str_concat(thead, tbody);
        table = html_tag("table", inner);
        free(inner);
    }
    return table;
}

char *html_document(const char *title, const char *css_href, const char *body,
                    const char *extra_js)
{
    char *et = html_escape(title ? title : "");
    char *ecss = html_escape(css_href ? css_href : "gfm.css");
    const char *b = body ? body : "";
    const char *ej = extra_js ? extra_js : "";
    char *out;
    size_t n;

    /* extra_js: already-wrapped <script src> tags from main (highlight / copy-wechat). */
    n = strlen(et) + strlen(ecss) + strlen(b) + strlen(ej) + 512;
    out = xmalloc(n);
    snprintf(out, n,
             "<!DOCTYPE html>\n"
             "<html lang=\"zh-CN\">\n"
             "<head>\n"
             "  <meta charset=\"utf-8\">\n"
             "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
             "  <title>%s</title>\n"
             "  <link rel=\"stylesheet\" href=\"%s\">\n"
             "</head>\n"
             "<body class=\"md-body\">\n"
             "<div id=\"md-article\">\n"
             "%s\n"
             "</div>\n"
             "%s"
             "</body>\n"
             "</html>\n",
             et, ecss, b, ej);
    free(et);
    free(ecss);
    return out;
}

static void strip_tags_to(const char *s, char *out);

char *html_plain(const char *html)
{
    char *out;
    if (!html)
        return str_dup("");
    out = xmalloc(strlen(html) + 1);
    strip_tags_to(html, out);
    return out;
}

static void strip_tags_to(const char *s, char *out)
{
    int in_tag = 0;
    char *p = out;
    for (; *s; s++) {
        if (*s == '<')
            in_tag = 1;
        else if (*s == '>')
            in_tag = 0;
        else if (!in_tag)
            *p++ = *s;
    }
    *p = '\0';
}

char *html_first_h1_text(const char *body)
{
    const char *start, *end;
    char *tmp, *out;
    if (!body)
        return NULL;
    start = strstr(body, "<h1 class=\"md-h1\">");
    if (!start)
        return NULL;
    start += strlen("<h1 class=\"md-h1\">");
    end = strstr(start, "</h1>");
    if (!end)
        return NULL;
    tmp = xmalloc((size_t)(end - start) + 1);
    memcpy(tmp, start, (size_t)(end - start));
    tmp[end - start] = '\0';
    out = xmalloc(strlen(tmp) + 1);
    strip_tags_to(tmp, out);
    free(tmp);
    if (!out[0]) {
        free(out);
        return NULL;
    }
    return out;
}

char *html_filename_title(const char *path)
{
    const char *base, *slash, *dot;
    char *out;
    size_t n;
    if (!path)
        return str_dup("untitled");
    slash = strrchr(path, '/');
#ifdef _WIN32
    {
        const char *bslash = strrchr(path, '\\');
        if (!slash || (bslash && bslash > slash))
            slash = bslash;
    }
#endif
    base = slash ? slash + 1 : path;
    dot = strrchr(base, '.');
    n = (dot && dot != base) ? (size_t)(dot - base) : strlen(base);
    if (n == 0)
        return str_dup("untitled");
    out = xmalloc(n + 1);
    memcpy(out, base, n);
    out[n] = '\0';
    return out;
}

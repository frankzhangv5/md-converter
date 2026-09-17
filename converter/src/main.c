#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "embedded_assets.h"
#include "html.h"

extern FILE *yyin;
extern char *g_body;
extern int g_parse_ok;
int yyparse(void);

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s input.md [-o out.html] [--theme name] [--css path]\n"
            "                     [--no-highlight] [--no-copy]\n"
            "  no -o              writes HTML next to input (foo.md → foo.html)\n"
            "  --theme name       built-in CSS: gfm (default), teal, vermillion\n"
            "  --css path         read CSS from file and inline (overrides --theme)\n"
            "  --no-highlight     skip inlining highlight.js\n"
            "  --no-copy          skip inlining copy-wechat.js (复制到公众号 button)\n",
            argv0);
}

/* foo.md → foo.html; path/bar → path/bar.html; keeps directory prefix. */
static char *default_output_path(const char *input)
{
    const char *slash;
    const char *base;
    const char *dot;
    size_t prefix_len;
    char *out;

    if (!input || !input[0])
        return NULL;

    slash = strrchr(input, '/');
#ifdef _WIN32
    {
        const char *bslash = strrchr(input, '\\');
        if (!slash || (bslash && bslash > slash))
            slash = bslash;
    }
#endif
    base = slash ? slash + 1 : input;
    dot = strrchr(base, '.');
    if (dot && dot != base)
        prefix_len = (size_t)(dot - input);
    else
        prefix_len = strlen(input);

    out = (char *)malloc(prefix_len + 6);
    if (!out) {
        fprintf(stderr, "md-convert: out of memory\n");
        exit(1);
    }
    memcpy(out, input, prefix_len);
    memcpy(out + prefix_len, ".html", 6);
    return out;
}

static char *read_file_text(const char *path)
{
    FILE *f;
    long sz;
    char *buf;
    size_t n;

    f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        fprintf(stderr, "md-convert: out of memory\n");
        exit(1);
    }
    n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

/* Avoid premature </script> close if JS ever contains that sequence. */
static char *sanitize_inline_js(const char *js)
{
    const char *p;
    const char *needle = "</script>";
    size_t needle_len = 9;
    size_t extra = 0;
    char *out;
    char *w;

    if (!js)
        return str_dup("");
    for (p = js; (p = strstr(p, needle)) != NULL; p += needle_len)
        extra += 1; /* insert one '\' before '/' */
    out = (char *)malloc(strlen(js) + extra + 1);
    if (!out) {
        fprintf(stderr, "md-convert: out of memory\n");
        exit(1);
    }
    w = out;
    for (p = js; *p;) {
        if (strncmp(p, needle, needle_len) == 0) {
            memcpy(w, "<\\/script>", 10);
            w += 10;
            p += needle_len;
        } else {
            *w++ = *p++;
        }
    }
    *w = '\0';
    return out;
}

static char *script_inline_tag(const char *id, const char *js)
{
    size_t n;
    char *out;
    char *safe;

    if (!js || !js[0])
        return str_dup("");
    safe = sanitize_inline_js(js);
    n = strlen(id) + strlen(safe) + 64;
    out = (char *)malloc(n);
    if (!out) {
        fprintf(stderr, "md-convert: out of memory\n");
        exit(1);
    }
    snprintf(out, n, "  <script id=\"%s\">\n%s\n  </script>\n", id, safe);
    free(safe);
    return out;
}

int main(int argc, char **argv)
{
    const char *input = NULL;
    const char *output = NULL;
    char *output_owned = NULL;
    const char *theme = "gfm";
    const char *css_path = NULL;
    char *css_owned = NULL;
    const char *css_text;
    int highlight = 1;
    int copy_btn = 1;
    FILE *in;
    FILE *out;
    char *title;
    char *doc;
    char *scripts;
    char *s1;
    char *s2;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                usage(argv[0]);
                return 2;
            }
            output = argv[++i];
        } else if (strcmp(argv[i], "--theme") == 0) {
            if (i + 1 >= argc) {
                usage(argv[0]);
                return 2;
            }
            theme = argv[++i];
        } else if (strcmp(argv[i], "--css") == 0) {
            if (i + 1 >= argc) {
                usage(argv[0]);
                return 2;
            }
            css_path = argv[++i];
        } else if (strcmp(argv[i], "--no-highlight") == 0) {
            highlight = 0;
        } else if (strcmp(argv[i], "--no-copy") == 0) {
            copy_btn = 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (argv[i][0] == '-') {
            usage(argv[0]);
            return 2;
        } else if (!input) {
            input = argv[i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (!input) {
        usage(argv[0]);
        return 2;
    }

    if (!output) {
        output_owned = default_output_path(input);
        output = output_owned;
    }

    if (css_path) {
        css_owned = read_file_text(css_path);
        if (!css_owned) {
            perror(css_path);
            free(output_owned);
            return 1;
        }
        css_text = css_owned;
    } else {
        css_text = md_theme_css(theme);
        if (!css_text) {
            fprintf(stderr,
                    "md-convert: unknown theme '%s' (use gfm, teal, or vermillion)\n",
                    theme);
            free(output_owned);
            return 2;
        }
    }

    in = fopen(input, "rb");
    if (!in) {
        perror(input);
        free(css_owned);
        free(output_owned);
        return 1;
    }
    yyin = in;

    if (yyparse() != 0 || !g_parse_ok) {
        fclose(in);
        free(css_owned);
        free(output_owned);
        return 1;
    }
    fclose(in);

    title = html_first_h1_text(g_body);
    if (!title)
        title = html_filename_title(input);

    s1 = highlight ? script_inline_tag("md-highlight", md_js_highlight)
                   : str_dup("");
    s2 = copy_btn ? script_inline_tag("md-copy-wechat-js", md_js_copy_wechat)
                  : str_dup("");
    scripts = str_concat(s1, s2);

    doc = html_document(title, css_text, g_body ? g_body : "", scripts);
    free(scripts);
    free(title);
    free(g_body);
    g_body = NULL;
    free(css_owned);

    out = fopen(output, "wb");
    if (!out) {
        perror(output);
        free(doc);
        free(output_owned);
        return 1;
    }
    fputs(doc, out);
    fclose(out);
    free(doc);
    free(output_owned);
    return 0;
}

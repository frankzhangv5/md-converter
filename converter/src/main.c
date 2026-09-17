#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "html.h"

extern FILE *yyin;
extern char *g_body;
extern int g_parse_ok;
int yyparse(void);

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s input.md [-o out.html] [--css path] [--no-highlight] [--no-copy]\n"
            "  no -o            writes HTML next to input (foo.md → foo.html)\n"
            "  --css path       stylesheet href (default: gfm.css; includes code-token colors)\n"
            "  --no-highlight   skip linking highlight.js\n"
            "  --no-copy        skip linking copy-wechat.js (复制到公众号 button)\n",
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

static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

static void dirname_copy(const char *path, char *out, size_t cap)
{
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *bslash = strrchr(path, '\\');
    if (!slash || (bslash && bslash > slash))
        slash = bslash;
#endif
    if (!slash) {
        snprintf(out, cap, ".");
        return;
    }
    if (slash == path) {
        snprintf(out, cap, "/");
        return;
    }
    {
        size_t n = (size_t)(slash - path);
        if (n >= cap)
            n = cap - 1;
        memcpy(out, path, n);
        out[n] = '\0';
    }
}

/* Browser-relative src, derived like --css href (not absolute filesystem paths). */
static char *resolve_js_src(const char *css_href, const char *argv0, const char *name)
{
    char dir[1024];
    char parent[1024];
    char buf[1200];
    char href[1200];

    dirname_copy(css_href ? css_href : "gfm.css", dir, sizeof(dir));

    /* Same directory as the stylesheet. */
    if (strcmp(dir, ".") == 0)
        snprintf(buf, sizeof(buf), "%s", name);
    else
        snprintf(buf, sizeof(buf), "%s/%s", dir, name);
    if (file_exists(buf))
        return str_dup(buf);

    /* Sibling js/ next to the css/ directory: css/gfm.css → js/name. */
    if (strcmp(dir, ".") != 0) {
        dirname_copy(dir, parent, sizeof(parent));
        if (strcmp(parent, ".") == 0)
            snprintf(href, sizeof(href), "js/%s", name);
        else
            snprintf(href, sizeof(href), "%s/js/%s", parent, name);
        if (file_exists(href))
            return str_dup(href);
    }

    snprintf(href, sizeof(href), "js/%s", name);
    if (file_exists(href))
        return str_dup(href);

    dirname_copy(argv0 ? argv0 : ".", dir, sizeof(dir));
    snprintf(buf, sizeof(buf), "%s/../js/%s", dir, name);
    if (file_exists(buf))
        return str_dup(href);

    snprintf(buf, sizeof(buf), "converter/js/%s", name);
    if (file_exists(buf))
        return str_dup(buf);

    return NULL;
}

static char *script_src_tag(const char *id, const char *src)
{
    size_t n;
    char *out;
    char *esrc;
    if (!src || !src[0])
        return str_dup("");
    esrc = html_escape(src);
    n = strlen(id) + strlen(esrc) + 64;
    out = (char *)malloc(n);
    if (!out) {
        fprintf(stderr, "md-convert: out of memory\n");
        exit(1);
    }
    snprintf(out, n, "  <script id=\"%s\" src=\"%s\"></script>\n", id, esrc);
    free(esrc);
    return out;
}

int main(int argc, char **argv)
{
    const char *input = NULL;
    const char *output = NULL;
    char *output_owned = NULL;
    const char *css = "gfm.css";
    int highlight = 1;
    int copy_btn = 1;
    FILE *in;
    FILE *out;
    char *title;
    char *doc;
    char *hl_src = NULL;
    char *copy_src = NULL;
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
        } else if (strcmp(argv[i], "--css") == 0) {
            if (i + 1 >= argc) {
                usage(argv[0]);
                return 2;
            }
            css = argv[++i];
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

    in = fopen(input, "rb");
    if (!in) {
        perror(input);
        free(output_owned);
        return 1;
    }
    yyin = in;

    if (yyparse() != 0 || !g_parse_ok) {
        fclose(in);
        free(output_owned);
        return 1;
    }
    fclose(in);

    title = html_first_h1_text(g_body);
    if (!title)
        title = html_filename_title(input);

    if (highlight) {
        hl_src = resolve_js_src(css, argv[0], "highlight.js");
        if (!hl_src) {
            fprintf(stderr,
                    "md-convert: warning: highlight.js not found; "
                    "token classes will not be applied in the browser\n");
        }
    }

    if (copy_btn) {
        copy_src = resolve_js_src(css, argv[0], "copy-wechat.js");
        if (!copy_src) {
            fprintf(stderr,
                    "md-convert: warning: copy-wechat.js not found; "
                    "复制到公众号 button will be missing\n");
        }
    }

    s1 = script_src_tag("md-highlight", hl_src ? hl_src : "");
    s2 = script_src_tag("md-copy-wechat-js", copy_src ? copy_src : "");
    scripts = str_concat(s1, s2);

    doc = html_document(title, css, g_body ? g_body : "", scripts);
    free(scripts);
    free(title);
    free(g_body);
    g_body = NULL;
    free(hl_src);
    free(copy_src);

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

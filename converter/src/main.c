#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "embedded_assets.h"
#include "html.h"

extern char *g_body;
extern int g_parse_ok;
int yyparse(void);
void yyrestart(FILE *input_file);
void lex_reset(void);

typedef struct {
    char **paths;
    size_t n;
    size_t cap;
} path_list;

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s input.md [-o outdir] [--theme name] [--css path]\n"
            "                     [--no-highlight] [--no-copy]\n"
            "       %s -d indir [-o outdir] [--theme name] [--css path]\n"
            "                     [--no-highlight] [--no-copy]\n"
            "  -d, --dir indir    convert every *.md file in indir (non-recursive)\n"
            "  -o outdir          write HTML into outdir (basename.md → basename.html)\n"
            "  no -o              write HTML next to each input (foo.md → foo.html)\n"
            "  --theme name       built-in CSS: gfm (default), teal, vermillion,\n"
            "                     azure, lime, jade, tangerine\n"
            "  --css path         read CSS from file and inline (overrides --theme)\n"
            "  --no-highlight     skip inlining highlight.js\n"
            "  --no-copy          skip inlining copy-wechat.js (复制到公众号 button)\n",
            argv0, argv0);
}

static void die_oom(void)
{
    fprintf(stderr, "md-convert: out of memory\n");
    exit(1);
}

static const char *path_sep_last(const char *path)
{
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    {
        const char *bslash = strrchr(path, '\\');
        if (!slash || (bslash && bslash > slash))
            slash = bslash;
    }
#endif
    return slash;
}

static const char *path_basename(const char *path)
{
    const char *slash = path_sep_last(path);
    return slash ? slash + 1 : path;
}

static int ends_with_md(const char *name)
{
    size_t n;
    if (!name)
        return 0;
    n = strlen(name);
    if (n < 3)
        return 0;
    return (name[n - 3] == '.' &&
            (name[n - 2] == 'm' || name[n - 2] == 'M') &&
            (name[n - 1] == 'd' || name[n - 1] == 'D'));
}

/* foo.md → foo.html; bar → bar.html; keeps only the basename. */
static char *html_basename(const char *input)
{
    const char *base = path_basename(input);
    const char *dot = strrchr(base, '.');
    size_t prefix_len;
    char *out;

    if (dot && dot != base)
        prefix_len = (size_t)(dot - base);
    else
        prefix_len = strlen(base);

    out = (char *)malloc(prefix_len + 6);
    if (!out)
        die_oom();
    memcpy(out, base, prefix_len);
    memcpy(out + prefix_len, ".html", 6);
    return out;
}

/* Join dir + name with the separator already used in dir (default '/'). */
static char *path_join(const char *dir, const char *name)
{
    size_t dlen;
    size_t nlen;
    int need_sep;
    char sep = '/';
    char *out;

    if (!dir || !dir[0])
        return str_dup(name);
    dlen = strlen(dir);
    nlen = strlen(name);
#ifdef _WIN32
    if (strchr(dir, '\\') && !strchr(dir, '/'))
        sep = '\\';
#endif
    need_sep = (dir[dlen - 1] != '/' && dir[dlen - 1] != '\\');
    out = (char *)malloc(dlen + need_sep + nlen + 1);
    if (!out)
        die_oom();
    memcpy(out, dir, dlen);
    if (need_sep)
        out[dlen++] = sep;
    memcpy(out + dlen, name, nlen + 1);
    return out;
}

/* foo.md → foo.html beside input; keeps directory prefix of input. */
static char *sidecar_html_path(const char *input)
{
    const char *slash = path_sep_last(input);
    char *base_html = html_basename(input);
    char *out;

    if (!slash) {
        out = base_html;
    } else {
        size_t dir_len = (size_t)(slash - input + 1);
        out = (char *)malloc(dir_len + strlen(base_html) + 1);
        if (!out)
            die_oom();
        memcpy(out, input, dir_len);
        memcpy(out + dir_len, base_html, strlen(base_html) + 1);
        free(base_html);
    }
    return out;
}

static char *resolve_output_path(const char *input, const char *outdir)
{
    char *base_html;
    char *out;

    if (!outdir)
        return sidecar_html_path(input);
    base_html = html_basename(input);
    out = path_join(outdir, base_html);
    free(base_html);
    return out;
}

static int ensure_dir(const char *path)
{
#ifdef _WIN32
    if (_mkdir(path) == 0)
        return 0;
#else
    if (mkdir(path, 0755) == 0)
        return 0;
#endif
    if (errno == EEXIST)
        return 0;
    return -1;
}

static int is_directory(const char *path)
{
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return 0;
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    if (stat(path, &st) != 0)
        return 0;
    return S_ISDIR(st.st_mode);
#endif
}

static void path_list_push(path_list *list, char *path)
{
    if (list->n >= list->cap) {
        size_t cap = list->cap ? list->cap * 2 : 16;
        char **p = (char **)realloc(list->paths, cap * sizeof(char *));
        if (!p)
            die_oom();
        list->paths = p;
        list->cap = cap;
    }
    list->paths[list->n++] = path;
}

static void path_list_free(path_list *list)
{
    size_t i;
    for (i = 0; i < list->n; i++)
        free(list->paths[i]);
    free(list->paths);
    list->paths = NULL;
    list->n = list->cap = 0;
}

static int path_cmp(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static int collect_md_in_dir(const char *dir, path_list *list)
{
#ifdef _WIN32
    char pattern[MAX_PATH];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    size_t n;

    n = strlen(dir);
    if (n + 3 >= sizeof(pattern)) {
        fprintf(stderr, "md-convert: directory path too long: %s\n", dir);
        return -1;
    }
    memcpy(pattern, dir, n);
    if (n && dir[n - 1] != '/' && dir[n - 1] != '\\') {
        pattern[n++] = '\\';
    }
    pattern[n++] = '*';
    pattern[n] = '\0';

    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "md-convert: cannot open directory: %s\n", dir);
        return -1;
    }
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        if (!ends_with_md(fd.cFileName))
            continue;
        path_list_push(list, path_join(dir, fd.cFileName));
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d;
    struct dirent *ent;

    d = opendir(dir);
    if (!d) {
        perror(dir);
        return -1;
    }
    while ((ent = readdir(d)) != NULL) {
        char *full;
        struct stat st;
        if (!ends_with_md(ent->d_name))
            continue;
        full = path_join(dir, ent->d_name);
        if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) {
            free(full);
            continue;
        }
        path_list_push(list, full);
    }
    closedir(d);
#endif
    if (list->n > 1)
        qsort(list->paths, list->n, sizeof(char *), path_cmp);
    return 0;
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
        die_oom();
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
    if (!out)
        die_oom();
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
    if (!out)
        die_oom();
    snprintf(out, n, "  <script id=\"%s\">\n%s\n  </script>\n", id, safe);
    free(safe);
    return out;
}

static int convert_one(const char *input, const char *outdir, const char *css_text,
                       int highlight, int copy_btn)
{
    FILE *in;
    FILE *out;
    char *output = NULL;
    char *title;
    char *doc;
    char *scripts;
    char *s1;
    char *s2;

    in = fopen(input, "rb");
    if (!in) {
        perror(input);
        return 1;
    }

    yyrestart(in);
    lex_reset();
    g_parse_ok = 1;
    free(g_body);
    g_body = NULL;

    if (yyparse() != 0 || !g_parse_ok) {
        fclose(in);
        free(g_body);
        g_body = NULL;
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

    output = resolve_output_path(input, outdir);
    out = fopen(output, "wb");
    if (!out) {
        perror(output);
        free(doc);
        free(output);
        return 1;
    }
    if (fputs(doc, out) < 0) {
        perror(output);
        fclose(out);
        free(doc);
        free(output);
        return 1;
    }
    fclose(out);
    free(doc);
    free(output);
    return 0;
}

int main(int argc, char **argv)
{
    const char *input = NULL;
    const char *indir = NULL;
    const char *outdir = NULL;
    const char *theme = "gfm";
    const char *css_path = NULL;
    char *css_owned = NULL;
    const char *css_text;
    int highlight = 1;
    int copy_btn = 1;
    path_list files = {0};
    size_t i;
    int failed = 0;

    for (i = 1; i < (size_t)argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= (size_t)argc) {
                usage(argv[0]);
                return 2;
            }
            outdir = argv[++i];
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dir") == 0) {
            if (i + 1 >= (size_t)argc) {
                usage(argv[0]);
                return 2;
            }
            indir = argv[++i];
        } else if (strcmp(argv[i], "--theme") == 0) {
            if (i + 1 >= (size_t)argc) {
                usage(argv[0]);
                return 2;
            }
            theme = argv[++i];
        } else if (strcmp(argv[i], "--css") == 0) {
            if (i + 1 >= (size_t)argc) {
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

    if ((input && indir) || (!input && !indir)) {
        usage(argv[0]);
        return 2;
    }

    if (indir) {
        if (!is_directory(indir)) {
            fprintf(stderr, "md-convert: not a directory: %s\n", indir);
            return 1;
        }
        if (collect_md_in_dir(indir, &files) != 0)
            return 1;
        if (files.n == 0) {
            fprintf(stderr, "md-convert: no .md files in %s\n", indir);
            path_list_free(&files);
            return 1;
        }
    } else {
        path_list_push(&files, str_dup(input));
    }

    if (outdir) {
        if (ensure_dir(outdir) != 0) {
            perror(outdir);
            path_list_free(&files);
            return 1;
        }
    }

    if (css_path) {
        css_owned = read_file_text(css_path);
        if (!css_owned) {
            perror(css_path);
            path_list_free(&files);
            return 1;
        }
        css_text = css_owned;
    } else {
        css_text = md_theme_css(theme);
        if (!css_text) {
            fprintf(stderr,
                    "md-convert: unknown theme '%s' "
                    "(use gfm, teal, vermillion, azure, lime, jade, or tangerine)\n",
                    theme);
            path_list_free(&files);
            return 2;
        }
    }

    for (i = 0; i < files.n; i++) {
        if (convert_one(files.paths[i], outdir, css_text, highlight, copy_btn) != 0) {
            fprintf(stderr, "md-convert: failed: %s\n", files.paths[i]);
            failed = 1;
        } else {
            printf("md-convert: ok: %s\n", files.paths[i]);
        }
    }

    free(css_owned);
    path_list_free(&files);
    return failed ? 1 : 0;
}

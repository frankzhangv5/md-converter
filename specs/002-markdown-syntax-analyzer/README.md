---
status: complete
created: 2026-08-28
priority: high
---

# Markdown 语法分析器

> Bison 消费 001 的 token，用 LALR 文法识别 GFM 子集；**每个规则按 type 返回带 CSS class 的 HTML 字符串**。实现：`converter/src/parser.y`。

## Overview

转换器的主路径是 **语法引导翻译**，不是先建 AST 再遍历。

`parser.y` 里每个非终结符的语义值是 `char*` HTML 片段。规约时根据产生式 type 调用统一出口，生成 003 规定的 tag + class。完整文档骨架（doctype、内联 `<style>` / `<script>`）由 `main.c` 按 003 包装，yacc 负责 `<body>` 内部。

依赖：[001 词法](../001-markdown-lexical-analyzer/README.md)。class 名以 [003](../003-markdown-html-converter/README.md) 为准。

## Design

### 工具

- 文件：`src/parser.y`
- `%define api.pure` 非必须；课程/作业级全局 `yyparse` 即可
- 冲突：`%expect` 必须写清原因；禁止靠默认 shift  silently 接受错误 Markdown

### 文法分层

```
document     → blocks
blocks       → blocks block | block | ε
block        → heading | paragraph | list | blockquote
             | fence | indent_code | table | hr | blank
heading      → ATX inline NEWLINE
paragraph    → inlines NEWLINE
list         → list_item+
blockquote   → BLOCKQUOTE blocks（续行规则可简化：每行都要有 >）
fence        → FENCE_OPEN fence_body FENCE_CLOSE
indent_code  → INDENT_CODE_LINE+
table        → table_row TABLE_SEP table_row+
inline       → TEXT | strong | em | strike | code_span
             | link | image | autolink | ESCAPE
```

列表、引用允许内部再出现 block（嵌套）。围栏 `fence_body` 只拼接字面量，不走 inline。

### 按 type 生成 HTML

所有 HTML 只从一处发出，避免每个规则手写不同的拼接格式：

```
char *html_tag(const char *type, const char *inner);
char *html_void(const char *type, const char *attrs);
```

`type` 是逻辑节点名（与 003 映射表 key 相同）。`html_tag` 查表得到 tag 名和 class，返回 `<tag class="...">inner</tag>`。

示例（动作伪代码，实现时写 C）：

```
heading: ATX inlines NEWLINE {
    $$ = html_heading($1 /* level */, $2);
    /* type = h1..h6 */
}

strong: STAR2 inlines STAR2 {
    $$ = html_tag("strong", $2);
}

ul: list_items {
    $$ = html_tag("ul", $1);
}
```

**禁止**在 lex 规则里 `printf("<h1>")`。**禁止**在 yacc 外再根据 AST type 生成一遍 HTML（辅助 `html_tag` 可以放 `.c`，调用必须在动作里）。

### 支持的 GFM 子集（v1）

| 结构 | 支持 | 备注 |
| ---- | ---- | ---- |
| ATX 标题 h1–h6 | 是 | |
| 段落、硬换行（行尾两空格或 `\`） | 是 | 软换行可做成空格或 `<br>`，与 003 一致 |
| `*` `_` 强调、`**` `__` 加粗 | 是 | 不支持任意复杂开闭歧义时，按成对 token 匹配 |
| `~~` 删除线 | 是 | GFM |
| 行内代码 | 是 | |
| 围栏代码块 + info string | 是 | info 放到 class 或 `data-lang`，见 003 |
| 缩进代码块（≥4 空格/tab） | 是 | CommonMark；无语言；HTML 同围栏 |
| 链接 `[text](url)`、图片 `![alt](url)` | 是 | |
| 自动链接 | 是 | |
| 无序/有序列表、任务列表 | 是 | GFM task |
| 引用 | 是 | 嵌套 `>` 可做一层或多层 |
| 表格 | 是 | GFM；对齐信息写入 class 或 style，见 003 |
| 水平线 | 是 | |
| Setext 标题 | 否 | 与 HR 冲突 |
| 参考式链接 | 否 | |
| 原始 HTML 块 | 否 | 当 TEXT 转义后输出 |
| 脚注、警告框等 GitHub 扩展 | 否 | |

### 冲突与消歧（必须在实现时落地）

- `*` 既是 `UL_MARK` 也是强调：只在 `LINE_START` 出 `UL_MARK`
- `-` 既是列表也是 `HR`：整行匹配 HR 优先
- `[` 开始链接还是普通文本：需要 `RBRACK LPAREN` 才归约为 link，否则 TEXT
- 段落 vs 空行：`BLANK` 结束当前 paragraph

### 内存

每个 `$$` 是新分配字符串；用完 `$1/$2` 后 `free`。`document` 的最终字符串交给 `main` 写出后释放。

## Plan

- [x] `parser.y`：`%union`、`%token` 与 001 对齐；`document` 可解析纯 TEXT 段落
- [x] 实现 `html_tag` / `html_heading` / `html_fence`，type 走 003 表
- [x] 块：标题、段落、HR、引用、列表、任务列表、围栏
- [x] 行内：em/strong/strike/code/link/image/autolink
- [x] 表格
- [x] 处理 shift/reduce；`%expect 21`（段落续行 vs 下一块；默认 shift）

## Test

- [x] 最小文档：`# A` → 含 `md-h1` 的 HTML
- [x] 强调成对匹配；不支持的嵌套不陷入死循环
- [x] 围栏内 Markdown 不被二次解析
- [x] 任务列表产出 `md-task` / `md-checkbox`
- [x] 表格至少 2 列、有分隔行

## Notes

Markdown 不是干净的 CFG。实现用词法状态 + 简化续行。v1 限制：

- 强调不嵌套（`**a *b* c**` 的内层 `*` 不是 em）
- 列表项之间空行会结束当前列表
- 未配对的 `*` / `**` 可能导致 parse error
- 表格行需以 `|` 开头

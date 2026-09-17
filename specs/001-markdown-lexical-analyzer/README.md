---
status: complete
created: 2026-08-28
priority: high
---

# Markdown 词法分析器

> **Status**: complete · **Priority**: high · **Created**: 2026-08-28

实现目录：`converter/src/lex.l`。

## Overview

Markdown 的块结构依赖**行首**、**缩进**、**空行**和**围栏状态**。用无状态正则一次性切词会把列表、引用、代码块切错。

本 spec 定义 Flex 的职责：识别 token、维护 start condition、把原始文本交给 yacc。HTML 与 CSS class 属于 [002](../002-markdown-syntax-analyzer/README.md) / [003](../003-markdown-html-converter/README.md)。

## Design

### 工具

- 文件：`src/lex.l`
- Flex 与 Bison 通过 `parser.tab.h` 共享 token 名与 `yylval`

### Start conditions

| 状态 | 何时进入 | 作用 |
| ---- | -------- | ---- |
| `LINE_START` | 每个换行后 | 识别标题 `#`、引用 `>`、列表标记、围栏、表格分隔、水平线 |
| `INLINE` | 行内内容开始后 | 强调、链接、图片、行内代码、普通文字 |
| `FENCE` | 打开 \`\`\` 或 ~~~ 后 | 直到闭合围栏，内容全部当字面量 |
| `CODE_SPAN` | 打开行内 backtick 后 | 直到对等数量的闭合 backtick |

行首空白（0–3 空格 vs ≥4 空格/tab）只在 `LINE_START` 解释：4 空格可能是缩进代码，需与围栏代码一起在文法里消歧，词法层先给出 `INDENT` / `SPACES` 信息。

### Token 集合

Bison `%token` 必须与下表一致。

**结构**

| Token | 识别 | 语义值 |
| ----- | ---- | ------ |
| `NEWLINE` | `\n` / `\r\n` | 无 |
| `BLANK` | 仅空白的一行 | 无 |
| `TEXT` | 不含特殊标记的连续字符 | `char*` 原文 |
| `ESCAPE` | `\` + ASCII 标点 | 被转义的字符 |

**块标记（仅 LINE_START 或明确块上下文）**

| Token | 识别 |
| ----- | ---- |
| `ATX` | 行首 1–6 个 `#`，后跟空格或行尾 |
| `BLOCKQUOTE` | 行首 `>` |
| `UL_MARK` | 行首 `*` / `-` / `+` + 空白 |
| `OL_MARK` | 行首 `digits.` 或 `digits)` + 空白 |
| `TASK_MARK` | 列表标记后的 `[ ]` / `[x]` / `[X]` |
| `FENCE_OPEN` / `FENCE_CLOSE` | 行首 ≥3 的 `` ` `` 或 `~`，可带 info string |
| `HR` | 行首 ≥3 的 `---` / `***` / `___`（可夹空格） |
| `PIPE` | `\|`（表格） |
| `TABLE_SEP` | `\| :--- \| --- \|` 这类分隔行，可在词法合并为单 token，或拆成 `PIPE` + `COLON` + `DASH` |

**行内标记**

| Token | 识别 |
| ----- | ---- |
| `STAR` / `STAR2` / `STAR3` | `*` / `**` / `***` |
| `UNDER` / `UNDER2` | `_` / `__` |
| `TILDE2` | `~~`（GFM 删除线） |
| `BACKTICK` | 1 个或多个 `` ` ``，`yylval` 带长度以便配对 |
| `LBRACK` / `RBRACK` | `[` `]` |
| `LPAREN` / `RPAREN` | `(` `)` |
| `BANG` | `!`（图片 `![`） |
| `LT` / `GT` | `<` `>`（autolink；块引用的 `>` 走 `BLOCKQUOTE`） |
| `AUTOLINK` | `<http(s)://...>` 或 GFM 裸 URL，可作为词法捷径 |

### yylval

```
%union {
    char *str;
    int   num;   /* ATX 级别、backtick 长度、任务框是否勾选 */
    struct Fence { char *info; char delim; int len; } fence;
}
```

所有 `char*` 由词法 `strdup`，由 yacc 动作 `free`。不要用 Flex 内部缓冲区指针跨规则保存。

### 不做的事

- 不输出 HTML
- 不在词法里配对 `**bold**` 或嵌套列表（交给 yacc）
- 不实现完整 CommonMark 的“容器延续行”全部边角；无法识别时退回 `TEXT`，并在 Notes 记录

## Plan

- [x] `lex.l` 骨架：`%option noyywrap yylineno`，include `parser.tab.h`
- [x] `LINE_START` / `INLINE` / `FENCE` / `IN_TICKS`（行内代码，避免与 token `CODE_SPAN` 撞名）
- [x] 块 token：ATX、引用、列表、任务框、围栏、HR、表格相关
- [x] 行内 token：强调、删除线、括号、autolink、TEXT、ESCAPE
- [x] 与 `parser.y` 的 `%token` / `%union` 对齐，能 `flex && bison && cc` 链接

## Test

- [x] `# Title` → `ATX` + `TEXT`
- [x] 空行 → `BLANK`
- [x] `**x**` → `STAR2` `TEXT` `STAR2`
- [x] 围栏内的 `#`、`*` 全部是围栏文本
- [x] `\*` → `ESCAPE` 而不是 `STAR`
- [x] `- [x] task` → `UL_MARK` + `TASK_MARK` + `TEXT`

## Notes

GFM 参考：[GitHub Flavored Markdown Spec](https://github.github.com/gfm/)。本项目不追求 100% CommonMark 用例通过；词法以“块标记在行首、行内标记在 INLINE、围栏隔离”为底线。

Setext 标题（`===` / `---` 下划线）与 `HR`、表格分隔冲突，若实现成本高可列入 002 的“暂不支持”。

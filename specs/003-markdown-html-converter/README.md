---
status: complete
created: 2026-08-28
priority: high
---

# Markdown HTML/CSS 转换

> 定义 yacc 必须遵守的 **type → HTML tag + CSS class** 契约，以及完整 HTML 文档与自定义样式表。实现：`converter/src/html.c`、`converter/src/main.c`、`converter/css/gfm.css`。

## Overview

用户要的不是裸标签，而是**可换肤的 HTML 文档**。yacc 按 type 返回带 class 的 tag（见 [002](../002-markdown-syntax-analyzer/README.md)）；本 spec 规定 class 名、属性、文档外壳和默认 CSS。换皮肤只改 `css/gfm.css`，不改文法。

## Design

### 完整文档

`main.c` 在 `yyparse` 成功后输出：

```
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>...</title>
  <style id="md-theme">…embedded or --css body…</style>
</head>
<body class="md-body">
  ... yacc 返回的片段 ...
</body>
</html>
```

- `--theme` 选择内置 CSS（`gfm` 默认，另有 `teal` / `vermillion` / `azure` / `lime` / `jade` / `tangerine`）；构建时把 `css/*.css` 与 `js/*.js` 编进二进制
- `--css path` 读入外部 CSS 并**内联**进 `<style>`（覆盖 `--theme`）
- 标题：第一个 `h1` 文本，否则文件名
- 失败：非 0 退出，不写半截 HTML

### type → tag + class

yacc 的 `html_tag(type, inner)` 只允许使用下表。class 前缀 `md-`，全部小写。

| type | HTML | class | 说明 |
| ---- | ---- | ----- | ---- |
| `h1`–`h6` | `h1`–`h6` | `md-h1`–`md-h6` | ATX 级别 |
| `p` | `p` | `md-p` | 段落 |
| `br` | `br` | `md-br` | 硬换行，void |
| `strong` | `strong` | `md-strong` | 加粗 |
| `em` | `em` | `md-em` | 强调 |
| `del` | `del` | `md-del` | 删除线 |
| `code` | `code` | `md-code` | 行内代码 |
| `pre` | `pre` | `md-pre` | 代码块外壳 |
| `code_block` | `code` | `md-code-block` | 放在 `pre` 内；有语言时追加 `md-lang-{lang}` |
| `blockquote` | `blockquote` | `md-blockquote` | |
| `ul` | `ul` | `md-ul` | |
| `ol` | `ol` | `md-ol` | |
| `li` | `li` | `md-li` | |
| `task_item` | `li` | `md-li md-task` | 任务项 |
| `checkbox` | `input` | `md-checkbox` | `type="checkbox" disabled`；勾选则 `checked` |
| `a` | `a` | `md-a` | `href` 必须转义 |
| `img` | `img` | `md-img` | `src`/`alt` 转义 |
| `table` | `table` | `md-table` | |
| `thead` | `thead` | `md-thead` | |
| `tbody` | `tbody` | `md-tbody` | |
| `tr` | `tr` | `md-tr` | |
| `th` | `th` | `md-th` | 对齐：`md-align-left/center/right` |
| `td` | `td` | `md-td` | 同上 |
| `hr` | `hr` | `md-hr` | void |
| `body` | （外壳） | `md-body` | 仅文档包装，不在块规则里生成 |

围栏 HTML 形状：

```
<pre class="md-pre"><span class="md-line-numbers" aria-hidden="true">1
2</span><code class="md-code-block md-lang-c">...</code></pre>
```

无语言则只有 `md-code-block`。有正文时在 `code` 前插入行号 gutter（`md-line-numbers`，行数与正文 `\n` 对齐）；空围栏不输出 gutter。行号不是 Markdown type，换肤时与 `.md-pre` / `.md-code-block` 一起样式化。

### 转义

所有来自源文的文本（含 href/src）按 HTML 转义：`&` `<` `>` `"`。行内代码和围栏同样转义，保证任意 Markdown 不能打出原始标签（本项目不支持 HTML 块）。

### 自定义 CSS

文件：`css/gfm.css`。只使用上表 class，不依赖 tag 选择器作为唯一样式（tag+class 可以组合，例如 `table.md-table`）。

默认主题应覆盖：

- 正文排版（字体、行高、最大宽度、居中）
- 标题层级
- 引用左边框
- 代码块背景与滚动（默认 Visual Studio Dark）；围栏行号（`md-line-numbers`）与代码行高对齐
- 围栏内 token 颜色（`md-tok-*`，与 `.md-pre` 背景一起设置）
- 表格边框与表头
- 链接颜色
- 任务列表 checkbox 与文字对齐
- 图片最大宽度 100%

换肤：复制 CSS 改 class 下的属性即可，不必改 yacc。

### CLI（建议）

```
md-convert input.md [-o outdir] [--theme name] [--css path] [--no-highlight] [--no-copy]
md-convert -d indir [-o outdir] [--theme name] [--css path] [--no-highlight] [--no-copy]
```

- `-d` / `--dir indir`：转换 `indir` 下所有 `*.md`（不递归子目录）；与单文件 `input.md` 二选一。
- `-o outdir`：输出**目录**；每个输入写出 `outdir/<basename>.html`。
- 未指定 `-o` 时写出与输入同目录、扩展名为 `.html` 的文件（`path/foo.md` → `path/foo.html`），不写 stdout。

内置主题（`--theme`，默认 `gfm`）：`gfm` / `teal` / `vermillion` / `azure` / `lime` / `jade` / `tangerine`，对应 `css/*.css`，构建时嵌入二进制。`--css path` 从文件读 CSS 并内联，覆盖 `--theme`。

围栏代码高亮：token 颜色写在主题 CSS 的 `.md-pre` / `.md-code-block` 旁；文档把嵌入的 `highlight.js` **内联**为 `<script id="md-highlight">…</script>`（认 `md-lang-*`）。`--no-highlight` 不写入该脚本。换肤时背景与高亮色一起换。不改 yacc 产出的 tag。

「复制到公众号」脚本同样内联（`id="md-copy-wechat-js"`）；`--no-copy` 关闭。生成 HTML **不依赖**外部 `css/`、`js/` 路径，单独分发二进制即可。

文档外壳：`body.md-body` 内包一层 `<div id="md-article">`（yacc 片段放其中），便于脚本选取正文。

### 微信公众号（浏览器复制）

磁盘 HTML **不**预写 `style=`（对齐用 `md-align-*` class），以便换肤。默认启用「复制到公众号」（`--no-copy` 关闭）：对 `#md-article` 按当前 CSS **计算样式临时内联** 并写入剪贴板。行为细则见 [004](../004-copy-to-wechat/README.md)。

## Plan

- [x] 把 type 映射做成 `html_tag()` 查表（`converter/src/html.c`）
- [x] `main.c` 包装完整 HTML，内联 CSS / JS（`--theme` / `--css`）
- [x] 编写 `converter/css/gfm.css`
- [x] 文本与属性转义
- [x] fixtures：标题/列表/代码/表格/链接 对照 class

## Test

- [x] 每个 v1 type 至少出现一次，class 与表一致（`tests/run_tests.py`）
- [x] `<script>` 写成转义文本
- [x] `--theme` / `--css` 只换样式内容，class 不变
- [x] 空输入：合法 HTML，`body.md-body` 为空

## Notes

class 一旦有测试依赖就视为稳定 API。改名必须同时改 002 动作、本表、CSS 和 fixtures。

不在磁盘 HTML 上内联 `style=`（对齐用 `md-align-*` class），以便自定义 CSS 能覆盖。复制到公众号时由浏览器脚本按计算样式临时内联，见 [004](../004-copy-to-wechat/README.md)。

代码高亮 token class（`md-tok-*`）由文档内脚本生成，颜色写在 `gfm.css` 的 `.md-code-block` 下，不属于上表 Markdown type。围栏行号 class（`md-line-numbers`）由 `html_fence()` 生成，同样不在上表。

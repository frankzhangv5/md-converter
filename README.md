# md-converter

用 **Flex / Bison** 把 [GitHub Flavored Markdown（GFM）](https://github.github.com/gfm/) 转成带自定义 CSS class 的 HTML，并支持在浏览器中「复制到微信公众号」。

实现约束与 Agent 约定见 [AGENTS.md](AGENTS.md)；构建与 CLI 细节见 [converter/README.md](converter/README.md)。

## LeanSpec 驱动的规格

本仓库用 [LeanSpec](https://github.com/codervisor/lean-spec) 做 **Spec-Driven Development**：先写短小、可验收的规格，再按依赖顺序实现，改行为时同步更新对应 spec。

| 约定 | 说明 |
| ---- | ---- |
| 规格目录 | `specs/`（见 [`.lean-spec/config.json`](.lean-spec/config.json)） |
| 单条规格 | `specs/<编号>-<名称>/README.md`，含 YAML frontmatter（`status` / `priority` / `created` 等） |
| 模板 | [`.lean-spec/templates/spec-template.md`](.lean-spec/templates/spec-template.md) |
| 工作流 | 读 spec → 实现 → 勾选 Plan/Test → 标 `complete`；行为变更必须回写 spec |

规格索引与状态：

| Spec | 状态 | 职责 |
| ---- | ---- | ---- |
| [001 词法分析](specs/001-markdown-lexical-analyzer/README.md) | complete | Flex 识别 GFM token、维护 start condition |
| [002 语法分析](specs/002-markdown-syntax-analyzer/README.md) | complete | Bison 文法；规约时按 type 生成带 class 的 HTML |
| [003 HTML/CSS 输出](specs/003-markdown-html-converter/README.md) | complete | 文档骨架、`md-*` class 契约、自定义主题 CSS |
| [004 复制到公众号](specs/004-copy-to-wechat/README.md) | complete | 浏览器计算样式内联、公众号兼容改写、剪贴板 |

依赖顺序：**001 → 002 → 003 → 004**。002 的语义动作遵守 003 的 class 约定；003 的 stylesheet 只样式化 002 会产出的标签；004 只约束浏览器复制瞬间，不改 yacc 产出。

完整列表见 [specs/README.md](specs/README.md)。

## 按规格实现的功能

### 001 / 002 — 解析与翻译（GFM 子集）

主路径是 **语法引导翻译**：词法产出稳定 token，文法规约时按 type 拼出 HTML，不做独立 AST visitor。

| 已支持 | 说明 |
| ------ | ---- |
| ATX 标题 | `h1`–`h6` |
| 段落与硬换行 | 行尾两空格或 `\` |
| 强调 / 加粗 | `*` `_` / `**` `__` |
| 删除线 | `~~…~~`（GFM） |
| 行内代码、围栏代码 | 围栏可带 info string → `md-lang-*` |
| 链接、图片、自动链接 | `[text](url)` / `![alt](url)` / URL |
| 无序 / 有序 / 任务列表 | 任务项 → `md-task` + checkbox |
| 引用、表格、水平线 | 表格对齐 → `md-align-*` |

暂不支持（spec 已标明）：Setext 标题、参考式链接、原始 HTML 块、脚注与警告框等。

### 003 — HTML 文档与换肤

- 输出完整 HTML（doctype、charset、viewport、`body.md-body`、`#md-article`）
- 统一 `type → tag + md-* class`（见 003 映射表）
- 源文 HTML 转义；磁盘 HTML **不**预写内联 `style=`，便于换肤
- 主题：内置 `gfm` / `teal` / `vermillion` / `azure` / `lime` / `jade` / `tangerine`（`--theme`）；`--css` 可读外部 CSS。样式与 `highlight.js` / `copy-wechat.js` 均**内联**进 HTML，二进制自包含
- 可选代码高亮（围栏 `md-tok-*`）；`--no-highlight` 关闭

### 004 — 复制到微信公众号

- 生成页默认挂「复制到公众号」按钮（`--no-copy` 关闭）
- 按**当前主题**的计算样式临时内联，写入剪贴板
- 公众号兼容：去掉文首 `h1`、围栏/列表改写、任务框换文本、图片居中等

## 快速开始

需要 `flex`、`bison`、C 编译器与 `python3`（嵌入资源）。在 `converter/` 下构建后运行：

```bash
# Linux / macOS
cd converter && make
./build/md-convert input.md -o outdir --theme gfm
./build/md-convert -d docs -o site --theme gfm

# Windows（MSYS2）
cd converter
powershell -ExecutionPolicy Bypass -File build.ps1
.\build\md-convert.exe input.md -o outdir --theme gfm
.\build\md-convert.exe -d docs -o site --theme gfm
```

浏览器打开生成的 HTML 即可预览；右下角可复制到公众号编辑器。更多选项与 class 一览见 [converter/README.md](converter/README.md)。

## 目录一览

```
specs/                 LeanSpec 规格（001–004）
converter/src/         lex.l · parser.y · html.c · main.c
converter/css/         主题样式（md-*）
converter/js/          highlight.js · copy-wechat.js
converter/tests/       fixtures 与回归
.lean-spec/            LeanSpec 配置与模板
AGENTS.md              实现约束（给人类与 Agent）
```

## License

[MIT](LICENSE)

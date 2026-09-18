# md-convert

用 Flex / Bison 把 GFM Markdown（GitHub Flavored Markdown） 转成带自定义 CSS class 的 HTML。

**CommonMark** 是通用 Markdown 的严格规范；**GFM** 在 CommonMark 之上增加 GitHub 常用扩展。主要区别：

| 特性 | CommonMark | GFM |
| ---- | ---------- | --- |
| 表格 | 无 | 支持 `\|` 管道表格 |
| 删除线 | 无 | 支持 `~~text~~` |
| 任务列表 | 仅为普通列表 | 支持 `- [ ]` / `- [x]` |
| 自动链接 | 需显式 `[text](url)` | 裸 URL（如 `https://…`）可自动识别为链接 |
| 围栏代码 info string | 支持 `` ```lang `` | 同左，并约定语言标识用于高亮等场景 |
| 消歧义 | 按规范处理边界 | 部分边界（如列表与强调嵌套）更贴近 GitHub 渲染 |

本转换器以 **GFM 子集**为目标（标题、段落、强调、链接、列表、引用、围栏代码、表格、删除线等）。

```
converter/
  src/lex.l parser.y html.c html.h main.c
  css/gfm.css teal.css vermillion.css azure.css lime.css jade.css tangerine.css
  js/highlight.js copy-wechat.js
  tools/embed_assets.py
  tests/fixtures/
  Makefile
  build.ps1
```

## 构建

需要 `flex`、`bison`、C 编译器（`gcc`）和 `python3`（构建时把 CSS/JS 嵌入二进制）。

### Ubuntu

```
sudo apt update
sudo apt install -y flex bison build-essential
make
```

`build-essential` 提供 `gcc` 与 `make`。

### Windows

依赖通过 [MSYS2](https://www.msys2.org/) 安装。`build.ps1` / `Makefile` 默认找 `D:\msys64`（`usr\bin` 的 flex/bison，`mingw64\bin` 的 gcc）；路径不同时用环境变量或 `MSYS=` 覆盖。

1. 安装 MSYS2，打开 **MSYS2 MINGW64** 终端。
2. 安装工具链：

```
pacman -Syu
pacman -S --needed flex bison make mingw-w64-x86_64-gcc
```

3. 在项目 `converter/` 目录构建：

```
powershell -ExecutionPolicy Bypass -File build.ps1
# 或（MSYS2 不在 D:\msys64 时）:
# $env:MSYS = "C:\msys64"; .\build.ps1
# 或: make MSYS=D:/msys64
```

## 运行

```
./build/md-convert input.md                  # 写出 input.html（默认 --theme gfm）
./build/md-convert input.md -o outdir        # 写出 outdir/input.html
./build/md-convert -d docs                   # 转换 docs/ 下全部 *.md → 同目录 .html
./build/md-convert -d docs -o site           # 转换 docs/*.md → site/*.html
./build/md-convert input.md -o outdir --theme gfm
./build/md-convert input.md -o outdir --theme teal
./build/md-convert input.md -o outdir --theme vermillion
./build/md-convert input.md -o outdir --theme azure
./build/md-convert input.md -o outdir --theme lime
./build/md-convert input.md -o outdir --theme jade
./build/md-convert input.md -o outdir --theme tangerine
./build/md-convert input.md -o outdir --css css/my-theme.css   # 自定义 CSS 内联
./build/md-convert input.md --no-highlight   # 不内联 highlight.js
./build/md-convert input.md --no-copy        # 不内联「复制到公众号」脚本
```

- `-d` / `--dir` 扫描目录中所有 `.md`（不递归），与单文件参数二选一。
- `-o` 为**输出目录**；HTML 文件名取输入 basename（`foo.md` → `foo.html`）。未指定时写到输入文件同目录。
- `--theme` 选用构建时嵌入二进制的主题 CSS（`gfm` / `teal` / `vermillion` / `azure` / `lime` / `jade` / `tangerine`），写入 `<style id="md-theme">`。
- `--css path` 从文件读 CSS 并内联（覆盖 `--theme`）。
- `highlight.js` / `copy-wechat.js` 同样嵌入二进制，以 `<script>…</script>` **内联**写入；单独拷贝二进制即可，不依赖仓库里的 `css/`、`js/`。
- `teal`（#009688）、`vermillion`（#f83929）为微信公众号阅读风；`azure`（#415FFF）、`lime`（#006B33）、`jade`（#07C160）、`tangerine`（#FF6A00）为基于 `gfm` 排版的强调色变体；多行代码块均为 Visual Studio Dark，`md-tok-*` 高亮色写在所选 CSS 里。

浏览器打开生成的 HTML 后，右下角有「复制到公众号」：按**当前 CSS 主题**把渲染结果（计算样式内联）写入剪贴板，再粘贴到公众号编辑器。换 `--theme` 即换复制样式。HTML 可放在任意目录打开，样式与脚本都已内联。

## 自定义样式

转换器**不改 HTML 标签名与 class**，只换样式表内容。换肤有两种方式：

1. **选用内置主题**（推荐）：

   ```
   ./build/md-convert input.md -o outdir --theme teal
   ```

2. **自己写主题**：复制一份现有 CSS，只改各 `.md-*` 下的属性，不要改 class 名，再用 `--css` 内联：

   ```
   cp css/gfm.css css/my-theme.css
   # 编辑 my-theme.css 里的颜色、字体、边距等
   ./build/md-convert input.md -o outdir --css css/my-theme.css
   ```

注意：

- 样式应通过 **`.md-*` class** 选择，而不是只靠裸标签（例如写 `.md-table`，不要只写 `table`）。
- 磁盘上的 HTML **不会**预写内联 `style=`，便于换肤；「复制到公众号」时才会按计算样式临时内联。
- 代码高亮颜色写在 `.md-code-block .md-tok-*` 下（Visual Studio Dark 色板），与主题一起换。

## CSS class 一览

所有 class 前缀为 `md-`，小写。生成 HTML 大致形如：

```html
<body class="md-body">
  <div id="md-article">
    <!-- 正文：各 md-* 标签 -->
  </div>
</body>
```

### 文档与块级

| class | 对应标签 | 含义 |
| ----- | -------- | ---- |
| `md-body` | `body` | 整页正文容器（字体、最大宽度、背景） |
| `md-h1` … `md-h6` | `h1` … `h6` | ATX 标题，级别对应 1–6 |
| `md-p` | `p` | 段落 |
| `md-br` | `br` | 硬换行（行末两个空格等） |
| `md-hr` | `hr` | 分隔线 |
| `md-blockquote` | `blockquote` | 引用块 |
| `md-ul` | `ul` | 无序列表 |
| `md-ol` | `ol` | 有序列表 |
| `md-li` | `li` | 列表项 |
| `md-task` | `li` | 任务列表项（与 `md-li` 同用：`md-li md-task`） |
| `md-checkbox` | `input` | 任务复选框（`disabled`；勾选则带 `checked`） |
| `md-pre` | `pre` | 围栏代码块外壳 |
| `md-line-numbers` | `span` | 围栏行号 gutter（`aria-hidden`；空围栏不输出） |
| `md-code-block` | `code` | 围栏内代码；有语言时再加 `md-lang-{lang}`（如 `md-lang-c`） |
| `md-table` | `table` | 表格 |
| `md-thead` | `thead` | 表头 |
| `md-tbody` | `tbody` | 表体 |
| `md-tr` | `tr` | 行 |
| `md-th` | `th` | 表头单元格 |
| `md-td` | `td` | 表体单元格 |
| `md-align-left` | `th`/`td` | 列左对齐 |
| `md-align-center` | `th`/`td` | 列居中 |
| `md-align-right` | `th`/`td` | 列右对齐 |
| `md-img` | `img` | 图片（建议限制 `max-width: 100%`） |

围栏代码形状示例：

```html
<pre class="md-pre"><span class="md-line-numbers" aria-hidden="true">1
2</span><code class="md-code-block md-lang-c">...</code></pre>
```

无语言信息时只有 `md-code-block`。有正文时带行号 gutter。

### 行内

| class | 对应标签 | 含义 |
| ----- | -------- | ---- |
| `md-strong` | `strong` | 加粗 |
| `md-em` | `em` | 斜体强调 |
| `md-del` | `del` | 删除线 |
| `md-code` | `code` | 行内代码 |
| `md-a` | `a` | 链接（`href` 已转义） |

### 代码高亮（由 `highlight.js` 注入，非 Markdown type）

| class | 含义 |
| ----- | ---- |
| `md-tok-kw` | 关键字 |
| `md-tok-str` | 字符串 |
| `md-tok-cmt` | 注释 |
| `md-tok-num` | 数字 |
| `md-tok-pre` | 预处理 / 特殊前缀类 token |

另有 `#md-article`：包住正文的 `div`，供「复制到公众号」脚本选取，一般不必改样式。

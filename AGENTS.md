# md-converter

用 Flex（lex）和 Bison（yacc）实现的 **GFM Markdown → 自定义 CSS HTML** 转换器。

词法分析由 Flex 完成；语法分析与 HTML 生成由 Bison 完成。yacc 语义动作按节点 type 返回带 CSS class 的 HTML 标签，再包装成完整 HTML 文档。

## 目标

- 输入：GitHub Flavored Markdown（GFM）文本
- 输出：带自定义 stylesheet 的 HTML 文档
- 实现约束：
  - 词法：`flex` / `lex.l`
  - 语法与翻译：`bison` / `parser.y`
  - **HTML 标签在 yacc 里按 type 生成**，不要另做独立 AST visitor 作为主路径

## 规格索引

实现前先读对应 spec，改行为时同步更新。

| Spec | 职责 | 依赖 |
| ---- | ---- | ---- |
| [001 词法分析](specs/001-markdown-lexical-analyzer/README.md) | Flex 识别 GFM token | 无 |
| [002 语法分析](specs/002-markdown-syntax-analyzer/README.md) | Bison 文法 + 按 type 生成带 class 的 HTML | 001 |
| [003 HTML/CSS 输出](specs/003-markdown-html-converter/README.md) | 文档骨架、class 命名、自定义 CSS | 002 |
| [004 复制到公众号](specs/004-copy-to-wechat/README.md) | 浏览器内联 style + 剪贴板，兼容公众号粘贴 | 003 |

工作顺序：**001 → 002 → 003 → 004**。002 的语义动作必须遵守 003 的 class 约定；003 的 stylesheet 只样式化 002 实际会产出的标签；004 只约束浏览器复制瞬间的行为，不改 yacc 产出。

## 目录约定

实现放在 `converter/`：

    converter/src/lex.l          Flex 词法
    converter/src/parser.y       Bison 文法与 HTML 语义动作
    converter/src/html.c         type → tag/class（yacc 调用）
    converter/src/main.c         读入、yyparse、写出完整 HTML
    converter/css/gfm.css        自定义样式（class 与 003 对齐）
    converter/js/copy-wechat.js  浏览器内「复制到公众号」按钮
    converter/js/highlight.js    围栏代码高亮（浏览器内联）
    converter/tests/fixtures/    样例 .md
    converter/Makefile           flex/bison/cc 构建
    converter/build.ps1          Windows 构建
    specs/                       需求与设计

生成物（`lex.yy.c`、`parser.tab.c`、二进制）不要手改，也不要提交。

## 实现原则

1. **Token 要细、文法要稳**：Flex 产出稳定 token；块结构、嵌套、列表/引用/代码围栏由 Bison 处理。
2. **翻译在规约时完成**：每个非终结符 `$n` 是 HTML 字符串；`$$` 按 type 拼出带 class 的 tag。
3. **class 单一来源**：标签与 class 只以 003 的映射表为准，禁止词法阶段直接吐 HTML。
4. **GFM 子集先跑通**：优先标题、段落、强调、链接、列表、引用、围栏代码、表格、删除线；无法用 LALR(1) 干净表达的边角（如复杂 HTML 块、参考式链接）在 spec 里标明“暂不支持”，不要 silently 错解析。
5. **Markdown 对缩进和空行敏感**：Flex 用 start condition 区分行首/行中/围栏内；不要把整份文档当无状态正则切完。

## 对 Agent 的要求

- 先读本文件和相关 spec，再改 `lex.l` / `parser.y`。
- 改 token 时同步 `%token` 与 001；改 HTML 形状时同步 002/003；改「复制到公众号」行为时同步 004。
- 不要往 spec 里塞 LeanSpec、MCP、board/search 之类与本项目无关的流程。
- 不要手写大段生成 C 代码来绕过 Flex/Bison；辅助函数可以放在 `src/` 的 `.c/.h`，但 type → tag 的分发必须在 yacc 动作里可见。

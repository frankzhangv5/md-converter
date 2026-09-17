# Specs

md-convert 的设计规格。按依赖顺序实现，不要跳步。

| 编号 | 名称 | 状态 | 内容 |
| ---- | ---- | ---- | ---- |
| [001](001-markdown-lexical-analyzer/README.md) | 词法分析器 | complete | Flex 扫描 GFM，产出 token |
| [002](002-markdown-syntax-analyzer/README.md) | 语法分析器 | complete | Bison 文法；规约时按 type 返回带 CSS class 的 HTML |
| [003](003-markdown-html-converter/README.md) | HTML/CSS 转换 | complete | 完整 HTML 文档 + 自定义 CSS；class 命名契约 |
| [004](004-copy-to-wechat/README.md) | 复制到公众号 | complete | 浏览器计算样式内联、公众号兼容改写、剪贴板 |

整体目标、目录与实现约束见仓库根目录 [AGENTS.md](../AGENTS.md)。

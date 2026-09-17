# GFM 全量测试

覆盖本转换器 v1 支持的 GitHub Flavored Markdown：ATX 标题、段落与换行、强调、删除线、行内代码、链接/图片/自动链接、列表与任务列表、引用、围栏代码、表格、水平线、转义。

## Heading 2

### Heading 3

#### Heading 4

##### Heading 5

###### Heading 6

## 段落与换行

普通段落，含中文、ASCII、以及数字 123。

软换行第一行
软换行第二行，中间应合并为空格。

硬换行（行尾两空格）：  
下一行。

反斜杠硬换行：\
也到下一行。

## 行内强调

星号：*italic* 与 **bold** 与 ***bold-italic***。

下划线：_italic_ 与 __bold__。

删除线：~~deleted~~。

行内代码：`code`、`a * b`、以及双反引号包一层 `` `tick` ``。

转义星号：\*not-em\* 与 \*\*not-bold\*\*。

HTML 当文本：<script>alert(1)</script> 与 <div class="x">。

## 链接与图片

行内链接 [example](https://example.com) 与带路径 [query](https://example.com/path?q=1)。

自动链接尖括号 <https://example.com>。

裸 URL： https://github.com/github/docs

图片：![logo](https://7cmb.com/images/local.jpg)

空 alt：![](https://7cmb.com/images/local.jpg)

## 无序列表

- dash one
- dash two

* star one
* star two

+ plus one
+ plus two

## 有序列表

1. first
2. second
3. third

1) paren first
2) paren second

## 任务列表

- [ ] unchecked
- [x] checked lower
- [X] checked upper

## 引用

> 单层引用第一行
> 单层引用第二行

> 引用里的 **bold** 与 `code`

> # 引用内标题

> - 引用内列表

> 外层引用
> > 内层嵌套引用

## 围栏代码

多行 C（info string `c`，内容不当 Markdown 再解析）：

```c
#include <stdio.h>

int add(int a, int b)
{
    /* # not a heading */
    /* * not a list */
    return a + b;
}
```

```python
def hello():
    print("hi")
```

```
no language
still code
```

~~~js
const n = 1;
~~~

## 表格

| left | center | right |
| :--- | :----: | ----: |
| a | b | c |
| **bold** | `code` | [ex](https://example.com) |
| 中文 | 100 | ~~old~~ |

## 水平线

---

***

___

## 收尾段落

混合：A **bold** *em* ~~del~~ `code` [ok](https://example.com) 结束。

v1 不覆盖（未写入正文以免误解析）：Setext 标题、参考式链接、原始 HTML 块、脚注。

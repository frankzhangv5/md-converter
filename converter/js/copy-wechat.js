(function () {
  "use strict";

  var ARTICLE_ID = "md-article";
  var BTN_ID = "md-copy-wechat";
  var TOAST_ID = "md-copy-toast";

  /*
   * Do NOT inline height/width: 公众号字体与行宽和浏览器不一致，
   * 固定 height 会导致行重叠。flex/grid 也不要原样带入，见 wechatDisplay。
   */
  var PROPS = [
    "color",
    "background-color",
    "background-image",
    "font-family",
    "font-size",
    "font-weight",
    "font-style",
    "line-height",
    "letter-spacing",
    "text-align",
    "text-decoration",
    "text-decoration-line",
    "text-decoration-color",
    "text-indent",
    "white-space",
    "word-break",
    "word-wrap",
    "overflow-wrap",
    "margin-top",
    "margin-right",
    "margin-bottom",
    "margin-left",
    "padding-top",
    "padding-right",
    "padding-bottom",
    "padding-left",
    "border-top-width",
    "border-right-width",
    "border-bottom-width",
    "border-left-width",
    "border-top-style",
    "border-right-style",
    "border-bottom-style",
    "border-left-style",
    "border-top-color",
    "border-right-color",
    "border-bottom-color",
    "border-left-color",
    "border-radius",
    "display",
    "list-style-type",
    "list-style-position",
    "vertical-align",
    "max-width",
    "box-shadow",
    "opacity"
  ];

  var defaultCache = Object.create(null);

  function defaultsFor(tag) {
    var t = tag.toLowerCase();
    if (defaultCache[t]) return defaultCache[t];
    var probe = document.createElement(t);
    document.body.appendChild(probe);
    var cs = getComputedStyle(probe);
    var map = Object.create(null);
    var i;
    for (i = 0; i < PROPS.length; i++) {
      map[PROPS[i]] = cs.getPropertyValue(PROPS[i]);
    }
    document.body.removeChild(probe);
    defaultCache[t] = map;
    return map;
  }

  function rgbToHex(v) {
    var m = /^rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)(?:\s*,\s*([\d.]+))?\s*\)$/i.exec(
      v
    );
    if (!m) return v;
    var a = m[4] === undefined ? 1 : parseFloat(m[4]);
    if (a === 0) return "transparent";
    if (a < 1) return v;
    function h(n) {
      var s = parseInt(n, 10).toString(16);
      return s.length === 1 ? "0" + s : s;
    }
    return "#" + h(m[1]) + h(m[2]) + h(m[3]);
  }

  function isUseless(prop, val) {
    if (!val || val === "none" || val === "normal") return true;
    /* margin auto is how .md-img centers; must keep for WeChat */
    if (val === "auto") {
      if (prop === "margin-left" || prop === "margin-right") return false;
      return true;
    }
    if (val === "0px" && prop.indexOf("border") === 0 && prop.indexOf("width") >= 0)
      return true;
    if (
      (val === "rgba(0, 0, 0, 0)" || val === "transparent") &&
      prop.indexOf("background") === 0
    )
      return true;
    /* WeChat ignores/mangles these; keep layout flowing */
    if (prop === "overflow" || prop === "box-shadow") return true;
    return false;
  }

  /* 公众号对 flex/grid 支持差，改成可预测的 block/inline。 */
  function wechatDisplay(tag, display) {
    var d = (display || "").toLowerCase();
    if (d === "flex" || d === "inline-flex" || d === "grid" || d === "inline-grid") {
      if (tag === "SPAN" || tag === "CODE" || tag === "A" || tag === "STRONG" ||
          tag === "EM" || tag === "DEL")
        return "inline";
      return "block";
    }
    return display;
  }

  function styleAttr(parts) {
    return parts.filter(Boolean).join(";");
  }

  /* WeChat mangles pixel width/height and margin:auto on img. */
  function styleImage(liveEl, cloneEl) {
    var cs = getComputedStyle(liveEl);
    var radius = cs.borderRadius;
    var parts = [
      "display:inline-block",
      "max-width:100%",
      "width:auto",
      "height:auto",
      "vertical-align:middle",
      "margin:0"
    ];
    if (radius && radius !== "0px") parts.push("border-radius:" + radius);
    cloneEl.setAttribute("style", parts.join(";"));
    cloneEl.removeAttribute("class");
    cloneEl.removeAttribute("id");
    if (liveEl.src) cloneEl.setAttribute("src", liveEl.src);
  }

  /*
   * Wrap each img in a centered block so layout matches browser (display:block +
   * margin:auto) without relying on margin:auto, which 公众号 often strips.
   */
  function wrapImagesForWechat(root) {
    var imgs = root.querySelectorAll("img");
    var i;
    for (i = 0; i < imgs.length; i++) {
      var img = imgs[i];
      if (
        img.parentNode &&
        img.parentNode.getAttribute &&
        img.parentNode.getAttribute("data-md-img-wrap") === "1"
      )
        continue;
      var wrap = document.createElement("span");
      wrap.setAttribute("data-md-img-wrap", "1");
      wrap.setAttribute(
        "style",
        "display:block;width:100%;text-align:center;margin:10px 0;"
      );
      img.parentNode.insertBefore(wrap, img);
      wrap.appendChild(img);
    }
  }

  /* Snapshot element children before mutating, so checkbox replace cannot desync. */
  function applyComputedSafe(liveEl, cloneEl) {
    var tag = liveEl.tagName;
    if (tag === "SCRIPT" || tag === "STYLE") return;
    if (tag === "BR") {
      cloneEl.removeAttribute("class");
      return;
    }
    if (tag === "INPUT" && liveEl.type === "checkbox") {
      cloneEl.parentNode.replaceChild(
        document.createTextNode(liveEl.checked ? "☑ " : "☐ "),
        cloneEl
      );
      return;
    }
    if (tag === "IMG") {
      styleImage(liveEl, cloneEl);
      return;
    }

    var cs = getComputedStyle(liveEl);
    var def = defaultsFor(tag);
    var parts = [];
    var i, prop, val;
    for (i = 0; i < PROPS.length; i++) {
      prop = PROPS[i];
      val = cs.getPropertyValue(prop);
      if (val === def[prop]) continue;
      if (isUseless(prop, val)) continue;
      if (prop === "display") val = wechatDisplay(tag, val);
      /* break-all 会把 --css 拆开；不要用 nowrap/inline-block（会触发 li 首子换行） */
      if (tag === "CODE" && prop === "word-break" && val === "break-all")
        val = "normal";
      if (prop.indexOf("color") >= 0 || prop === "background-color")
        val = rgbToHex(val);
      parts.push(prop + ":" + val);
    }
    if (parts.length) cloneEl.setAttribute("style", styleAttr(parts));
    cloneEl.removeAttribute("class");
    cloneEl.removeAttribute("id");

    var liveEls = [];
    var cloneEls = [];
    for (i = 0; i < liveEl.children.length; i++) liveEls.push(liveEl.children[i]);
    for (i = 0; i < cloneEl.children.length; i++) cloneEls.push(cloneEl.children[i]);
    var n = Math.min(liveEls.length, cloneEls.length);
    for (i = 0; i < n; i++) applyComputedSafe(liveEls[i], cloneEls[i]);
  }

  /*
   * 公众号会丢掉 white-space:pre / 表格单元格里的换行。
   * 把围栏拆成「每行一个 block」：行号 + 代码，用 <br> 不再依赖 pre。
   */
  function pickStyle(style, prop, fallback) {
    if (!style) return fallback;
    var re = new RegExp(
      "(?:^|;)\\s*" + prop.replace(/[.*+?^${}()|[\]\\]/g, "\\$&") + ":\\s*([^;]+)",
      "i"
    );
    var m = re.exec(style);
    return m ? m[1].trim() : fallback;
  }

  function textNewlinesToBr(root) {
    var texts = [];
    var walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, null);
    var n;
    while ((n = walker.nextNode())) texts.push(n);
    var i, t, parts, frag, j;
    for (i = 0; i < texts.length; i++) {
      t = texts[i];
      if (!t.nodeValue || t.nodeValue.indexOf("\n") < 0) continue;
      parts = t.nodeValue.split("\n");
      frag = document.createDocumentFragment();
      for (j = 0; j < parts.length; j++) {
        if (j) frag.appendChild(document.createElement("br"));
        if (parts[j].length) frag.appendChild(document.createTextNode(parts[j]));
      }
      t.parentNode.replaceChild(frag, t);
    }
  }

  function splitByBr(container) {
    var lines = [document.createDocumentFragment()];
    var nodes = [];
    var i;
    for (i = 0; i < container.childNodes.length; i++)
      nodes.push(container.childNodes[i]);
    for (i = 0; i < nodes.length; i++) {
      if (nodes[i].nodeName === "BR") {
        lines.push(document.createDocumentFragment());
      } else {
        lines[lines.length - 1].appendChild(nodes[i]);
      }
    }
    return lines;
  }

  function fencesForWechat(root) {
    var pres = root.querySelectorAll("pre");
    var i;
    for (i = 0; i < pres.length; i++) {
      var pre = pres[i];
      var nums = null;
      var code = null;
      var c;
      for (c = 0; c < pre.children.length; c++) {
        var kid = pre.children[c];
        if (kid.tagName === "CODE") code = kid;
        else if (kid.tagName === "SPAN" && !nums) nums = kid;
      }
      if (!code) continue;

      var preStyle = pre.getAttribute("style") || "";
      var codeStyle = code.getAttribute("style") || "";
      var numsStyle = nums ? nums.getAttribute("style") || "" : "";
      var bgc = pickStyle(
        codeStyle,
        "background-color",
        pickStyle(preStyle, "background-color", "#1e1e1e")
      );
      var fgc = pickStyle(codeStyle, "color", pickStyle(preStyle, "color", "#d4d4d4"));
      var rad = pickStyle(preStyle, "border-radius", "5px");
      var ff = pickStyle(
        codeStyle,
        "font-family",
        "Consolas,'Courier New',monospace"
      );
      var fs = pickStyle(codeStyle, "font-size", "12px");
      var lh = pickStyle(codeStyle, "line-height", "1.6");
      var numColor = pickStyle(numsStyle, "color", "#858585");

      var work = document.createElement("div");
      while (code.firstChild) work.appendChild(code.firstChild);
      textNewlinesToBr(work);
      var codeLines = splitByBr(work);

      var numList = nums
        ? (nums.textContent || "").replace(/\r/g, "").split("\n")
        : [];
      while (numList.length < codeLines.length) numList.push(String(numList.length + 1));
      if (codeLines.length === 0) codeLines.push(document.createDocumentFragment());

      var box = document.createElement("section");
      box.setAttribute(
        "style",
        styleAttr([
          "margin:10px 0",
          "padding:12px 14px",
          "background-color:" + bgc,
          "color:" + fgc,
          "border-radius:" + rad,
          "font-family:" + ff,
          "font-size:" + fs,
          "line-height:" + lh,
          "text-align:left",
          "overflow:auto",
          "word-break:normal"
        ])
      );

      var rowStyle = styleAttr([
        "margin:0",
        "padding:0",
        "line-height:" + lh,
        "font-family:" + ff,
        "font-size:" + fs,
        "color:" + fgc,
        "white-space:normal",
        "word-wrap:break-word"
      ]);
      var numStyle = styleAttr([
        "display:inline-block",
        "min-width:2em",
        "margin-right:12px",
        "color:" + numColor,
        "text-align:right",
        "font-family:" + ff,
        "font-size:" + fs,
        "line-height:" + lh,
        "vertical-align:top",
        "user-select:none"
      ]);

      var r;
      for (r = 0; r < codeLines.length; r++) {
        var row = document.createElement("p");
        row.setAttribute("style", rowStyle);
        if (nums) {
          var ns = document.createElement("span");
          ns.setAttribute("style", numStyle);
          ns.textContent = numList[r] != null && numList[r] !== ""
            ? numList[r]
            : String(r + 1);
          row.appendChild(ns);
        }
        var body = document.createElement("span");
        body.setAttribute(
          "style",
          styleAttr([
            "color:" + fgc,
            "font-family:" + ff,
            "font-size:" + fs,
            "line-height:" + lh
          ])
        );
        body.appendChild(codeLines[r]);
        if (!body.childNodes.length)
          body.appendChild(document.createTextNode(" "));
        row.appendChild(body);
        box.appendChild(row);
      }

      if (pre.parentNode) pre.parentNode.replaceChild(box, pre);
    }
  }

  /*
   * 公众号：li 标记 + 首个子元素（code/strong）常被拆成两行。
   * 复制前把 ul/ol 收成「• / 1.」文本前缀的 <p>，不再依赖 list-marker。
   */
  function listsForWechat(root) {
    var guard = 0;
    while (guard++ < 40) {
      var lists = root.querySelectorAll("ul, ol");
      if (!lists.length) break;
      var list = null;
      var i;
      for (i = 0; i < lists.length; i++) {
        if (!lists[i].querySelector("ul, ol")) {
          list = lists[i];
          break;
        }
      }
      if (!list) list = lists[lists.length - 1];
      flattenOneList(list);
    }
  }

  function flattenOneList(list) {
    var ordered = list.tagName === "OL";
    var parent = list.parentNode;
    var items = [];
    var i;
    for (i = 0; i < list.children.length; i++) {
      if (list.children[i].tagName === "LI") items.push(list.children[i]);
    }
    var idx = 0;
    for (i = 0; i < items.length; i++) {
      var li = items[i];
      idx++;
      var st = li.getAttribute("style") || "";
      st = st
        .replace(/(?:^|;)\s*display\s*:\s*[^;]*/gi, "")
        .replace(/(?:^|;)\s*list-style[^;]*/gi, "")
        .replace(/^;+|;+$/g, "");
      var p = document.createElement("p");
      p.setAttribute(
        "style",
        styleAttr([
          st,
          "display:block",
          "margin:5px 0",
          "padding-left:0",
          "text-align:left"
        ])
      );
      p.appendChild(
        document.createTextNode(ordered ? idx + ".\u00A0" : "•\u00A0")
      );
      while (li.firstChild) {
        var ch = li.firstChild;
        li.removeChild(ch);
        if (ch.nodeType === 1 && ch.tagName === "P") {
          var nest = ch.getAttribute("style") || "";
          ch.setAttribute(
            "style",
            styleAttr([nest, "padding-left:1.5em", "margin:3px 0"])
          );
          parent.insertBefore(ch, list);
        } else {
          p.appendChild(ch);
        }
      }
      parent.insertBefore(p, list);
    }
    parent.removeChild(list);
  }

  /*
   * 仅替换 '-' → U+2011，不加 nowrap/inline-block。
   */
  function protectInlineCodeHyphens(root) {
    var codes = root.querySelectorAll("code");
    var i, code, texts, t, w, n;
    for (i = 0; i < codes.length; i++) {
      code = codes[i];
      texts = [];
      w = document.createTreeWalker(code, NodeFilter.SHOW_TEXT, null);
      while ((n = w.nextNode())) texts.push(n);
      for (t = 0; t < texts.length; t++) {
        if (texts[t].nodeValue && texts[t].nodeValue.indexOf("-") >= 0)
          texts[t].nodeValue = texts[t].nodeValue.replace(/-/g, "\u2011");
      }
    }
  }

  /*
   * strong/em 后紧跟的「（…）」不要并进加粗标签（否则括号也会变粗）。
   * 用 nowrap span 包住二者，减少公众号在标签边界断行。
   * 列表已收成「1.␣…」文本前缀，此 span 不是段落首子元素，可安全 nowrap。
   */
  function wrapTrailingParen(root) {
    var nodes = root.querySelectorAll("strong, em, a");
    var list = [];
    var i, el, next, m, span;
    for (i = 0; i < nodes.length; i++) list.push(nodes[i]);
    for (i = 0; i < list.length; i++) {
      el = list[i];
      next = el.nextSibling;
      if (!next || next.nodeType !== 3 || !next.nodeValue) continue;
      m = /^(（[^）]*）[:：]?)/.exec(next.nodeValue);
      if (!m) continue;
      span = document.createElement("span");
      span.setAttribute("style", "white-space:nowrap");
      el.parentNode.insertBefore(span, el);
      span.appendChild(el);
      span.appendChild(document.createTextNode(m[1]));
      next.nodeValue = next.nodeValue.slice(m[1].length);
    }
  }

  /* Drop leading article title (first top-level h1); WeChat has its own title field. */
  function stripArticleTitle(root) {
    var kids = root.children;
    var i;
    for (i = 0; i < kids.length; i++) {
      if (kids[i].tagName === "H1") {
        root.removeChild(kids[i]);
        return;
      }
      /* skip leading blank text wrappers — only strip if h1 is first element */
      break;
    }
  }

  /*
   * 公众号粘贴会丢掉「元素之间」的普通空格文本节点，导致
   * </strong> <em> 粘成 boldem。仅当空白夹在两个元素兄弟之间时，
   * 把空格换成 \u00A0；换行/制表仍保留，避免破坏块结构。
   */
  function protectInterElementSpaces(root) {
    var walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, null);
    var nodes = [];
    var n;
    while ((n = walker.nextNode())) nodes.push(n);
    var i, text, prev, next, v;
    for (i = 0; i < nodes.length; i++) {
      text = nodes[i];
      v = text.nodeValue;
      if (!v || !/^[ \t\r\n]+$/.test(v)) continue;
      if (v.indexOf(" ") < 0) continue;
      prev = text.previousSibling;
      next = text.nextSibling;
      if (!prev || !next || prev.nodeType !== 1 || next.nodeType !== 1) continue;
      text.nodeValue = v.replace(/ /g, "\u00A0");
    }
  }

  function sectionStyle(article) {
    var cs = getComputedStyle(article);
    var parts = [];
    var i, prop, val;
    for (i = 0; i < PROPS.length; i++) {
      prop = PROPS[i];
      val = cs.getPropertyValue(prop);
      if (
        isUseless(prop, val) &&
        prop !== "font-family" &&
        prop !== "font-size" &&
        prop !== "line-height" &&
        prop !== "color" &&
        prop !== "text-align" &&
        prop !== "background-color"
      )
        continue;
      if (prop.indexOf("color") >= 0 || prop === "background-color")
        val = rgbToHex(val);
      parts.push(prop + ":" + val);
    }
    return parts.join(";");
  }

  function buildCopyHtml(article) {
    var clone = article.cloneNode(true);
    stripArticleTitle(clone);
    applyComputedFromLive(article, clone);
    fencesForWechat(clone);
    listsForWechat(clone);
    protectInlineCodeHyphens(clone);
    wrapTrailingParen(clone);
    wrapImagesForWechat(clone);
    protectInterElementSpaces(clone);
    var wrap = document.createElement("section");
    wrap.setAttribute("style", sectionStyle(article));
    while (clone.firstChild) wrap.appendChild(clone.firstChild);
    return wrap.outerHTML;
  }

  /*
   * Walk live (in-document) and clone in parallel, skipping the first top-level h1
   * on both sides so indices stay aligned after stripArticleTitle.
   */
  function applyComputedFromLive(liveRoot, cloneRoot) {
    var liveKids = [];
    var cloneKids = [];
    var i;
    var skipped = false;
    for (i = 0; i < liveRoot.children.length; i++) {
      if (!skipped && liveRoot.children[i].tagName === "H1") {
        skipped = true;
        continue;
      }
      liveKids.push(liveRoot.children[i]);
    }
    for (i = 0; i < cloneRoot.children.length; i++)
      cloneKids.push(cloneRoot.children[i]);
    var n = Math.min(liveKids.length, cloneKids.length);
    for (i = 0; i < n; i++) applyComputedSafe(liveKids[i], cloneKids[i]);
  }

  function copyViaExec(html) {
    var box = document.createElement("div");
    box.setAttribute("contenteditable", "true");
    box.style.cssText =
      "position:fixed;left:-9999px;top:0;opacity:0;pointer-events:none;";
    box.innerHTML = html;
    document.body.appendChild(box);
    var range = document.createRange();
    range.selectNodeContents(box);
    var sel = window.getSelection();
    sel.removeAllRanges();
    sel.addRange(range);
    var ok = false;
    try {
      ok = document.execCommand("copy");
    } catch (e) {
      ok = false;
    }
    sel.removeAllRanges();
    document.body.removeChild(box);
    return ok;
  }

  function toast(msg, ok) {
    var el = document.getElementById(TOAST_ID);
    if (!el) {
      el = document.createElement("div");
      el.id = TOAST_ID;
      document.body.appendChild(el);
    }
    el.textContent = msg;
    el.style.cssText =
      "position:fixed;z-index:100000;left:50%;bottom:88px;transform:translateX(-50%);" +
      "padding:10px 18px;border-radius:8px;font:14px/1.4 sans-serif;" +
      "color:#fff;background:" +
      (ok ? "#009688" : "#c62828") +
      ";box-shadow:0 4px 16px rgba(0,0,0,.2);";
    clearTimeout(el._t);
    el._t = setTimeout(function () {
      if (el.parentNode) el.parentNode.removeChild(el);
    }, 2200);
  }

  function onCopy() {
    var article = document.getElementById(ARTICLE_ID);
    if (!article) {
      toast("未找到正文", false);
      return;
    }
    var html;
    try {
      html = buildCopyHtml(article);
    } catch (e) {
      toast("准备复制失败", false);
      return;
    }
    var plainBox = article.cloneNode(true);
    stripArticleTitle(plainBox);
    var plain = plainBox.innerText || plainBox.textContent || "";

    if (navigator.clipboard && window.ClipboardItem) {
      try {
        var item = new ClipboardItem({
          "text/html": new Blob([html], { type: "text/html" }),
          "text/plain": new Blob([plain], { type: "text/plain" })
        });
        navigator.clipboard.write([item]).then(
          function () {
            toast("已复制，可粘贴到公众号", true);
          },
          function () {
            if (copyViaExec(html)) toast("已复制，可粘贴到公众号", true);
            else toast("复制失败，请手动全选复制", false);
          }
        );
        return;
      } catch (e) {
        /* fall through */
      }
    }
    if (copyViaExec(html)) toast("已复制，可粘贴到公众号", true);
    else toast("复制失败，请手动全选复制", false);
  }

  function mountButton() {
    if (document.getElementById(BTN_ID)) return;
    var btn = document.createElement("button");
    btn.id = BTN_ID;
    btn.type = "button";
    btn.textContent = "复制到公众号";
    btn.title = "复制当前主题渲染后的正文，粘贴到微信公众号编辑器";
    btn.style.cssText =
      "position:fixed;z-index:99999;right:24px;bottom:24px;" +
      "padding:12px 18px;border:0;border-radius:999px;cursor:pointer;" +
      "font:600 14px/1.2 \"Microsoft YaHei\",sans-serif;color:#fff;" +
      "background:#009688;box-shadow:0 4px 14px rgba(0,150,136,.45);";
    btn.addEventListener("click", onCopy);
    document.body.appendChild(btn);
  }

  function boot() {
    mountButton();
  }

  if (document.readyState === "loading")
    document.addEventListener("DOMContentLoaded", boot);
  else boot();
})();

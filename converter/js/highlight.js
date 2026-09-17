(function () {
  "use strict";

  var ALIAS = {
    c: "c", h: "c", cpp: "c", cc: "c", cxx: "c", hpp: "c",
    python: "python", py: "python",
    javascript: "js", js: "js", typescript: "js", ts: "js",
    java: "java",
    go: "go",
    rust: "rust", rs: "rust",
    json: "json",
    bash: "bash", sh: "bash", shell: "bash", zsh: "bash",
    html: "html", xml: "html",
    css: "css"
  };

  var KW = {
    c: "alignas alignof auto bool break case catch char class const consteval constexpr continue default delete do double else enum explicit export extern false float for friend goto if inline int long mutable namespace new noexcept nullptr operator private protected public register return short signed sizeof static static_cast struct switch template this throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while",
    python: "False None True and as assert async await break class continue def del elif else except finally for from global if import in is lambda nonlocal not or pass raise return try while with yield",
    js: "async await break case catch class const continue debugger default delete do else export extends false finally for function if import in instanceof let new null of return static super switch this throw true try typeof undefined var void while with yield",
    java: "abstract assert boolean break byte case catch char class const continue default do double else enum extends final finally float for if implements import instanceof int interface long native new package private protected public return short static super switch synchronized this throw throws transient try void volatile while true false null",
    go: "break case chan const continue default defer else fallthrough for func go goto if import interface map package range return select struct switch type var true false nil iota",
    rust: "as async await break const continue crate dyn else enum extern false fn for if impl in let loop match mod move mut pub ref return self Self static struct super trait true type unsafe use where while",
    bash: "case coproc do done elif else esac fi for function if in select then time until while true false",
    json: "true false null"
  };

  function esc(s) {
    return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
  }

  function wrap(cls, s) {
    return '<span class="' + cls + '">' + esc(s) + "</span>";
  }

  function kwSet(lang) {
    var src = KW[lang];
    if (!src) return null;
    var o = Object.create(null);
    src.split(" ").forEach(function (w) {
      if (w) o[w] = 1;
    });
    return o;
  }

  function atLineStart(code, i) {
    var k = i;
    while (k > 0 && code.charAt(k - 1) !== "\n") {
      var p = code.charAt(k - 1);
      if (p !== " " && p !== "\t") return false;
      k--;
    }
    return true;
  }

  function highlight(code, lang) {
    var kws = kwSet(lang);
    var i = 0;
    var n = code.length;
    var out = "";
    var py = lang === "python";
    var html = lang === "html";
    var css = lang === "css";
    var bash = lang === "bash";
    var clike = lang === "c" || lang === "java" || lang === "js" || lang === "go" || lang === "rust";

    function starts(s) {
      return code.substr(i, s.length) === s;
    }

    while (i < n) {
      var c = code.charAt(i);

      if (html && starts("<!--")) {
        var he = code.indexOf("-->", i + 4);
        he = he < 0 ? n : he + 3;
        out += wrap("md-tok-cmt", code.slice(i, he));
        i = he;
        continue;
      }
      if (html && c === "<") {
        var gt = code.indexOf(">", i + 1);
        gt = gt < 0 ? n - 1 : gt;
        out += wrap("md-tok-kw", code.slice(i, gt + 1));
        i = gt + 1;
        continue;
      }

      if ((clike || css) && starts("/*")) {
        var be = code.indexOf("*/", i + 2);
        be = be < 0 ? n : be + 2;
        out += wrap("md-tok-cmt", code.slice(i, be));
        i = be;
        continue;
      }
      if ((clike || css) && starts("//")) {
        var le = code.indexOf("\n", i);
        le = le < 0 ? n : le;
        out += wrap("md-tok-cmt", code.slice(i, le));
        i = le;
        continue;
      }
      if ((py || bash) && c === "#") {
        var pe = code.indexOf("\n", i);
        pe = pe < 0 ? n : pe;
        out += wrap("md-tok-cmt", code.slice(i, pe));
        i = pe;
        continue;
      }
      if (lang === "c" && c === "#" && atLineStart(code, i)) {
        var pre = code.indexOf("\n", i);
        pre = pre < 0 ? n : pre;
        out += wrap("md-tok-pre", code.slice(i, pre));
        i = pre;
        continue;
      }

      if (py && (starts('"""') || starts("'''"))) {
        var t = code.substr(i, 3);
        var te = code.indexOf(t, i + 3);
        te = te < 0 ? n : te + 3;
        out += wrap("md-tok-str", code.slice(i, te));
        i = te;
        continue;
      }
      if (lang === "js" && c === "`") {
        var j = i + 1;
        while (j < n) {
          if (code.charAt(j) === "\\" && j + 1 < n) {
            j += 2;
            continue;
          }
          if (code.charAt(j) === "`") {
            j++;
            break;
          }
          j++;
        }
        out += wrap("md-tok-str", code.slice(i, j));
        i = j;
        continue;
      }
      if (c === '"' || c === "'") {
        var q = c;
        var k = i + 1;
        while (k < n) {
          if (code.charAt(k) === "\\" && k + 1 < n) {
            k += 2;
            continue;
          }
          if (code.charAt(k) === q) {
            k++;
            break;
          }
          if (c === "'" && py === false && lang !== "js" && code.charAt(k) === "\n") break;
          k++;
        }
        out += wrap("md-tok-str", code.slice(i, k));
        i = k;
        continue;
      }

      if (/[0-9]/.test(c)) {
        var m = i;
        if (code.substr(i, 2) === "0x" || code.substr(i, 2) === "0X") m += 2;
        while (m < n && /[0-9A-Fa-f_.]/.test(code.charAt(m))) m++;
        out += wrap("md-tok-num", code.slice(i, m));
        i = m;
        continue;
      }

      if (/[A-Za-z_]/.test(c)) {
        var u = i + 1;
        while (u < n && /[A-Za-z0-9_]/.test(code.charAt(u))) u++;
        var w = code.slice(i, u);
        if (kws && kws[w]) out += wrap("md-tok-kw", w);
        else out += esc(w);
        i = u;
        continue;
      }

      out += esc(c);
      i++;
    }
    return out;
  }

  function langOf(el) {
    var m = el.className.match(/(?:^|\s)md-lang-([a-z0-9_+-]+)/i);
    return m ? m[1].toLowerCase() : "";
  }

  function boot() {
    var nodes = document.querySelectorAll("code.md-code-block");
    var i;
    for (i = 0; i < nodes.length; i++) {
      var el = nodes[i];
      var raw = langOf(el);
      if (!raw) continue;
      var lang = ALIAS[raw] || raw;
      if (!KW[lang] && lang !== "html" && lang !== "css") continue;
      el.innerHTML = highlight(el.textContent, lang);
    }
  }

  if (document.readyState === "loading")
    document.addEventListener("DOMContentLoaded", boot);
  else boot();
})();

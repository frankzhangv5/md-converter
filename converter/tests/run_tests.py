#!/usr/bin/env python3
"""Check md-convert fixtures against spec 003 class names."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
BIN = BUILD / ("md-convert.exe" if sys.platform == "win32" else "md-convert")
FIXTURES = ROOT / "tests" / "fixtures"


def run(md: Path, extra: list[str] | None = None) -> str:
    out = BUILD / f"_test_{md.stem}.html"
    cmd = [str(BIN), str(md), "-o", str(out)]
    if extra:
        cmd.extend(extra)
    r = subprocess.run(
        cmd,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        cwd=ROOT,
    )
    if r.returncode != 0:
        raise SystemExit(f"{md.name} failed ({r.returncode}):\n{r.stderr}")
    try:
        return out.read_text(encoding="utf-8")
    finally:
        out.unlink(missing_ok=True)


def main() -> None:
    if not BIN.exists():
        raise SystemExit(f"missing binary: {BIN} (run make first)")

    empty = run(FIXTURES / "empty.md")
    assert "<body class=\"md-body\">" in empty
    assert 'id="md-article"' in empty
    assert "<!DOCTYPE html>" in empty

    heading = run(FIXTURES / "heading.md")
    assert "<h1 class=\"md-h1\">" in heading
    assert "<title>Sample Title</title>" in heading

    esc = run(FIXTURES / "escape.md")
    if "&lt;script&gt;" not in esc:
        raise SystemExit("script was not escaped")
    if "<script>alert" in esc:
        raise SystemExit("raw <script>alert leaked")

    br = run(FIXTURES / "hardbreak.md")
    assert "md-br" in br

    html = run(FIXTURES / "sample.md")
    needed = [
        "md-h1",
        "md-h2",
        "md-h3",
        "md-h4",
        "md-h5",
        "md-h6",
        "md-p",
        "md-br",
        "md-strong",
        "md-em",
        "md-del",
        "md-code",
        "md-ul",
        "md-ol",
        "md-li",
        "md-task",
        "md-checkbox",
        "md-blockquote",
        "md-pre",
        "md-line-numbers",
        "md-code-block",
        "md-lang-c",
        "md-table",
        "md-thead",
        "md-tbody",
        "md-tr",
        "md-th",
        "md-td",
        "md-align-left",
        "md-align-center",
        "md-align-right",
        "md-a",
        "md-img",
        "md-hr",
        "md-body",
    ]
    missing = [c for c in needed if c not in html]
    if missing:
        raise SystemExit("missing classes: " + ", ".join(missing) + "\n" + html)
    if "&lt;script&gt;" not in html:
        raise SystemExit("fixture <script> was not escaped")
    if "<script>alert" in html:
        raise SystemExit("raw <script>alert leaked into body")
    if 'id="md-highlight"' not in html:
        raise SystemExit("highlighter script missing")
    if 'id="md-copy-wechat-js"' not in html:
        raise SystemExit("copy-wechat script missing")
    if 'src="js/highlight.js"' in html or 'src="js/copy-wechat.js"' in html:
        raise SystemExit("JS should be inlined, not linked via src")
    if 'rel="stylesheet"' in html or "<link " in html:
        raise SystemExit("CSS should be inlined in <style>, not linked")
    if 'id="md-theme"' not in html or "<style" not in html:
        raise SystemExit("theme <style> missing")
    if ".md-body" not in html:
        raise SystemExit("default theme CSS not inlined")
    if "var ALIAS" not in html:
        raise SystemExit("highlight.js body not inlined")
    if "var BTN_ID" not in html:
        raise SystemExit("copy-wechat.js body not inlined")
    if "copy-code.js" in html or 'id="md-copy-code-js"' in html:
        raise SystemExit("copy-code script should not be present")
    if "highlight.css" in html:
        raise SystemExit("highlight.css should not be linked")
    if "# not a heading" not in html and "not a heading" not in html:
        raise SystemExit("fenced code lost body text")

    teal = run(FIXTURES / "sample.md", ["--theme", "teal"])
    if "#009688" not in teal:
        raise SystemExit("--theme teal did not inline teal.css")
    bad = subprocess.run(
        [str(BIN), str(FIXTURES / "sample.md"), "-o", str(BUILD / "_bad.html"), "--theme", "nope"],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        cwd=ROOT,
    )
    if bad.returncode == 0:
        raise SystemExit("unknown --theme should fail")

    plain = run(FIXTURES / "sample.md", ["--no-highlight"])
    if 'id="md-highlight"' in plain:
        raise SystemExit("--no-highlight still inlined highlighter")
    if 'id="md-copy-wechat-js"' not in plain:
        raise SystemExit("--no-highlight removed copy-wechat")

    nocopy = run(FIXTURES / "sample.md", ["--no-copy"])
    if 'id="md-copy-wechat-js"' in nocopy:
        raise SystemExit("--no-copy still inlined copy-wechat")
    if 'id="md-highlight"' not in nocopy:
        raise SystemExit("--no-copy removed highlighter")

    default_out = FIXTURES / "empty.html"
    if default_out.exists():
        default_out.unlink()
    r = subprocess.run(
        [str(BIN), str(FIXTURES / "empty.md")],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        cwd=ROOT,
    )
    if r.returncode != 0:
        raise SystemExit(f"default -o path failed ({r.returncode}):\n{r.stderr}")
    if r.stdout.strip():
        raise SystemExit("default output wrote to stdout; expected sidecar .html")
    if not default_out.is_file():
        raise SystemExit("default output missing empty.html next to empty.md")
    default_html = default_out.read_text(encoding="utf-8")
    default_out.unlink(missing_ok=True)
    if "<!DOCTYPE html>" not in default_html:
        raise SystemExit("default sidecar HTML incomplete")

    print("ok")


if __name__ == "__main__":
    main()

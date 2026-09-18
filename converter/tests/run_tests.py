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
    out = BUILD / f"{md.stem}.html"
    cmd = [str(BIN), str(md), "-o", str(BUILD)]
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

    under = run(FIXTURES / "underscore_path.md")
    if "converter/tools/embed_assets.py" not in under:
        raise SystemExit("path with underscore was mangled")
    if "foo_bar" not in under:
        raise SystemExit("snake_case was mangled")
    if '<em class="md-em">italic</em>' not in under:
        raise SystemExit("_italic_ did not become em")
    if '<strong class="md-strong">bold</strong>' not in under:
        raise SystemExit("__bold__ did not become strong")
    if "embed<em" in under:
        raise SystemExit("underscore inside path opened emphasis")

    edge = run(FIXTURES / "inline_edge.md")
    if "UL_MARK" not in edge or "block_image" not in edge:
        raise SystemExit("CJK-adjacent snake_case mangled")
    if "md-*" not in edge or "html_*" not in edge:
        raise SystemExit("unpaired markers not kept as text")
    # Only the Keep line should contain emphasis/del from markers.
    unpaired_para = edge
    if "md-<em" in unpaired_para or "html<em" in unpaired_para:
        raise SystemExit("unpaired * opened emphasis")
    if '<del class="md-del">del</del>' not in edge:
        raise SystemExit("~~del~~ did not become del")
    if "~~~" not in edge:
        raise SystemExit("loose ~~~ was eaten")

    li_hb = run(FIXTURES / "list_hardbreak.md")
    if "md-ol" not in li_hb or "md-li" not in li_hb:
        raise SystemExit("list with hardbreak missing ol/li")
    if "trailing spaces end item" not in li_hb or "next item" not in li_hb:
        raise SystemExit("hardbreak list items missing text")

    indented = run(FIXTURES / "indented_code.md")
    if "md-pre" not in indented or "md-code-block" not in indented:
        raise SystemExit("indented code missing pre/code_block")
    if "line one" not in indented or "line two" not in indented:
        raise SystemExit("indented code lines missing")
    if "line one line two" in indented:
        raise SystemExit("indented code merged into paragraph")
    if "path/with_under/file.py" not in indented:
        raise SystemExit("indented path with underscore missing")
    if '<p class="md-p">Before:</p>' not in indented:
        raise SystemExit("text before indented code mangled")
    if '<p class="md-p">After.</p>' not in indented:
        raise SystemExit("text after indented code mangled")

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
    for name, swatch in (
        ("azure", "#415fff"),
        ("lime", "#006b33"),
        ("jade", "#07c160"),
        ("tangerine", "#ff6a00"),
    ):
        themed = run(FIXTURES / "sample.md", ["--theme", name])
        if swatch not in themed:
            raise SystemExit(f"--theme {name} did not inline {name}.css")
    bad = subprocess.run(
        [str(BIN), str(FIXTURES / "sample.md"), "-o", str(BUILD), "--theme", "nope"],
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
        raise SystemExit(f"default output path failed ({r.returncode}):\n{r.stderr}")
    if "<!DOCTYPE" in r.stdout or "<html" in r.stdout.lower():
        raise SystemExit("default output wrote HTML to stdout; expected sidecar .html")
    if "md-convert: ok:" not in r.stdout:
        raise SystemExit("expected success status on stdout")
    if not default_out.is_file():
        raise SystemExit("default output missing empty.html next to empty.md")
    default_html = default_out.read_text(encoding="utf-8")
    default_out.unlink(missing_ok=True)
    if "<!DOCTYPE html>" not in default_html:
        raise SystemExit("default sidecar HTML incomplete")

    batch_dir = BUILD / "_batch_in"
    batch_out = BUILD / "_batch_out"
    if batch_dir.exists():
        for p in batch_dir.iterdir():
            p.unlink()
        batch_dir.rmdir()
    if batch_out.exists():
        for p in batch_out.iterdir():
            p.unlink()
        batch_out.rmdir()
    batch_dir.mkdir(parents=True)
    (batch_dir / "a.md").write_text("# A\n", encoding="utf-8")
    (batch_dir / "b.md").write_text("# B\n", encoding="utf-8")
    (batch_dir / "skip.txt").write_text("nope\n", encoding="utf-8")
    r = subprocess.run(
        [str(BIN), "-d", str(batch_dir), "-o", str(batch_out)],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        cwd=ROOT,
    )
    if r.returncode != 0:
        raise SystemExit(f"-d batch failed ({r.returncode}):\n{r.stderr}")
    if not (batch_out / "a.html").is_file() or not (batch_out / "b.html").is_file():
        raise SystemExit("-d/-o did not write expected HTML files")
    if (batch_out / "skip.html").exists():
        raise SystemExit("-d converted non-markdown file")
    a_html = (batch_out / "a.html").read_text(encoding="utf-8")
    if "<h1 class=\"md-h1\">A</h1>" not in a_html:
        raise SystemExit("-d batch HTML content wrong")
    for p in batch_dir.iterdir():
        p.unlink()
    batch_dir.rmdir()
    for p in batch_out.iterdir():
        p.unlink()
    batch_out.rmdir()

    print("ok")


if __name__ == "__main__":
    main()

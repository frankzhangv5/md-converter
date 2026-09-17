$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$Msys = if ($env:MSYS) { $env:MSYS } else { "D:\msys64" }
$env:PATH = "$Msys\usr\bin;$Msys\mingw64\bin;$env:PATH"

$Flex = Get-Command flex -ErrorAction SilentlyContinue
$Bison = Get-Command bison -ErrorAction SilentlyContinue
$Gcc = Get-Command gcc -ErrorAction SilentlyContinue
if (-not $Flex -or -not $Bison -or -not $Gcc) {
    Write-Error "Need flex, bison, and gcc on PATH (MSYS=$Msys)"
}

$Python = Get-Command python -ErrorAction SilentlyContinue
if (-not $Python) {
    $Python = Get-Command python3 -ErrorAction SilentlyContinue
}
if (-not $Python) {
    Write-Error "Need python or python3 on PATH (for embedding css/js)"
}

New-Item -ItemType Directory -Force -Path "$Root\build" | Out-Null
& $Python.Source "$Root\tools\embed_assets.py" -o "$Root\build" --root "$Root"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& flex -o "$Root\build\lex.yy.c" "$Root\src\lex.l"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& bison -d -v -o "$Root\build\parser.tab.c" "$Root\src\parser.y"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -Wall -Wextra -O2 -I"$Root\src" -I"$Root\build" -o "$Root\build\md-convert.exe" `
    "$Root\src\main.c" "$Root\src\html.c" "$Root\build\embedded_assets.c" `
    "$Root\build\lex.yy.c" "$Root\build\parser.tab.c"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "built $Root\build\md-convert.exe"

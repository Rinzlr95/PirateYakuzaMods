param([string]$Zig = 'zig')
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $version = & $Zig version
    if ($LASTEXITCODE -ne 0 -or $version.Trim() -ne '0.14.1') {
        throw 'Install Zig 0.14.1 and pass its zig.exe path with -Zig.'
    }
    New-Item -ItemType Directory -Force -Path build | Out-Null
    $buildArgs = @(
        'cc', '-target', 'x86_64-windows-gnu', '-shared', '-Os', '-s',
        '-std=c11', '-Wall', '-Wextra', '-Werror', '-Iminhook/include',
        'PirateAffixes.c', 'minhook/src/buffer.c', 'minhook/src/hook.c',
        'minhook/src/trampoline.c', 'minhook/src/hde/hde64.c',
        '-lbcrypt', '-ladvapi32', '-lcomctl32', '-luser32', '-lgdi32',
        '-o', 'build/PirateAffixes.asi'
    )
    & $Zig @buildArgs
    if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE." }
    Get-FileHash -Algorithm SHA256 -LiteralPath build/PirateAffixes.asi
} finally {
    Pop-Location
}

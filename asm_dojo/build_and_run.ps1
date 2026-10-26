# Build and run every asm_dojo demo with MinGW-w64 GCC.
# Usage:  .\build_and_run.ps1
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $root
$failed = @()
try {
    Get-ChildItem -Filter '*.c' |
        Where-Object { $_.Name -match '^\d\d_' } |
        Sort-Object Name |
        ForEach-Object {
            $exe = [IO.Path]::ChangeExtension($_.Name, 'exe')
            Write-Host "== $($_.Name) =="
            & gcc -O2 -std=c11 -Wall -Wextra $_.Name -o $exe
            if ($LASTEXITCODE -ne 0) {
                $failed += $_.Name
                return
            }
            & ".\$exe"
            if ($LASTEXITCODE -ne 0) {
                $failed += "$($_.Name) (run)"
            }
        }
}
finally {
    Pop-Location
}
if ($failed.Count -gt 0) {
    Write-Host "FAILED: $($failed -join ', ')"
    exit 1
}
Write-Host "All demos completed."

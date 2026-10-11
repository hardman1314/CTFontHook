# to_gbk.ps1 - Re-encode CTFontHook .bat scripts to GBK(codepage 936) + CRLF.
#
# Why: cmd.exe mis-parses multi-line Chinese batch files when the codepage is
# 65001 (known defect). The stable combination is a GBK (936) file on the
# default Chinese Windows console, with no `chcp` line.
#
# Workflow for editing any script under scripts\:
#   1. Edit the .bat as UTF-8 (that is what editors and AI tools produce).
#   2. Run this tool to convert every scripts\*.bat back to GBK + CRLF:
#        powershell -NoProfile -ExecutionPolicy Bypass -File scripts\tools\to_gbk.ps1
#
# The tool is idempotent: running it twice changes nothing.
param(
    [string]$Dir = (Join-Path $PSScriptRoot '..')
)
$ErrorActionPreference = 'Stop'

try { [System.Text.Encoding]::RegisterProvider([System.Text.CodePagesEncodingProvider]::Instance) } catch { }

$gbk  = [System.Text.Encoding]::GetEncoding(936)
$utf8 = New-Object System.Text.UTF8Encoding($false)

Get-ChildItem -Path $Dir -Filter *.bat | ForEach-Object {
    $p = $_.FullName
    $bytes = [System.IO.File]::ReadAllBytes($p)

    # Already GBK? (has no valid UTF-8 multi-byte sequence for our Chinese text)
    # Simple heuristic: try strict UTF-8 decode; fall back to GBK.
    $text = $null
    try {
        $strict = New-Object System.Text.UTF8Encoding($false, $true)
        $text = $strict.GetString($bytes)
    } catch {
        $text = $gbk.GetString($bytes)
    }

    $text = $text -replace "`r`n", "`n"
    $lines = @($text -split "`n")
    $out = New-Object System.Collections.ArrayList
    foreach ($line in $lines) {
        if ($line -match '^\s*chcp\s+65001') { continue }
        [void]$out.Add($line)
    }
    $newText = ($out.ToArray() -join "`r`n")
    [System.IO.File]::WriteAllText($p, $newText, $gbk)
    Write-Host ("GBK: " + $_.Name)
}

param(
    [string]$DllPath = ""
)

$ErrorActionPreference = "Stop"
if (-not $DllPath) {
    $DllPath = Join-Path $PSScriptRoot "..\build\Release\EmpfThumbs.dll"
}
$DllPath = (Resolve-Path $DllPath).Path

Write-Host "Unregistering $DllPath"
cmd.exe /c "`"$env:SystemRoot\System32\regsvr32.exe`" /u /s `"$DllPath`" & exit /b %ERRORLEVEL%"
if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
    throw "regsvr32 /u failed with exit code $LASTEXITCODE"
}
Write-Host "Unregistered."

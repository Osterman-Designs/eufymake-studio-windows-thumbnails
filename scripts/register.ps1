param(
    [string]$DllPath = ""
)

$ErrorActionPreference = "Stop"
if (-not $DllPath) {
    $DllPath = Join-Path $PSScriptRoot "..\build\Release\EmpfThumbs.dll"
}
$DllPath = (Resolve-Path $DllPath).Path

Write-Host "Registering $DllPath"
cmd.exe /c "`"$env:SystemRoot\System32\regsvr32.exe`" /s `"$DllPath`" & exit /b %ERRORLEVEL%"
if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
    throw "regsvr32 failed with exit code $LASTEXITCODE"
}

# Refresh Explorer associations without killing the shell.
Start-Process -FilePath "$env:SystemRoot\System32\ie4uinit.exe" -ArgumentList "-show" -WindowStyle Hidden -Wait -ErrorAction SilentlyContinue
Write-Host "Registered. Open a folder of .empf files in icon/extra-large view, or use the Preview pane."
Write-Host "If old generic icons remain, press F5 or sign out once so the thumbnail cache refreshes."

# --- Diagnostics Start ---
$uproject = Join-Path $env:PACKAGE_DIR "HostProject\HostProject.uproject"
$pluginBinaries = Join-Path $env:PACKAGE_DIR "HostProject\Plugins\UEJackAudioLink\Binaries\Win64"
$logFile = Join-Path $env:PACKAGE_DIR "AutomationTest.log"

Write-Host "--- Verifying paths ---"
Write-Host "uproject path: $uproject"
Write-Host "Plugin binaries path: $pluginBinaries"
Write-Host "Log file will be at: $logFile"

Write-Host "--- Checking for uproject existence ---"
if (Test-Path $uproject) {
    Write-Host "OK: HostProject.uproject found."
} else {
    Write-Host "ERROR: HostProject.uproject NOT found at $uproject"
    exit 1
}

Write-Host "--- Listing packaged plugin binaries ---"
if (Test-Path $pluginBinaries) {
    Get-ChildItem -Path $pluginBinaries -Filter *.dll | ForEach-Object { Write-Host "Found DLL: $($_.Name)" }
} else {
    Write-Host "WARNING: Plugin binaries directory not found at $pluginBinaries"
}
# --- Diagnostics End ---


& "$env:UE_PATH\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
      $uproject `
      -unattended -nopause -nullrhi -nosound `
      -log="$logFile" `
      -ExecCmds="Automation RunTests UEJackAudioLink.*; Quit"

# --- More Diagnostics ---
Write-Host "--- Unreal Editor log output ---"
if (Test-Path $logFile) {
    Get-Content $logFile
} else {
    Write-Host "Log file was not created."
}
# --- End Diagnostics ---


if ($LASTEXITCODE -ne 0) {
    throw "AutomationTests failed with code $LASTEXITCODE. Check the log output above for details."
}
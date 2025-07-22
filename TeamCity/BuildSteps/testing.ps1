# Create a minimal test project to run automation tests
$testProjectDir = Join-Path $env:PACKAGE_DIR "TestProject"
$testProjectFile = Join-Path $testProjectDir "TestProject.uproject"
$pluginDir = Join-Path $testProjectDir "Plugins\UEJackAudioLink"

Write-Host "--- Creating test project ---"
Write-Host "Test project dir: $testProjectDir"

# Create directory structure
New-Item -Path $testProjectDir -ItemType Directory -Force
New-Item -Path (Join-Path $testProjectDir "Plugins") -ItemType Directory -Force

# Create minimal .uproject file (no custom modules)
$uprojectContent = @"
{
    "FileVersion": 3,
    "EngineAssociation": "5.6",
    "Category": "",
    "Description": "",
    "Plugins": [
        {
            "Name": "UEJackAudioLink",
            "Enabled": true
        }
    ]
}
"@

Set-Content -Path $testProjectFile -Value $uprojectContent

# Copy plugin source from checkout directory (not the built package)
Write-Host "--- Copying plugin source to test project ---"
$sourcePluginDir = Get-Location  # Current working directory is the checkout
Write-Host "Source plugin directory: $sourcePluginDir"
Write-Host "Target plugin directory: $pluginDir"

# Copy all source files except build artifacts
Copy-Item -Path "$sourcePluginDir\*" -Destination $pluginDir -Recurse -Force -Exclude @("Build", ".git", "TeamCity")

# Verify the plugin was copied
Write-Host "--- Verifying plugin copy ---"
$pluginFile = Join-Path $pluginDir "UEJackAudioLink.uplugin"
if (Test-Path $pluginFile) {
    Write-Host "OK: Plugin file found at $pluginFile"
} else {
    Write-Host "ERROR: Plugin file not found at $pluginFile"
    Write-Host "Contents of plugin directory:"
    if (Test-Path $pluginDir) {
        Get-ChildItem -Path $pluginDir -Recurse | ForEach-Object { Write-Host "  $($_.FullName)" }
    } else {
        Write-Host "  Plugin directory does not exist"
    }
    throw "Plugin copy failed"
}

Write-Host "--- Building test project ---"
$logFile = Join-Path $env:PACKAGE_DIR "BuildTest.log"

& "$env:UE_PATH\Engine\Binaries\ThirdParty\DotNet\8.0.300\win-x64\dotnet.exe" `
    "$env:UE_PATH\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" `
    Development Win64 `
    -Project="$testProjectFile" `
    -TargetType=Editor `
    -Progress -NoHotReloadFromIDE

if ($LASTEXITCODE -ne 0) {
    throw "Failed to build test project with code $LASTEXITCODE"
}

Write-Host "--- Running automation tests ---"
$logFile = Join-Path $env:PACKAGE_DIR "AutomationTest.log"

& "$env:UE_PATH\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
      "$testProjectFile" `
      -unattended -nopause -nullrhi -nosound `
      -log="$logFile" `
      -ExecCmds="Automation RunTests UEJackAudioLink.*; Quit"

# Print the log for debugging
Write-Host "--- Unreal Editor log output ---"
if (Test-Path $logFile) {
    Get-Content $logFile
} else {
    Write-Host "Log file was not created."
}

if ($LASTEXITCODE -ne 0) {
    throw "AutomationTests failed with code $LASTEXITCODE. Check the log output above for details."
}
# Create a minimal test project to run automation tests
$testProjectDir = Join-Path $env:PACKAGE_DIR "TestProject"
$testProjectFile = Join-Path $testProjectDir "TestProject.uproject"
$pluginDir = Join-Path $testProjectDir "Plugins\UEJackAudioLink"

Write-Host "--- Creating test project ---"
Write-Host "Test project dir: $testProjectDir"
# Clean up any existing test project
if (Test-Path $testProjectDir) {
    Remove-Item -Path $testProjectDir -Recurse -Force
}

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

# Copy only the essential plugin files
$itemsToCopy = @("Source", "Resources", "UEJackAudioLink.uplugin")
foreach ($item in $itemsToCopy) {
    $sourcePath = Join-Path $sourcePluginDir $item
    if (Test-Path $sourcePath) {
        Write-Host "Copying $item from $sourcePath to $pluginDir..."
        
        if ($item -eq "UEJackAudioLink.uplugin") {
            # Copy the plugin file directly
            Copy-Item -Path $sourcePath -Destination $pluginDir -Force
        } else {
            # Copy directories
            $targetPath = Join-Path $pluginDir $item
            Copy-Item -Path $sourcePath -Destination $targetPath -Recurse -Force
        }
        
        Write-Host "  Copied successfully"
    } else {
        Write-Host "Warning: $item not found at $sourcePath"
    }
}

# Show what was actually copied
Write-Host "--- Contents after copy ---"
if (Test-Path $pluginDir) {
    Get-ChildItem -Path $pluginDir -Recurse | ForEach-Object { 
        $relativePath = $_.FullName.Replace($pluginDir, "")
        Write-Host "  $relativePath"
    }
} else {
    Write-Host "Plugin directory doesn't exist!"
}

# Verify the plugin was copied
Write-Host "--- Verifying plugin copy ---"
$pluginFile = Join-Path $pluginDir "UEJackAudioLink.uplugin"
if (Test-Path $pluginFile) {
    Write-Host "OK: Plugin file found at $pluginFile"
    
    # Check for Source directory
    $sourceDir = Join-Path $pluginDir "Source"
    if (Test-Path $sourceDir) {
        Write-Host "OK: Source directory found"
        Write-Host "Source contents:"
        Get-ChildItem -Path $sourceDir | ForEach-Object { Write-Host "  $($_.Name)" }
    } else {
        Write-Host "ERROR: Source directory not found"
        throw "Plugin Source directory missing"
    }
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

Write-Host "--- Skipping build (using pre-built plugin from packaging step) ---"
# The plugin was already built in the packaging step, so we can use those binaries directly

Write-Host "--- Building plugin modules for testing ---"
# Only build the plugin modules, not the entire editor
& "$env:UE_PATH\Engine\Binaries\ThirdParty\DotNet\8.0.300\win-x64\dotnet.exe" `
    "$env:UE_PATH\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" `
    UnrealEditor `
    Win64 `
    Development `
    -Project="$testProjectFile" `
    -Plugin="$pluginDir\UEJackAudioLink.uplugin" `
    -Rocket

if ($LASTEXITCODE -ne 0) {
    throw "Failed to build plugin modules with code $LASTEXITCODE"
}

Write-Host "--- Running automation tests ---"
$logFile = Join-Path $env:PACKAGE_DIR "AutomationTest.log"

# Use more robust automation testing arguments
& "$env:UE_PATH\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
      "$testProjectFile" `
      -unattended -nopause -nullrhi -nosound -nocrashreports `
      -stdout -fullstdoutlogoutput `
      -log="$logFile" `
      -ExecCmds="Automation RunTests UEJackAudioLink; quit" `
      -ReportOutputPath="$env:PACKAGE_DIR"

$automationExitCode = $LASTEXITCODE

# Print the log for debugging
Write-Host "--- Unreal Editor log output ---"
if (Test-Path $logFile) {
    Write-Host "Log file found, contents:"
    Get-Content $logFile | ForEach-Object { Write-Host "  $_" }
} else {
    Write-Host "Log file was not created at: $logFile"
}

# Check for automation report files
$reportFiles = Get-ChildItem -Path $env:PACKAGE_DIR -Filter "*.json" -ErrorAction SilentlyContinue
if ($reportFiles) {
    Write-Host "--- Automation Report Files ---"
    foreach ($file in $reportFiles) {
        Write-Host "Report file: $($file.Name)"
        if ($file.Name -like "*Automation*") {
            Write-Host "Contents:"
            Get-Content $file.FullName | ForEach-Object { Write-Host "  $_" }
        }
    }
}

if ($automationExitCode -ne 0) {
    throw "AutomationTests failed with code $automationExitCode. Check the log output above for details."
}
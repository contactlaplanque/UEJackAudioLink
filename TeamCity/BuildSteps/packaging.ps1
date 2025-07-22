& "$env:UE_PATH\Engine\Build\BatchFiles\RunUAT.bat" `
      BuildPlugin `
      -Plugin="$env:PLUGIN_FILE" `
      -Package="$env:PACKAGE_DIR" `
      -TargetPlatforms=Win64 `
      -Configuration=Development `
      -VS2022 `
      -NoCleanStage

if ($LASTEXITCODE -ne 0) { throw "BuildPlugin failed with code $LASTEXITCODE" }

# --- Diagnostics: List all created files ---
Write-Host "--- Listing contents of package directory: $env:PACKAGE_DIR ---"
if (Test-Path $env:PACKAGE_DIR) {
    Get-ChildItem -Path $env:PACKAGE_DIR -Recurse | ForEach-Object { Write-Host $_.FullName }
} else {
    Write-Host "ERROR: Package directory was not created at all."
}
# --- End Diagnostics ---
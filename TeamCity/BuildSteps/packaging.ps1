& "%env.UE_PATH%\Engine\Build\BatchFiles\RunUAT.bat" `
      BuildPlugin `
      -Plugin="%env.PLUGIN_FILE%" `
      -Package="%env.PACKAGE_DIR%" `
      -TargetPlatforms=Win64 `
      -Configuration=Development `
      -VS2022 `
      -Rocket                              # strip editor-only symbols

if ($LASTEXITCODE -ne 0) { throw "BuildPlugin failed with code $LASTEXITCODE" }
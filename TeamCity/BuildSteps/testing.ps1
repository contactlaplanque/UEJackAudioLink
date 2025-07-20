$uproject = Join-Path "%env.PACKAGE_DIR%" "HostProject\HostProject.uproject"

& "%env.UE_PATH%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
      $uproject `
      -unattended -nopause -nullrhi -nosound -log `
      -ExecCmds="Automation RunTests UEJackAudioLink.*; Quit"

if ($LASTEXITCODE -ne 0) { throw "AutomationTests failed with code $LASTEXITCODE" }
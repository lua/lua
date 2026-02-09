@echo off
set EXT_DIR=%USERPROFILE%\.vscode\extensions\al-nashar-studio.yaqout-1.0.0
echo Installing Yaqout VS Code extension...
if not exist "%EXT_DIR%" mkdir "%EXT_DIR%"
xcopy /E /Y "vscode-yaqout\*" "%EXT_DIR%\"
echo Done! Please restart VS Code to see the changes.
pause

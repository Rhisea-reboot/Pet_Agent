set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
cd /d "%SCRIPT_DIR%"
set "PATH=%SCRIPT_DIR%\runtime;%SCRIPT_DIR%\runtime\Scripts;%PATH%"
set "PYTHON=runtime\python.exe"
if not exist "%PYTHON%" set "PYTHON=runtime\Scripts\python.exe"
if not exist "%PYTHON%" (
    echo Python runtime not found.
    exit /b 1
)
"%PYTHON%" -I webui.py zh_CN
pause

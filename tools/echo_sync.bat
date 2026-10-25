@echo off
setlocal
where py >nul 2>nul && set "PYTHON=py -3"
if not defined PYTHON where python >nul 2>nul && set "PYTHON=python"
if not defined PYTHON (
    echo EchoPod sync needs Python 3: https://www.python.org/downloads/
    exit /b 2
)
if "%~1"=="" goto ask
%PYTHON% "%~dp0echo_sync.py" %*
exit /b %errorlevel%
:ask
set /p "CARD=Card drive letter, for example E: "
if "%CARD:~-1%"=="\" set "CARD=%CARD:~0,-1%"
%PYTHON% "%~dp0echo_sync.py" "%CARD%"
pause

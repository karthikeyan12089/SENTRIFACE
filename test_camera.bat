@echo off
REM Tests the webcam + face detection only. No ESP32 needed.
cd /d "%~dp0"

set PY=
where py >nul 2>nul
if %errorlevel%==0 set PY=py -3
if "%PY%"=="" (
  where python >nul 2>nul
  if %errorlevel%==0 set PY=python
)
if "%PY%"=="" goto nopython

if exist ".venv\Scripts\python.exe" goto haveenv
echo Creating Python environment (first run only)...
%PY% -m venv .venv
if not exist ".venv\Scripts\python.exe" goto fail

:haveenv
".venv\Scripts\python.exe" -m pip install --disable-pip-version-check -q -r requirements.txt
if errorlevel 1 goto fail
".venv\Scripts\python.exe" python\camera_test.py
goto end

:nopython
echo Python was not found. Install it from https://www.python.org/downloads/
echo and tick "Add python.exe to PATH" in the installer.
goto end

:fail
echo Setup failed. See README.md, Troubleshooting.

:end
echo.
pause

@echo off
REM SENTRIFACE launcher: sets up Python on first run, then starts the camera script.
REM Double-click this file. Close the Arduino Serial Monitor first.
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
echo [1/3] Creating Python environment (first run only)...
%PY% -m venv .venv
if not exist ".venv\Scripts\python.exe" goto venvfail

:haveenv
echo [2/3] Checking packages (first run downloads ~60 MB)...
".venv\Scripts\python.exe" -m pip install --disable-pip-version-check -q -r requirements.txt
if errorlevel 1 goto pipfail

echo [3/3] Starting SENTRIFACE. Press q in the camera window to quit.
".venv\Scripts\python.exe" python\sentriface_camera.py
goto end

:nopython
echo Python was not found. Install it from https://www.python.org/downloads/
echo and tick "Add python.exe to PATH" in the installer, then run this again.
goto end

:venvfail
echo Could not create the Python environment. See README.md, Troubleshooting.
goto end

:pipfail
echo Package install failed. Check the internet connection, then run again.
echo If it says blocked by policy, see README.md, Troubleshooting.

:end
echo.
pause

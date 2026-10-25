@echo off
cd /d "%~dp0"
set /p ADDR=Host address (e.g. 192.168.1.20): 
start "" Atlas.exe --join %ADDR%

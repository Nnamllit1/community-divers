@echo off
start "" /wait "%~dp0CommunityDivers.exe" --update
exit /b %errorlevel%

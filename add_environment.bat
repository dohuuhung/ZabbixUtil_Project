@echo off
echo Adding the current folder and sub folder "dlls" to environment variable PATH...

REM Save absoluted path of the current folder to variable CURRENT
set "CURRENT=%CD%"

REM Write to variable PATH of user
setx PATH "%PATH%;%CURRENT%;%CURRENT%\dlls"

echo Finish adding. Open new CMD to apply new changes.
pause
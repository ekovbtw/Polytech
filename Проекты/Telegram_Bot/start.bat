@echo off
cd /d "%~dp0"
title FITS Bot
echo Starting FITS Bot...
python bot.py
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Bot stopped with error code %ERRORLEVEL%
    pause
)

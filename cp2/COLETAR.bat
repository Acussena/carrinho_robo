@echo off
chcp 65001 >nul
cd /d "%~dp0"
".venv\Scripts\python.exe" scripts\coleta_guiada.py
echo.
echo (janela parada para leitura - pode fechar)
pause >nul

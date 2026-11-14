@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo ============================================
echo   Fiscal de Calcadas - painel com IA (GS)
echo ============================================
echo  1) Robo pelo Wi-Fi (IP da ESP32)
echo  2) Robo pela USB (porta COM da ESP32)
echo  3) Simulacao (sem o robo)
echo.
set /p OP=Escolha 1, 2 ou 3: 
if "%OP%"=="1" goto wifi
if "%OP%"=="2" goto usb
goto simular

:wifi
set IP=fiscal.local
set /p IP=IP da ESP32 (Enter = fiscal.local): 
start "" http://localhost:8000
py -u painel.py --ip %IP%
goto fim

:usb
set /p PORTA=Porta COM da ESP32 (ex.: COM5): 
py -m pip install -q -r requirements.txt
start "" http://localhost:8000
py -u painel.py --porta %PORTA%
goto fim

:simular
start "" http://localhost:8000
py -u painel.py --simular

:fim
pause

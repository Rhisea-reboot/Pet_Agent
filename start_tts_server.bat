@echo off
REM ============================================
REM GPT-SoVITS TTS Server Launcher
REM 在启动桌宠前运行此脚本以启动 TTS 服务
REM ============================================

cd /d "%~dp0GPT-SoVITS"

echo Starting GPT-SoVITS API server...
echo Working directory: %cd%
echo Server will listen on http://127.0.0.1:9880
echo.

runtime\python.exe api_v2.py -a 127.0.0.1 -p 9880 -c GPT_SoVITS\configs\tts_infer.yaml

echo.
echo Server stopped.
pause

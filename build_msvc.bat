@echo off
REM "x86 Native Tools Command Prompt for VS" icinde calistir (32-bit olmali).
cl /nologo /LD /O2 /EHsc /MT nfsu2_gfx.cpp /Fe:NFSU2_GraphicsBoost.asi user32.lib
if errorlevel 1 (echo DERLEME HATASI & pause & exit /b 1)
echo Tamam: NFSU2_GraphicsBoost.asi olustu
pause

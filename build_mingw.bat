@echo off
REM 32-bit (i686) MinGW-w64 gerekir (ornek: winlibs.com, "i686" surumu).
REM g++ komutu 32-bit olmali.
g++ -shared -O2 -m32 -static -static-libgcc -static-libstdc++ -o NFSU2_GraphicsBoost.asi nfsu2_gfx.cpp
if errorlevel 1 (echo DERLEME HATASI & pause & exit /b 1)
echo Tamam: NFSU2_GraphicsBoost.asi olustu
pause

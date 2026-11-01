# NFSU2 Graphics Boost

## 1) Derleme (3 yoldan biri)
A) GitHub (kurulum yok): bu klasoru yeni bir repoya yukle -> Actions sekmesi -> "Build ASI" -> Artifacts'tan indir.
B) Visual Studio: "x86 Native Tools Command Prompt" ac, klasore gir, `build_msvc.bat` calistir.
C) MinGW-w64 (32-bit/i686): `build_mingw.bat`.
Cikti: NFSU2_GraphicsBoost.asi (MUTLAKA 32-bit olmali)

## 2) Kurulum
1. Ultimate ASI Loader (dinput8.dll olarak adlandirilmis surum) oyun klasorune (SPEED2.EXE yani).
2. Oyun klasorunde `scripts` klasoru olustur.
3. `NFSU2_GraphicsBoost.asi` ve `NFSU2_GraphicsBoost.ini` dosyalarini `scripts` icine koy.
4. Oyunu ac. Calistiysa `scripts\NFSU2_GraphicsBoost.log` olusur.

## 3) Sorun giderme
- Log olusmuyorsa: ASI Loader calismiyor ya da .asi 64-bit derlenmis.
- Siyah ekran / bozuk gorunum: ini'de MSAA=0 yap.
- Gorsel fark yoksa: ekran karti surucusu AF'yi eziyor olabilir ya da oyun sampler'lari farkli kuruyor; log dosyasini bana at.

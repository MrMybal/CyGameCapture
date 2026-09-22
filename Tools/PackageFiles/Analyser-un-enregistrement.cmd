@echo off
rem ---------------------------------------------------------------------------
rem  Analyser-un-enregistrement.cmd  <fichier.mkv>
rem
rem  Dit ce qui a reellement survecu dans un fichier : format de pixel, nombre
rem  de bits, niveaux distincts par composante, presence d'un canal alpha.
rem
rem  On peut aussi glisser-deposer un fichier sur ce script.
rem ---------------------------------------------------------------------------
if "%~1"=="" (
    echo.
    echo   Usage : glissez un fichier .mkv sur ce script,
    echo           ou    Analyser-un-enregistrement.cmd "chemin\fichier.mkv"
    echo.
    echo   Exemple de lecture :
    echo     - un depth qui etait en 16 bits et revient avec 214 niveaux
    echo       distincts a ete ecrase en 8 bits quelque part
    echo     - "NO ALPHA CHANNEL" sur un ATH isole veut dire que le masque
    echo       a ete perdu : il faut le mode "Sans perte RGB 8 bits"
    echo.
    pause
    exit /b 1
)
echo.
"%~dp0Outils\CyGameCaptureVideoProbe.exe" %1 3
echo.
pause

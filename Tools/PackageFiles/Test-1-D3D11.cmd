@echo off
rem ---------------------------------------------------------------------------
rem  Lance l'application de test Direct3D 11.
rem  Elle publie trois flux Spout : la scene propre, le depth linearise et
rem  l'ATH isole. Echap ou la croix pour fermer.
rem ---------------------------------------------------------------------------
echo.
echo   Application de test Direct3D 11
echo.
echo   Trois flux Spout vont apparaitre :
echo     CyGameCaptureRS::CyGameCaptureTestApp        la scene sans ATH
echo     CyGameCaptureRS::CyGameCaptureTestApp::2     le depth (16 bits)
echo     CyGameCaptureRS::CyGameCaptureTestApp::3     l'ATH seul (masque en alpha)
echo.
echo   Touche Origine (Home) pour l'overlay ReShade et le Buffer Inspector.
echo   Echap pour quitter.
echo.
cd /d "%~dp0Applications-de-test\D3D11"
start "" "CyGameCaptureTestApp.exe"

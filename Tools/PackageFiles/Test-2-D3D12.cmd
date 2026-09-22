@echo off
rem ---------------------------------------------------------------------------
rem  Lance l'application de test Direct3D 12.
rem  Meme chose qu'en D3D11, mais la sortie passe par le pont D3D11On12 : en
rem  D3D12 un handle partage classique ne peut pas etre produit autrement.
rem ---------------------------------------------------------------------------
echo.
echo   Application de test Direct3D 12
echo.
echo   Trois flux Spout vont apparaitre :
echo     CyGameCaptureRS::CyGameCaptureTestAppD3D12       la scene sans ATH
echo     CyGameCaptureRS::CyGameCaptureTestAppD3D12::2    le depth (16 bits)
echo     CyGameCaptureRS::CyGameCaptureTestAppD3D12::3    l'ATH seul (masque en alpha)
echo.
echo   Touche Origine (Home) pour l'overlay ReShade et le Buffer Inspector.
echo   Echap pour quitter.
echo.
cd /d "%~dp0Applications-de-test\D3D12"
start "" "CyGameCaptureTestAppD3D12.exe"

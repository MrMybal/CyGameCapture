@echo off
rem ---------------------------------------------------------------------------
rem  Affiche les senders Spout presents sur la machine.
rem
rem  Pour chaque flux CyGameCapture, l'index de frame publie est affiche entre
rem  crochets : les flux d'une meme frame portent le meme index. C'est ce qui
rem  permet a un enregistrement multi-tampons de rester aligne.
rem ---------------------------------------------------------------------------
echo.
"%~dp0Outils\CyGameCaptureSpoutReceiverTest.exe" --list
echo.
pause

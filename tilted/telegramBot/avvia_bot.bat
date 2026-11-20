@echo off
chcp 65001 >nul
cd /d "%~dp0"

:: Seleziona automaticamente un JDK 17+ installato sul PC (necessario per java.net.http del PDF)
set "JDK_DIR=C:\Users\Mattia\.jdks\ms-17.0.18"
if not exist "%JDK_DIR%\bin\javac.exe" (
    set "JDK_DIR=C:\Users\Mattia\.jdks\openjdk-25.0.2"
)

if exist "%JDK_DIR%\bin\javac.exe" (
    set "JAVAC=%JDK_DIR%\bin\javac.exe"
    set "JAVA=%JDK_DIR%\bin\java.exe"
) else (
    set "JAVAC=javac"
    set "JAVA=java"
)

if not exist "out" mkdir out

echo [1/2] Compilazione del Bot Java con org.json...
"%JAVAC%" -encoding UTF-8 -cp "lib\json-20240303.jar" -d out src\main\java\pokedexbot\*.java
if errorlevel 1 (
    echo [ERRORE] Compilazione fallita!
    pause
    exit /b 1
)

echo [2/2] Avvio PokedexBot...
"%JAVA%" -Dfile.encoding=UTF-8 -cp "out;lib\json-20240303.jar" pokedexbot.PokedexBot
pause

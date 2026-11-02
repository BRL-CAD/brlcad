@echo off
setlocal DisableDelayedExpansion

for %%I in ("%~dp0.") do set "BUNDLE_DIR=%%~fI"
set "BUNDLE_DIR_FWD=%BUNDLE_DIR:\=/%"
set "DAT_FILE=%BUNDLE_DIR%\creo-brl.dat"
set "DEBUG_DAT_FILE=%BUNDLE_DIR%\creo-brl-debug.dat"

(
  echo name creo-brl
  echo startup dll
  echo allow_stop TRUE
  echo exec_file %BUNDLE_DIR_FWD%/bin/creo-brl.dll
  echo text_dir %BUNDLE_DIR_FWD%/text
  echo description CREO to BRL-CAD
  echo end
) > "%DAT_FILE%" || (
  echo Failed to update %DAT_FILE% 1>&2
  exit /b 1
)

if exist "%DEBUG_DAT_FILE%" (
  (
    echo name creo-brl
    echo startup spawn
    echo allow_stop TRUE
    echo exec_file %BUNDLE_DIR_FWD%/bin/creo-brl-exe.exe
    echo text_dir %BUNDLE_DIR_FWD%/text
    echo description CREO to BRL-CAD
    echo end
  ) > "%DEBUG_DAT_FILE%" || (
    echo Failed to update %DEBUG_DAT_FILE% 1>&2
    exit /b 1
  )
)

set "CONFIG_PRO="
if not "%~1"=="" (
  if exist "%~f1\*" (
    set "CONFIG_PRO=%~f1\config.pro"
  ) else (
    set "CONFIG_PRO=%~f1"
  )
)

if not defined CONFIG_PRO (
  for %%P in ("%USERPROFILE%\config.pro" "%USERPROFILE%\Documents\config.pro") do (
    if not defined CONFIG_PRO if exist "%%~fP" (
      findstr /i /c:"creo-brl" "%%~fP" >nul 2>&1 && set "CONFIG_PRO=%%~fP"
    )
  )
)

if not defined CONFIG_PRO (
  for /d %%D in ("C:\Program Files\PTC\Creo *") do (
    if not defined CONFIG_PRO if exist "%%~fD\Common Files\text\config.pro" (
      findstr /i /c:"creo-brl" "%%~fD\Common Files\text\config.pro" >nul 2>&1 && set "CONFIG_PRO=%%~fD\Common Files\text\config.pro"
    )
  )
)

if not defined CONFIG_PRO (
  if exist "%USERPROFILE%\config.pro" (
    set "CONFIG_PRO=%USERPROFILE%\config.pro"
  ) else if exist "%USERPROFILE%\Documents\config.pro" (
    set "CONFIG_PRO=%USERPROFILE%\Documents\config.pro"
  ) else (
    set "CONFIG_PRO=%USERPROFILE%\config.pro"
  )
)

for %%I in ("%CONFIG_PRO%") do set "CONFIG_PRO=%%~fI"

set "CONFIRM="
set /p "CONFIRM=Update %CONFIG_PRO% to %DAT_FILE% [Y/n] "
if defined CONFIRM set "CONFIRM=%CONFIRM: =%"
if not defined CONFIRM goto :update_config
if /i "%CONFIRM%"=="y" goto :update_config
if /i "%CONFIRM%"=="yes" goto :update_config
exit /b 0

:update_config
set "CONFIG_KEY=protkdat"
if exist "%CONFIG_PRO%" (
  for /f "usebackq tokens=1" %%K in (`findstr /i /c:"creo-brl.dat" /c:"creo-brl-debug.dat" "%CONFIG_PRO%"`) do (
    if /i "%%K"=="protkdat" set "CONFIG_KEY=protkdat"
    if /i "%%K"=="creotkdat" set "CONFIG_KEY=creotkdat"
    if /i "%%K"=="toolkit_registry_file" set "CONFIG_KEY=toolkit_registry_file"
  )
)

for %%I in ("%CONFIG_PRO%") do set "CONFIG_DIR=%%~dpI"
if not exist "%CONFIG_DIR%" mkdir "%CONFIG_DIR%"

set "TEMP_FILE=%CONFIG_PRO%.tmp"
(
  if exist "%CONFIG_PRO%" (
    for /f "usebackq eol= delims=" %%L in (`findstr /v /i /c:"creo-brl.dat" /c:"creo-brl-debug.dat" "%CONFIG_PRO%"`) do echo(%%L
  )
  echo %CONFIG_KEY% %DAT_FILE%
) > "%TEMP_FILE%" || (
  del /q "%TEMP_FILE%" >nul 2>&1
  echo Failed to update %CONFIG_PRO% 1>&2
  exit /b 1
)

move /y "%TEMP_FILE%" "%CONFIG_PRO%" >nul || (
  del /q "%TEMP_FILE%" >nul 2>&1
  echo Failed to update %CONFIG_PRO% 1>&2
  exit /b 1
)
exit /b 0

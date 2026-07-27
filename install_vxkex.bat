@echo off
setlocal

:: VxKex インストールバッチスクリプト
:: このスクリプトは、VxKex が配置されたディレクトリ (例: C:\Program Files\VxKex)
:: から「管理者として実行」してください。

echo VxKex のインストールとシェル拡張の登録を行っています...

:: カレントディレクトリ (このバッチファイルの場所) を取得し、末尾の \ を削除
set "KEXDIR=%~dp0"
if "%KEXDIR:~-1%"=="\" set "KEXDIR=%KEXDIR:~0,-1%"

:: 1. KexDir をレジストリに登録 (VxKex のインストールパス)
reg add "HKLM\SOFTWARE\VXsoft\VxKex" /v KexDir /t REG_SZ /d "%KEXDIR%" /f

:: 2. KexShlEx.dll の CLSID 登録
reg add "HKCR\CLSID\{9AACA888-A5F5-4C01-852E-8A2005C1D45F}" /ve /t REG_SZ /d "VxKex Property Sheet Handler" /f
reg add "HKCR\CLSID\{9AACA888-A5F5-4C01-852E-8A2005C1D45F}\InProcServer32" /ve /t REG_SZ /d "%KEXDIR%\KexShlEx.dll" /f
reg add "HKCR\CLSID\{9AACA888-A5F5-4C01-852E-8A2005C1D45F}\InProcServer32" /v ThreadingModel /t REG_SZ /d "Apartment" /f

:: 3. シェル拡張の「承認済み (Approved)」リストへの追加 (Vista では必須)
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" /v "{9AACA888-A5F5-4C01-852E-8A2005C1D45F}" /t REG_SZ /d "VxKex Property Sheet Handler" /f

:: 4. .exe と .lnk ファイルのプロパティシートに VxKex を追加
reg add "HKCR\exefile\shellex\PropertySheetHandlers\VxKex" /ve /t REG_SZ /d "{9AACA888-A5F5-4C01-852E-8A2005C1D45F}" /f
reg add "HKCR\lnkfile\shellex\PropertySheetHandlers\VxKex" /ve /t REG_SZ /d "{9AACA888-A5F5-4C01-852E-8A2005C1D45F}" /f

echo.
echo 設定を反映させるために explorer.exe を再起動します...
taskkill /f /im explorer.exe
timeout /t 2 /nobreak >nul
start explorer.exe

echo.
echo VxKex のインストールが完了しました！
pause

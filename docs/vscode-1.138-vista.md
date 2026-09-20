# VSCode 1.138.0 / Server 2008 起動検証

## 結果

2026-09-17、Windows Server 2008 SP2 x64 (6.0.6003) VM で、指定された次の本体を起動し、デバッガーなしのエディター表示・テキストファイル読込みを確認した。

```text
C:\Users\Administrator\Downloads\VSCode-win32-x64-1.138.0\Code.exe
```

Code.exe のファイルバージョンと `7debcd0e2a\resources\app\package.json` の両方で 1.138.0 を確認。Code.exe 自体は変更していない。

## 起動方法

検証 VM のデスクトップにある **VSCode-1.138-Vista.cmd** を実行する。

他の環境では更新した Installer を導入し、`configure-vscode-vista.cmd` を管理者として実行してから `launch-vscode-1.138-vista.cmd` を使用する。第 1 引数で Code.exe の絶対パスを指定できる。設定データは `%LOCALAPPDATA%\VxKex\VSCode-1.138` に分離する。

初回の Welcome 案内は「Continue without Signing In」で閉じられる。アカウントへのサインインは起動に必要ない。

## 必須条件と制限

- `--no-sandbox` が必要。通常の対応 OS で提供される Electron の sandbox 隔離は利用できない。
- `--disable-gpu --disable-gpu-sandbox --disable-software-rasterizer --use-gl=disabled` を併用。GPU 描画と WebGL は無効。
- ダブルクリックによる Code.exe の直接起動では、Vista のプロセス作成がエラー 193 になる。必ず VistaRun.exe を使用する。
- Copilot の native module 読込みで未解決 export の例外を診断ログに記録した。Copilot、拡張機能全般、統合ターミナル、デバッグ実行、長時間安定性は今回の確認範囲外。
- タイトルバーの配置など、Vista 上で表示が不自然になる箇所が残る。起動成功は全機能対応を意味しない。

## 今回の変更

1. **KxBase/forwards.c**: K32GetModuleBaseNameA/W の転送先を Vista にない kernel32 export から psapi.GetModuleBaseNameA/W に修正。
2. **tools/VistaRun**: 既存の KexPatchCpiwSubsystemVersionCheck を呼ぶ小型ランチャーを追加。自分のプロセス内で subsystem version 検査を回避して CreateProcessW を呼ぶ。対象 EXE のヘッダー変更やデバッガー接続は行わない。
3. **Installer**: 更新した KxBase.dll、VistaRun.exe、1.138 用起動スクリプトを同梱。
4. **tests/vista_compat_probe.c**: K32GetModuleBaseNameA/W の実際の返却名を検証する 2 項目を追加。

初期診断ではアプリ直下に置いた KxBase.dll 自身の import が書き換えられ、LoadLibraryExW が再帰することも確認した。この診断用コピーは除去し、System32 の正規配置で検証している。互換 DLL を Code.exe の隣へコピーしないこと。

## 検証記録

- VS2010 + SDK 7.1 で KxBase、VistaRun、専用試験 EXE をビルド。
- 専用試験 **23 項目 PASS**（PSAPI 転送、SRW lock 競合、section DACL、既存 API 等）。
- VerCheck **4 項目 PASS**（VerifyVersionInfo / GetVersionEx / RtlGetVersion / Registry）。
- `audit/vscode138/file_verified2.png`: 通常起動のファイル表示。表示したテキストは以前の 1.70.2 試験用ファイルを再利用したもの。
- `audit/vscode138/debug5.txt`: sandbox 無効時の診断ログ。通常起動用 VistaRun は DEBUG_PROCESS を使用しない。
- `audit/vscode138/compat_results.txt`, `vercheck_regression.txt`: 回帰結果。
- 更新前の VM の System32 DLL は `KxBase.pre138.dll` として保持。

ランチャーのビルド:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\VistaRun\build.ps1
```

PSAPI の旧 OS 向け DLL の違いは [Microsoft GetModuleBaseName 文書](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getmodulebasenamea) に準拠。PE の OS/subsystem version フィールドは [Microsoft PE Format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format) を参照。

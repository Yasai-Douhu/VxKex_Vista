###############################################################################
#
# VxKex-Vista Deployment Package
# Target: Windows Vista SP2 x64
#
###############################################################################

## 概要

VxKex-Vistaは、Windows Vista SP2 x64環境でWindows 7以降のAPIを利用可能にする
ためのAPI拡張ライブラリです。

## ファイル構成

- KexDll.dll         - メインDLL（API拡張・IFEO処理）
- KexShlEx.dll       - シェル拡張（プロパティシート統合）
- KxNt.dll           - ntdll.dll APIフォワーディング
- KxBase.dll         - kernel32/kernelbase APIフォワーディング
- deploy_to_vista.bat - デプロイスクリプト

## インストール方法

1. 本ディレクトリの内容をVistaマシン上の適当な場所（例: C:\VxKex）に展開します。

2. 管理者権限でコマンドプロンプトを開き、以下を実行します:

   C:\VxKex> deploy_to_vista.bat install-all "C:\VxKex"

   これにより、notepad.exe、calc.exe、explorer.exe に VxKex が適用されます。

3. 特定のアプリにのみ適用する場合は:

   C:\VxKex> deploy_to_vista.bat install "C:\VxKex" notepad.exe

## アンインストール方法

C:\VxKex> deploy_to_vista.bat uninstall "C:\VxKex" notepad.exe

## 動作確認

展開したDLLが正しく読み込まれているかは、以下の方法で確認できます:

1. ターゲットアプリ（例: メモ帳）を起動
2. タスクマネージャーでプロセスを確認
3. DLLの読み込みをデバッガ等で確認

## 技術的な詳細

### IFEO（Image File Execution Options）

VxKexはWindowsのApplication Verifier機能を利用し、IFEOレジストリキーを通じて
DLLを自動ロードします。

設定されるレジストリキー:
  HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\<exe_name>
    ├─ VerifierDlls (REG_MULTI_SZ): "C:\VxKex\KexDll.dll"
    ├─ GlobalFlag (REG_DWORD): 0x80000000 (FLG_APPLICATION_VERIFIER)
    └─ VerifierFlags (REG_DWORD): 0x80000000

### シェル拡張

KexShlEx.dllはWindows ExplorerのプロパティシートにVxKex設定ダイアログを
追加します。CLSID: {9AACA888-A5F5-4C01-852E-8A2005C1D45F}

## 免責事項

本ソフトウェアは検証目的で使用されます。Vista環境での動作保証はしません。
実行前のバックアップを推奨します。

## リビジョン

- 2026-07-24: Initial deployment package creation

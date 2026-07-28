# VxKex_Vista - Windows Vista (NT 6.0) x64 実装

VxKex_Vista は Windows Vista (NT 6.0) に特化した VxKex の実装であり、Windows Vista SP2 および Windows Server 2008 SP2 向けに設計されています。
Windows 7 以降を対象としたオリジナルの VxKex を Vista 環境でも動作するように移植・修正したプロジェクトです。

## 主な特徴

- **対象 OS**: Windows Vista SP2 / Windows Server 2008 SP2 (NT 6.0) x64
- **ビルドツールチェーン**: Visual Studio 2010 + Windows SDK 7.1A
- **アーキテクチャ**: x64 専用

## プロジェクト構成

```
VxKex_Vista/
├── 00-Common-Headers/    # 共通ヘッダーファイル
├── 00-Import-Libraries/  # リンク用インポートライブラリ
├── KxBase/               # KxBase.dll ソース (kernel32/kernelbase のリダイレクト)
├── KxNt/                 # KxNt.dll ソース (NT API のリダイレクト)
├── x64/Release/          # ビルド出力先
├── build_all.ps1         # 統合ビルドスクリプト
├── build_kxnt.ps1        # KxNt.dll 用ビルドスクリプト
├── build_kxbase.ps1      # KxBase.dll 用ビルドスクリプト
├── Installer/            # インストーラ構築用ファイル
└── README.md
```

## ビルド要件

- Visual Studio 2010 (C++ 開発ツール含む)
- Windows SDK 7.1A
- PowerShell

## ビルド方法

### すべての DLL をビルドする

```powershell
powershell -ExecutionPolicy Bypass -File build_all.ps1
```

このスクリプトは以下の処理を行います：
1. 必要なファイルをコンパイル・ビルド
2. KxNt.dll および KxBase.dll などの全コンポーネントのビルド
3. 最終的なインストーラパッケージ (ZIP) の生成

## Vista 互換性に関する技術詳細

### kernelbase.dll の扱い

Windows Vista (NT 6.0) には `kernelbase.dll` が存在しません (Windows 7 で導入)。
VxKex_Vista では以下の対応を行っています：
- `kernelbase.dll` に転送されるはずのエクスポートを `kernel32.dll` や `advapi32.dll` に転送するよう修正
- `forwards.c` にて Vista 互換のエクスポート転送定義を記述

### dllmain.c の初期化タイミング問題

Windows Vista の 64ビット環境において、`sizeof(PVOID) == 8` による単純なアーキテクチャ判定では、WOW64プロセス初期化時の `LdrLoadDll` フック登録時に問題が発生し、クラッシュするバグがありました。本プロジェクトでは、より厳密な判定 (`KexRtlOperatingSystemBitness() == KexRtlCurrentProcessBitness()`) を用いるよう修正されています。

### 子プロセスへの伝播 (Propagation)

VxKex は子プロセスでも有効化されるよう、`NtCreateUserProcess` をフックして `NtOpenKey` 呼び出しをインターセプトします。
Windows Vista においては、ローダーのパス解決の仕様の違いからネイティブの `LdrAddDllDirectory` が利用できないため、拡張 DLL (`Kx*.dll`) を `System32` フォルダに直接コピーするインストーラを採用しています。また、子プロセス伝播用のダミーレジストリキー (`{VxKexPropagationVirtualKey}`) をレジストリに作成することで、確実なフックを実現しています。

## インストール方法

`VxKex_Vista_x64_Installer.zip` を展開し、中にある `install.bat` を管理者権限で実行してください。
UAC の昇格、各 DLL の `C:\VxKex` および `System32` へのコピー、レジストリの登録などをすべて自動で行います。

## 既知の問題

1. Windows 7 以降でのみサポートされている一部の API (例: `IsWow64Process2`, `WaitOnAddress`, `InitializeCriticalSectionEx`) は Vista では完全には動作しない場合があります。
2. SRWLock 系の関数は Vista SP1 以降が必須です。

## 関連プロジェクト

- [VxKex-NEXT](https://github.com/vxiiduu/VxKex) - メインの VxKex プロジェクト (Windows 7+)

## ライセンス

ライセンス情報はメインの VxKex プロジェクトに準拠します。

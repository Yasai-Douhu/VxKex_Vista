# VxKex-Vista 引継ぎドキュメント

## 目次

1. [プロジェクト概要](#1-プロジェクト概要)
2. [Vista環境での動作確認方法](#2-vista環境での動作確認方法)
3. [VxKex-NEXT/ と VxKex_Vista/ の実装比較](#3-vxkex-next-と-vxkex_vista/-の実装比較)
4. [プロパティタブ実装の詳細](#4-プロパティタブ実装の詳細)
5. [IFEOレジストリ適用の問題と解決策](#5-ifeoレジストリ適用の問題と解決策)
6. [今後の課題とタスクリスト](#6-今後の課題とタスクリスト)

---

## 1. プロジェクト概要

### 1.1 目的

VxKex-Vistaプロジェクトは、Windows Vista SP2 (NT 6.0) x64 専用VxKex実装です。VxKex-NEXTのコードベースをVS2010 + Windows SDK 7.1Aでビルド可能にし、VistaのAPI制約に対応させることを目的としています。

### 1.2 完了フェーズ

| フェーズ | 内容 | ステータス |
|---------|------|-----------|
| Phase 1 | 共通基盤の整備（ディレクトリ構造作成、ソースコピー） | 完了 |
| Phase 2 | ソースコードのVista対応確認と修正 | 完了 |

### 1.3 ビルド環境

- **コンパイラ**: Visual Studio 2010 (MSVC 16.0)
- **SDK**: Windows SDK 7.1A
- **アーキテクチャ**: x64
- **ターゲット**: Windows Vista SP2 x64 (NT 6.0)

### 1.4 Vista互換設計

各`buildcfg.h`で以下のプリプロセッサが定義されています:

```c
// Vista向けビルド設定
#define _WIN32_WINNT 0x0600
#define WINVER 0x0600
#define PSAPI_VERSION 1
```

---

## 2. Vista環境での動作確認方法

ユーザーはnotepad.exeにVxKexを適用しましたが、IFEOレジストリが適用されておらず、ログファイルも存在しない状態です。以下に正常動作を確認する方法を詳述します。

### 2.1 IFEOレジストリの確認

コマンドプロンプト（管理者権限）で以下のコマンドを実行し、IFEOキーが正しく設定されているか確認します:

```cmd
rem IFEOキーの存在確認
reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe"

rem VerifierDllsの確認（REG_MULTI_SZ形式）
reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe" /v VerifierDlls

rem GlobalFlagの確認（REG_DWORD形式、16進数表示推奨）
reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe" /v GlobalFlag /t REG_DWORD

rem VerifierFlagsの確認
reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe" /v VerifierFlags
```

**期待される値**:
- `VerifierDlls`: `C:\VxKex\KexDll.dll`（DLLへの絶対パス、REG_MULTI_SZ形式）
- `GlobalFlag`: `0x80000000` (2147483648 10進数)
- `VerifierFlags`: `0x80000000` (2147483648 10進数)

### 2.2 ログファイルの確認

VxKexは実行時のログを`.vxl`形式のファイルに出力します。ログファイルの場所は以下の通りです:

```
C:\VxKex\Logs\notepad.exe.vxl
```

または、アプリケーション依存のパス:

```
C:\VxKex\<ExecutableName>.vxl
```

**注意**: ログファイルが存在しない場合、VxKexがロードされていない可能性があります。

### 2.3 Process ExplorerによるDLL確認

1. [Process Explorer](https://learn.microsoft.com/ja-jp/sysinternals/downloads/process-explorer)を起動（管理者権限）
2. `notepad.exe`プロセスを検索
3. プロセスをダブルクリックしてプロパティを開く
4. 「Threads」タブまたは「DLLs」タブを確認
5. 以下のDLLがロードされていることを確認:
   - `KexDll.dll`
   - `KxNt.dll`（オプション）
   - `KxBase.dll`（オプション）

### 2.4 Debug ViewによるDbgPrint確認

[Debug View](https://learn.microsoft.com/ja-jp/sysinternals/downloads/debugview)を使用して、VxKexのデバッグ出力を確認します:

1. Debug Viewを起動（管理者権限）
2. `notepad.exe`を実行
3. Debug Viewで以下の形式のメッセージが表示されるか確認:

```
VXL (KexDll): Process created

The VxKex NEXT version is x.x.x (Release)
The Windows version is 6.0.6002
```

**重要**: Vistaにおいて、`DbgPrint`出力はDebug Viewのようなデバッガ経由でのみ確認可能です。VxlWriteLogEx関数内には以下のフォールバックがあります（[`vxlwrite.c:130-132`](VxKex_Vista/KexDll/vxlwrite.c:130)）:

```c
if (NtCurrentPeb()->BeingDebugged) {
    DbgPrint("VXL (%ws): %ws\r\n", SourceComponent, FileEntry->Text);
}
```

### 2.5 手動IFEO設定コマンド

デプロイスクリプトを手動で実行する場合:

```cmd
rem 管理者権限でコマンドプロンプトを実行

rem IFEOキーの作成
reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe" /v VerifierDlls /t REG_MULTI_SZ /d "C:\VxKex\KexDll.dll" /f

rem GlobalFlag設定（FLG_APPLICATION_VERIFIER = 0x80000000）
reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe" /v GlobalFlag /t REG_DWORD /d 2147483648 /f

rem VerifierFlags設定
reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe" /v VerifierFlags /t REG_DWORD /d 2147483648 /f
```

### 2.6 IFEOの動作原理

IFEO（Image File Execution Options）は、Windowsが実行ファイルを起動する際に以下のプロセスを踏みます:

1. Windowsが`notepad.exe`の起動を試みる
2. レジストリ`HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe`を確認
3. `VerifierDlls`値が存在する場合、指定されたDLLを`notepad.exe`のプロセス空間にロード
4. `GlobalFlag`が`FLG_APPLICATION_VERIFIER` (0x80000000) に設定されている場合、DLLは`DLL_PROCESS_ATTACH`でロードされる

**重要な注意点**:
- `VerifierDlls`の値はREG_MULTI_SZ形式（ワイド文字列、末尾にNULL、最終的に二重NULL）である必要があります
- パスは絶対パスであることが推奨されます
- Vistaでは、`Image File Execution Options`キーへのアクセスには管理者権限が必要です

---

## 3. VxKex-NEXT/ と VxKex_Vista/ の実装比較

### 3.1 ディレクトリ構造比較

#### VxKex-NEXT/（元のコードベース）

```
VxKex-NEXT/
├── KexBase/          # kernel32/kernelbase APIリダイレクト
├── KexGui/           # GUIヘルパーライブラリ
├── KexDll/           # コアVxKex DLL
├── KexShlEx/         # シェル拡張（プロパティシートタブ）
├── KexW32ML/         # Win32マルチリンガルヘルパー
├── KxCfgHlp/         # 設定ヘルパー（IFEO設定など）
├── KxNt/             # ntdll syscallフォワーディング
├── KxSetup/          # セットアップインストーラー
├── VxKexLdr/         # ローダーラッパー
├── VistaDLLs/        # ビルド済みDLL（Vista用）
└── ...
```

#### VxKex_Vista/（Vista専用実装）

```
VxKex_Vista/
├── KexBase/          # Vista対応修正済み
├── KexGui/           # Vista対応修正済み
├── KexDll/           # Vista対応修正済み
├── KexShlEx/         # Vista対応修正済み
├── KexW32ML/         # Vista対応修正済み
├── KxCfgHlp/         # Vista対応修正済み
├── KxNt/             # Vista対応修正済み
├── VxKexLdr/         # Vista対応修正済み
├── VistaDLLs/        # Vista用ビルド済みDLL
├── VistaDeploy/      # デプロイパッケージ用ディレクトリ
├── deploy_to_vista.bat  # デプロイスクリプト
├── VxKex_Vista_Deploy.zip  # ZIPパッケージ
└── ...
```

### 3.2 buildcfg.h の比較

#### [`VxKex-NEXT/KexDll/buildcfg.h`](VxKex-NEXT/KexDll/buildcfg.h)

```c
#define KEXAPI

//
// Set to TRUE to disable the PROTECTED_FUNCTION macro.
// Perhaps useful for debug builds, to catch exceptions under the 
// debugger more quickly - however this will prevent exceptions from
// being logged through VXL.
//
#define DISABLE_PROTECTED_FUNCTION FALSE

#define KEX_COMPONENT L"KexDll"
#define KEX_ENV_NATIVE
#define KEX_TARGET_TYPE_DLL
```

#### [`VxKex_Vista/KexDll/buildcfg.h`](VxKex_Vista/KexDll/buildcfg.h)

```c
// Vista向けビルド設定
#define _WIN32_WINNT 0x0600
#define WINVER 0x0600
#define PSAPI_VERSION 1

#define KEXAPI

//
// Set to TRUE to disable the PROTECTED_FUNCTION macro.
// Perhaps useful for debug builds, to catch exceptions under the 
// debugger more quickly - however this will prevent exceptions from
// being logged through VXL.
//
#define DISABLE_PROTECTED_FUNCTION FALSE

#define KEX_COMPONENT L"KexDll"
#define KEX_ENV_NATIVE
#define KEX_TARGET_TYPE_DLL
```

**差分**: Vista向けに3つのプリプロセッサ定義が追加されています:
- `_WIN32_WINNT 0x0600`: Vista以降のAPIのみを使用
- `WINVER 0x0600`: Vista以降の機能のみを有効化
- `PSAPI_VERSION 1`: Vista互換のPSAPIバージョン

#### [`VxKex-NEXT/VxKexLdr/buildcfg.h`](VxKex-NEXT/VxKexLdr/buildcfg.h) vs [`VxKex_Vista/VxKexLdr/buildcfg.h`](VxKex_Vista/VxKexLdr/buildcfg.h)

同様に、VxKexLdrのbuildcfg.hにもVistaプリプロセッサが追加されています。

### 3.3 KxBase.dll のVista特有の実装

[`VxKex_Vista/KxBase/forwards.c`](VxKex_Vista/KxBase/forwards.c) では、Vista特有のAPI構成に対応しています:

```c
// VxKex_Vista: Windows Vista (NT 6.0) does not have kernelbase.dll.
// These functions were exported directly from kernel32.dll and advapi32.dll on Vista.
#pragma comment(linker, "/EXPORT:AccessCheck=kernel32.AccessCheck")
#pragma comment(linker, "/EXPORT:AccessCheckAndAuditAlarmW=advapi32.AccessCheckAndAuditAlarmW")
```

**重要な注意点**:
- Windows 7以降では、kernel32.dllのエクスポートがkernelbase.dllにリダイレクトされています
- Vistaではkernelbase.dllが存在しないため、APIは直接kernel32.dllとadvapi32.dllからエクスポートされます
- このため、Vista向け実装では`kernelbase.`ではなく`kernel32.`または`advapi32.`へのフォワーディングが必要です

### 3.4 KxCfgHlp/setcfg.c の比較

[`VxKex-NEXT/KxCfgHlp/setcfg.c`](VxKex-NEXT/KxCfgHlp/setcfg.c) と [`VxKex_Vista/KxCfgHlp/setcfg.c`](VxKex_Vista/KxCfgHlp/setcfg.c) は同一の実装です。両方とも以下のプロセスでIFEOレジストリを設定します:

1. `LdrOpenImageFileOptionsKey()` でIFEOキーを開く
2. キーが存在しない場合は `KxCfgpCreateIfeoKeyForProgram()` で作成
3. `RegSetValueEx()` で以下を設定:
   - `VerifierDlls` (REG_MULTI_SZ)
   - `GlobalFlag` (REG_DWORD, 0x80000000)
   - `VerifierFlags` (REG_DWORD, 0x80000000)

### 3.5 シェル拡張の実装比較

[`VxKex-NEXT/KexShlEx/dllmain.c`](VxKex-NEXT/KexShlEx/dllmain.c) と [`VxKex_Vista/KexShlEx/dllmain.c`](VxKex_Vista/KexShlEx/dllmain.c) は同一の実装です:

```c
// {9AACA888-A5F5-4C01-852E-8A2005C1D45F}
DEFINE_GUID(CLSID_KexShlEx, 0x9aaca888, 0xa5f5, 0x4c01, 0x85, 0x2e, 0x8a, 0x20, 0x5, 0xc1, 0xd4, 0x5f);

STDAPI DllRegisterServer(VOID) { return E_NOTIMPL; }
```

**重要な注意点**: `DllRegisterServer()` は `E_NOTIMPL` を返します。これは、KexSetup（インストーラー）に登録処理が移動されているためです。Vistaでシェル拡張を有効にするには、手動でレジストリキーを作成する必要があります。

---

## 4. プロパティタブ実装の詳細

### 4.1 IShellPropSheetExt::AddPages 実装

[`VxKex_Vista/KexShlEx/ckxshlex.c`](VxKex_Vista/KexShlEx/ckxshlex.c:222) の `CKexShlEx_AddPages` 関数が、ファイルのプロパティダイアログにVxKexタブを追加する役割を果たします。

#### 主要処理フロー

```c
HRESULT STDMETHODCALLTYPE CKexShlEx_AddPages(
    IN  IKexShlEx               *This,
    IN  LPFNADDPROPSHEETPAGE    AddPage,
    IN  LPARAM                  LParam)
{
    // 1. WindowsディレクトリとKexDirを取得
    GetWindowsDirectory(WinDir, ARRAYSIZE(WinDir));
    Success = KxCfgGetKexDir(KexDir, ARRAYSIZE(KexDir));

    // 2. WindowsディレクトリまたはKexDir内のファイルはスキップ
    if ((PathIsPrefix(WinDir, This->ExeFullPath) && ...)
        || PathIsPrefix(KexDir, This->ExeFullPath)) {
        return E_FAIL;
    }

    // 3. PROPSHEETPAGE構造体を初期化
    ZeroMemory(&Page, sizeof(Page));
    Page.dwSize           = sizeof(Page);
    Page.dwFlags          = PSP_USEREFPARENT | PSP_USETITLE | PSP_USECALLBACK;
    Page.hInstance        = DllHandle;
    Page.pszTemplate      = MAKEINTRESOURCE(IDD_VXKEXPROPSHEETPAGE);
    Page.pszTitle         = L"VxKex";
    Page.pfnDlgProc       = DialogProc;
    Page.pfnCallback      = PropSheetCallbackProc;
    Page.pcRefParent      = (PUINT) &DllReferenceCount;
    Page.lParam           = (LPARAM) This;

    // 4. プロパティシートページを作成
    PageHandle = CreatePropertySheetPage(&Page);
    
    // 5. AddPageコールバックでタブを追加
    Success = AddPage(PageHandle, LParam);
    
    return S_OK;
}
```

#### Vista対応のポイント

1. **`PathCchFindExtension` の使用**: Vistaでは `PathCchFindExtension` が利用可能ですが、エラー処理が重要です。Vista SP1以降で追加されたAPIであるため、SP0環境では失敗する可能性があります。

2. **`.LNK` ファイルの処理**: ショートカットファイルの場合、`GetTargetFromLnkfile()` を呼び出して実際のターゲットパスを取得します。

3. **ファイル拡張子のチェック**: `.EXE` または `.MSI` ファイルのみが対象となります。

```c
if (!StringEqualI(FileExtension, L".EXE") &&
    !StringEqualI(FileExtension, L".MSI")) {
    return E_NOTIMPL;
}
```

### 4.2 シェル拡張の有効化方法

Vistaでシェル拡張を有効にするには、以下のレジストリキーを作成する必要があります:

```cmd
rem CLSID登録
reg add "HKLM\SOFTWARE\Classes\CLSID\{9AACA888-A5F5-4C01-852E-8A2005C1D45F}" /v "" /d "VxKex Shell Extension" /f
reg add "HKLM\SOFTWARE\Classes\CLSID\{9AACA888-A5F5-4C01-852E-8A2005C1D45F}\InprocServer32" /v "" /d "C:\VxKex\KexShlEx.dll" /f
reg add "HKLM\SOFTWARE\Classes\CLSID\{9AACA888-A5F5-4C01-852E-8A2005C1D45F}\InprocServer32" /v "ThreadingModel" /d "Apartment" /f

rem ShellExtensionフィルタ登録（任意のファイルタイプに対して有効化）
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ShellExecuteHooks" /v "{9AACA888-A5F5-4C01-852E-8A2005C1D45F}" /d "" /f
```

**注意**: `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ShellExecuteHooks` は、ファイルを開く際のフックポイントです。プロパティシートタブを追加する場合は、代わりに以下のキーを使用します:

```cmd
rem プロパティシート拡張登録
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\PropertySheetHandlers\{9AACA888-A5F5-4C01-852E-8A2005C1D45F}" /f
```

---

## 5. IFEOレジストリ適用の問題と解決策

### 5.1 問題の概要

ユーザーはnotepad.exeにVxKexを適用しましたが、以下の問題が発生しています:

1. **IFEOレジストリが適用されていない**: スクリーンショットから、IFEOキーが正しく設定されていない可能性
2. **ログファイルが存在しない**: `C:\VxKex\Logs\notepad.exe.vxl` が存在しない

### 5.2 考えられる原因

1. **管理者権限不足**: IFEOレジストリ（`HKLM\...`）への書き込みには管理者権限が必要です
2. **デプロイスクリプトのエラー**: `deploy_to_vista.bat` の実行中にエラーが発生した可能性
3. **パスの指定ミス**: `VerifierDlls` の値に誤ったパスが設定されている可能性
4. **REG_MULTI_SZ形式の誤り**: `VerifierDlls` はREG_MULTI_SZ形式（二重NULL終端）である必要があります

### 5.3 解決手順

#### ステップ1: デプロイスクリプトの手動実行

```cmd
rem 管理者権限でコマンドプロンプトを実行
cd /d "C:\VxKex"

rem DLLのコピー（必要に応じて）
copy VistaDLLs\KexDll.dll .
copy VistaDLLs\KexShlEx.dll .
copy VistaDLLs\KxNt.dll .
copy VistaDLLs\KxBase.dll .

rem IFEO設定
call deploy_to_vista.bat install "C:\VxKex" notepad.exe
```

#### ステップ2: レジストリの直接確認と修正

```cmd
rem IFEOキーの確認
reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe"

rem 既存の値を削除（必要に応じて）
reg delete "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe" /v VerifierDlls /f
reg delete "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe" /v GlobalFlag /f
reg delete "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe" /v VerifierFlags /f

rem 再設定
reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe" /v VerifierDlls /t REG_MULTI_SZ /d "C:\VxKex\KexDll.dll" /f
reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe" /v GlobalFlag /t REG_DWORD /d 2147483648 /f
reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe" /v VerifierFlags /t REG_DWORD /d 2147483648 /f
```

#### ステップ3: Windows再起動またはExplorer再起動

IFEO設定を有効にするには、対象プロセスの再起動が必要です:

```cmd
rem Explorerの再起動（シェル拡張を更新する場合）
taskkill /f /im explorer.exe
start explorer.exe

rem notepad.exeの再起動
taskkill /f /im notepad.exe
```

#### ステップ4: 動作確認

1. Process Explorerで `notepad.exe` のDLLリストを確認
2. Debug ViewでDbgPrint出力を確認
3. ログファイルの存在を確認: `dir C:\VxKex\Logs\notepad.exe.vxl`

### 5.4 deploy_to_vista.bat の詳細

[`deploy_to_vista.bat`](VxKex_Vista/deploy_to_vista.bat) は以下の機能を提供します:

| 機能 | コマンド | 説明 |
|-----|---------|------|
| DLLコピー | `install [TargetDir] [ExeName]` | DLLをターゲットディレクトリにコピーし、IFEOを設定 |
| IFEO設定 | `ApplyIfeo` | 指定したexeのIFEOレジストリを設定 |
| IFEO解除 | `RemoveIfeo` | 指定したexeのIFEOレジストリを削除 |
| シェル拡張登録 | `register` | KexShlEx.dllを手動で登録 |
| シェル拡張解除 | `unregister` | KexShlEx.dllの登録を解除 |

---

## 6. 今後の課題とタスクリスト

### 6.1 現在解決すべき課題

| # | 課題 | 優先度 | 状態 |
|---|------|--------|------|
| 1 | Vista環境でのIFEOレジストリ適用確認 | 高 | 未着手 |
| 2 | VxKex動作確認（Process Explorer/Debug View） | 高 | 未着手 |
| 3 | シェル拡張の手動レジストリ登録 | 中 | 未着手 |
| 4 | ログファイルの出力確認 | 中 | 未着手 |

### 6.2 技術的課題

#### 6.2.1 Vista SP0 vs SP1互換性

- `PathCchFindExtension` はVista SP1以降のAPIです
- Vista SP0環境では、代替実装（`PathFindExtension`等）が必要です

#### 6.2.2 kernelbase.dll の欠落

- Vistaにはkernelbase.dllが存在しません
- KxBase.dllは直接kernel32.dllとadvapi32.dllから関数をエクスポートする必要があります

#### 6.2.3 シェル拡張の登録

- `DllRegisterServer` は `E_NOTIMPL` を返すため、手動レジストリ登録が必要です
- KexSetup（インストーラー）での登録処理が推奨されます

### 6.3 次のフェーズで実装すべき機能

| フェーズ | 内容 | 優先度 |
|---------|------|--------|
| Phase 3 | Vista環境での動作テストとデバッグ | 高 |
| Phase 4 | シェル拡張の手動登録スクリプト完善 | 中 |
| Phase 5 | KexSetupインストーラーのVista対応 | 中 |
| Phase 6 | ドキュメント完善と配布パッケージ作成 | 低 |

### 6.4 テスト計画

#### 6.4.1 ユニットテスト

1. **IFEO設定テスト**: `deploy_to_vista.bat` の各関数が正しく動作するか確認
2. **DLLロードテスト**: notepad.exeがKexDll.dllを正しくロードするか確認
3. **APIリダイレクトテスト**: KxNt.dllとKxBase.dllが正しくAPIをリダイレクトするか確認

#### 6.4.2 インテグレーションテスト

1. **シェル拡張テスト**: ファイルのプロパティダイアログにVxKexタブが表示されるか確認
2. **伝播テスト**: 子プロセスへのVxKex適用が正しく動作するか確認
3. **ログ出力テスト**: .vxlログファイルが正しく生成されるか確認

---

## 付録A: 主要ファイル一覧

| ファイル | 場所 | 説明 |
|---------|------|------|
| [`buildcfg.h`](VxKex_Vista/KexDll/buildcfg.h) | VxKex_Vista/KexDll/ | KexDllのビルド設定 |
| [`dllmain.c`](VxKex_Vista/KexDll/dllmain.c) | VxKex_Vista/KexDll/ | KexDllのエントリポイント |
| [`vxlwrite.c`](VxKex_Vista/KexDll/vxlwrite.c) | VxKex_Vista/KexDll/ | ログファイル書き込み |
| [`ckxshlex.c`](VxKex_Vista/KexShlEx/ckxshlex.c) | VxKex_Vista/KexShlEx/ | シェル拡張実装 |
| [`setcfg.c`](VxKex_Vista/KxCfgHlp/setcfg.c) | VxKex_Vista/KxCfgHlp/ | IFEO設定実装 |
| [`forwards.c`](VxKex_Vista/KxNt/forwards.c) | VxKex_Vista/KxNt/ | ntdllフォワーディング |
| [`forwards.c`](VxKex_Vista/KxBase/forwards.c) | VxKex_Vista/KxBase/ | kernel32フォワーディング |
| [`deploy_to_vista.bat`](VxKex_Vista/deploy_to_vista.bat) | VxKex_Vista/ | デプロイスクリプト |

## 付録B: レジストリキー一覧

| キー | 値 | 説明 |
|-----|---|------|
| `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\<exe>\VerifierDlls` | REG_MULTI_SZ: `C:\VxKex\KexDll.dll` | ロードするDLL |
| `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\<exe>\GlobalFlag` | REG_DWORD: `0x80000000` | Application Verifierフラグ |
| `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\<exe>\VerifierFlags` | REG_DWORD: `0x80000000` | Verifierフラグ |
| `HKLM\SOFTWARE\Classes\CLSID\{9AACA888-A5F5-4C01-852E-8A2005C1D45F}\InprocServer32` | (デフォルト): `C:\VxKex\KexShlEx.dll` | シェル拡張DLL |
| `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\PropertySheetHandlers\{9AACA888-A5F5-4C01-852E-8A2005C1D45F}` | (なし) | プロパティシートフック |

## 付録C: CLSID一覧

| CLSID | 名前 | 説明 |
|-------|------|------|
| `{9AACA888-A5F5-4C01-852E-8A2005C1D45F}` | KexShlEx | シェル拡張COMオブジェクト |

---

## 更新履歴

| 日付 | 版数 | 変更内容 |
|-----|------|---------|
| 2026-07-26 | 1.0 | 初版作成。Vista環境での動作確認方法、実装比較、プロパティタブ実装の詳細を記載 |

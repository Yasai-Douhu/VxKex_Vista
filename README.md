# VxKex_Vista

[![Target OS](https://img.shields.io/badge/Target%20OS-Windows%20Vista%20SP2%20%7C%20Server%202008%20SP2-blue.svg)](https://github.com/Yasai-Douhu/VxKex_Vista)
[![Architecture](https://img.shields.io/badge/Architecture-x64%20%7C%20x86%20(WOW64)-success.svg)](https://github.com/Yasai-Douhu/VxKex_Vista)
[![Based on](https://img.shields.io/badge/Based%20on-VxKex--NEXT-orange.svg)](https://github.com/YuZhouRen86/VxKex-NEXT)
[![License](https://img.shields.io/badge/License-MIT%20%2F%20LGPL-green.svg)](https://github.com/Yasai-Douhu/VxKex_Vista)

**VxKex_Vista** は、[YuZhouRen86 氏による VxKex-NEXT](https://github.com/YuZhouRen86/VxKex-NEXT) をベースとし、**Windows Vista SP2** および **Windows Server 2008 SP2 (NT 6.0)** 向けに特化移植・機能拡張を行った API 拡張・互換性レイヤー (Compatibility Layer) です。

本来 Windows 7 / 8 / 8.1 / 10 / 11 専用となっているモダンなアプリケーションを、Windows Vista / Server 2008 環境上で動作可能にします。

---

## 🌟 VxKex_Vista でできること (主な機能)

### 1. 🚀 モダンアプリケーションの実行サポート
- Windows 7 以降で追加された Win32 API / NT Native API をエミュレート・スタブ実装・安全に転送。
- 新しいランタイム（Visual C++ 再頒布可能パッケージ等）や Chromium ベースのアプリ、最新のユーティリティなどの動作を可能にします。

### 2. 🎭 柔軟な OS バージョン偽装 (OS Version Spoofing)
- アプリケーションが `GetVersion`, `GetVersionEx`, `RtlGetVersion` を呼び出した際に、任意の OS バージョンとして応答させることができます。
- **偽装対応バージョン**:
  - Windows 7 (NT 6.1)
  - Windows 8 (NT 6.2)
  - Windows 8.1 (NT 6.3)
  - Windows 10 (NT 10.0)
  - Windows 11
- アプリケーション側の OS バージョンチェックによる起動制限を回避できます。

### 3. 🖱️ エクスプローラー右クリック「プロパティ」統合 (KexShlEx)
- 実行ファイル (`.exe`) やショートカット (`.lnk`) の右クリックメニューから「プロパティ」を開くと、専用の **「VxKex」タブ** が追加されます。
- GUI から直感的に「VxKex の有効化」「偽装する OS バージョンの選択」「拡張オプションの切り替え」が可能です。

### 4. 🎯 スタンドアロン・ローダー (VxKexLdr)
- レジストリ (IFEO) を変更することなく、手軽に VxKex 互換レイヤーを適用して起動できる `VxKexLdr.exe` を提供。
- 実行ファイルを `VxKexLdr.exe` にドラッグ＆ドロップするか、コマンドラインから指定するだけで即座にテスト起動できます。

### 5. 🔄 子プロセスへの自動伝播 (Process Propagation)
- 親プロセスから起動された子プロセス（インストーラから起動される本体、ランチャーから起動されるゲーム、ブラウザのマルチプロセスなど）に対しても、自動的に VxKex のフックと互換レイヤーが引き継がれます。
- `NtCreateUserProcess` および `NtOpenKey` のインテリジェントなインターセプトにより、シームレスなマルチプロセス動作を実現しています。

### 6. ⚙️ Windows Vista (NT 6.0) 特化のアーキテクチャ最適化
- **`kernelbase.dll` 不在への完全対応**: Windows 7 以降で導入された `kernelbase.dll` は Vista には存在しません。VxKex_Vista ではエクスポート転送定義を Vista ネイティブの `kernel32.dll` / `advapi32.dll` へ再ルーティングし、クラッシュを防ぎます。
- **x64 / x86 (WOW64) の安定動作**: 64-bit OS 上の 64-bit ネイティブアプリおよび 32-bit (WOW64) アプリの双方で正確なローダーフックと初期化判定を行います。
- **多岐にわたる拡張レイヤー**:
  - `KxBase` (Kernel32 / KernelBase 拡張)
  - `KxNt` (NTDLL / Native API 拡張)
  - `KxUser` (User32 / GDI32 拡張)
  - `KxAdvapi` (Advapi32 拡張)
  - `KxCom` (COM / OLE 拡張)
  - `KxCrt` (C ランタイム拡張)
  - `KxCryp` (暗号化 Bcrypt / Crypt32 拡張)
  - `KxDw` (DirectWrite 拡張)
  - `KxDx` (DirectX / DXGI / D3D11 拡張)
  - `KxMi` (Media Foundation 拡張)
  - `KxNet` (ネットワーク拡張)
  - `KxSChanl` (Schannel 拡張)
  - `KxUia` (UI Automation 拡張)

### 7. 🌐 多言語 UI サポート
- 日本語をはじめとする多言語リソースに対応しています。

---

## 💻 動作要件 (System Requirements)

- **対応 OS**:
  - Windows Vista SP2 (x64 / x86)
  - Windows Server 2008 SP2 (x64 / x86)
- **推奨環境**:
  - Windows Update で提供された最新のサービスパックおよびセキュリティ更新プログラムが適用されていること
  - 管理者権限 (インストールおよび IFEO 設定の変更に必要)

---

## 📦 インストール & 使い方

### インストール方法
1. リリースページまたは配布 ZIP アーカイブ (`VxKex_Vista_vX.X.X.zip`) をダウンロードして展開します。
2. 展開されたフォルダ内にある **`install.bat`** を右クリックし、**「管理者として実行」** します。
3. メニュー画面が表示されたら、`1` を入力して Enter を押すとインストールが完了します。
   - `C:\VxKex` および `System32` (WOW64用 `SysWOW64`) に必要なコンポーネントが配置され、シェル拡張が登録されます。

### 基本的な使い方

#### 方法 1: プロパティダイアログから設定する（推奨）
1. 起動させたいアプリケーションの実行ファイル (`.exe`) またはショートカットを右クリックし、**「プロパティ」** を選択します。
2. **「VxKex」タブ** を開きます。
3. **「VxKex をこのプログラムで有効にする」** にチェックを入れます。
4. **「OS バージョン偽装」** ドロップダウンから、アプリケーションが要求するバージョン（例: Windows 10）を選択します。
5. 「適用」をクリックしてプロパティを閉じ、アプリケーションを通常通り起動します。

#### 方法 2: VxKexLdr で手軽に実行する
- レジストリを変更せずに一時的にテストしたい場合、対象の `.exe` ファイルを `C:\VxKex\VxKexLdr.exe` にドラッグ＆ドロップします。
- または、コマンドプロンプトから以下のように実行します:
  ```cmd
  C:\VxKex\VxKexLdr.exe "C:\Path\To\YourApp.exe"
  ```

### アンインストール方法
1. インストーラフォルダまたは `C:\VxKex` 内の **`install.bat`** を管理者として実行します。
2. メニュー画面で `2` (Uninstall) を選択します。
3. レジストリ設定および配置されたファイルが安全に削除・復元されます。

---

## 🏗️ モジュール構成

```
VxKex_Vista/
├── 00-Common-Headers/    # 全モジュール共通のヘッダー定義
├── 00-Import-Libraries/  # リンク用インポートライブラリ
├── KexDll/               # コア DLL (フックエンジン、バージョン偽装、ログ記録)
├── KexShlEx/             # シェル拡張 DLL (プロパティダイアログの VxKex タブ)
├── VxKexLdr/             # スタンドアロン・ローダー実行ファイル
├── KexCfg/               # 設定管理ユーティリティ
├── KexGui/               # GUI コンポーネント用ライブラリ
├── KxBase/               # Kernel32 / KernelBase API エミュレーション
├── KxNt/                 # NTDLL ネイティブ API エミュレーション
├── KxUser/               # User32 / GDI32 拡張
├── KxAdvapi/             # Advapi32 拡張
├── KxCom/                # COM / OLE32 拡張
├── KxCrt/                # C Runtime 拡張
├── KxCryp/               # Bcrypt / Crypt32 拡張
├── KxDw/                 # DirectWrite 拡張
├── KxDx/                 # DXGI / D3D11 拡張
├── KxMi/                 # Media Foundation 拡張
├── KxNet/                # ネットワーク API 拡張
├── KxSChanl/             # Schannel 拡張
├── KxUia/                # UI Automation 拡張
├── Installer/            # 配布用インストーラパッケージ
└── build_all.ps1         # 統合ビルドスクリプト
```

---

## 🛠️ 開発者向け情報 (ビルド方法)

本プロジェクトをソースコードからビルドする場合の要件と手順です。

### ビルド要件
- **開発環境**: Visual Studio 2010 (C++ ツールチェーン / MSVC 10.0)
- **SDK**: Windows SDK 7.1A
- **シェル環境**: PowerShell (ExecutionPolicy Bypass)

### ビルド手順
PowerShell を管理者権限で起動し、リポジトリルートで以下のスクリプトを実行します:

```powershell
# 全モジュール (x64) のビルド & インストーラパッケージ生成
powershell -ExecutionPolicy Bypass -File .\build_all.ps1

# 32-bit (x86 / WOW64) モジュールのビルド
powershell -ExecutionPolicy Bypass -File .\build_all_x86.ps1
```

ビルドされた成果物は `x64/Release/` および `Win32/Release/` に出力されます。

---

## 🤝 クレジット & 謝辞 (Credits)

- **[YuZhouRen86 (VxKex-NEXT)](https://github.com/YuZhouRen86/VxKex-NEXT)**:
  Windows 7 向けの素晴らしい機能拡張・多言語対応・VxKex 改善の基盤を提供してくださったことに深く感謝いたします。
- **[vxiiduu (VxKex)](https://github.com/vxiiduu/VxKex)**:
  VxKex のオリジナル作者である vxiiduu 氏の革新的なプロジェクトに敬意を表します。

---

## 📄 ライセンス

本プロジェクトはベースとなる VxKex / VxKex-NEXT のライセンス（LGPL / MIT 等）に準拠します。詳細についてはソースコード各ファイルのライセンスヘッダーをご確認ください。

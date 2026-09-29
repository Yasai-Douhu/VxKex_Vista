更新内容

- **VS Code 統合ターミナル / PowerShell 3.0 の互換性向上**:
  - VS Code 等から起動された PowerShell 3.0 プロセスへの VxKex 自動伝播を除外する処理を追加 (`KexDll/propagte.c`)。これにより、Windows Vista / Server 2008 上で WinPTY / VistaPty を介した PowerShell 起動時の CLR 初期化エラー (`0x80004005`) を解消しました。
  - VS Code 1.139.1 向けに WinPTY バックエンドを統合するオプショナルパッケージ `Installer/VistaPty` を同梱。
- **Inno Setup 汎用コンテンツベース AVX 検出・抑止**:
  - ファイル名や特定 RVA に依存せず、Delphi / Inno Setup バイナリ内の AVX 初期化コードをシグネチャ照合で検出して無効化する汎用プロファイル (`00-Common-Headers/InnoProfile.h`) を導入。Sublime Text 4215 や任意ファイル名のインストーラでの未定義命令例外 (`0xC000001D` / Runtime error 255) を解消。
- **Inno Setup ショートカット作成互換性 (`IPropertyStore::Commit`)**:
  - Inno Setup によるショートカット作成時、Vista 未対応の AppUserModel プロパティに対する `IPropertyStore::Commit` の `0x80070057` (`E_INVALIDARG`) を適切に処理するフックを追加 (`KxCom/shelllink.c`)。VS Code Insiders インストーラ等の完了を可能にしました。
- **Visual Studio Code - Insiders 対応**:
  - `VistaIsVSCode` (`00-Common-Headers/VistaLaunch.h`) で `Visual Studio Code - Insiders` を認識し、Electron 起動フラグ (`--no-sandbox` 等) を自動付与するように `VistaRun.exe` を更新。
- **VxKex 設定リセットツールの同梱**:
  - 既存の IFEO 設定やレジストリ設定を初期状態にリセットするツール `Reset-VxKex-Config.bat` / `.reg` を `Installer` に追加。`Install.bat` メニューからもワンタッチで実行可能にしました。
- **Sublime Text Package Control の WinINet 12029 回避ドキュメント整備**:
  - Vista 環境で WinINet のモダン TLS 非対応により Package Control で `errno 12029` が発生する問題に対し、`"downloader_precedence": { "windows": ["urllib"] }` を設定して Python 側の urllib (OpenSSL) を使用させる手順をドキュメント化 (`docs/sublime-python314.md`)。

検証

- Windows Server 2008 VM および Windows Vista VM にて、VS Code Insiders の起動・Welcome画面表示、VS Code 統合ターミナルでの PowerShell 実行、Inno Setup (Sublime 4215 / VS Code Insiders / Git 2.55.0.5) の起動・ショートカット作成、Sublime Text 4213/4215 での Package Control 4.2.8 への自動更新と動作を確認。

SHA-256は同梱の `VxKex_Vista-1.2.0.2231-SHA256.txt` を参照してください。

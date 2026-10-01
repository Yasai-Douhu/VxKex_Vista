# VxKex Vista 2.0.0.2232

Windows Vista SP2 / Windows Server 2008 SP2 (x64) 向けのメジャー更新です。Release tag と主要バイナリの製品バージョンは `v2.0.0.2232` / `2.0.0.2232` で一致します。

## 主な更新

- VxKex NEXT 1.2.3.2463 から優先度「高」と判定した4領域を実装し、Installerへ統合しました。
- TLS 1.2/1.3 の処理、Vista CNG 補助、対象プロセスに限定した SSPI 登録、証明書チェーン判定を追加しました。ROOT.sst は同梱しますが、Windows のグローバル Root ストアへ自動登録しません。
- 設定管理 UI と昇格 writer を分離し、アプリ設定、ログ、MSI、context menu の編集を追加しました。更新処理は失敗時に rollback します。
- VxlView の検索、詳細、goto、Unicode export、関連付けを改善し、破損ログと主要な失敗経路を処理します。
- VistaSetup の install/update/uninstall をトランザクション化し、既存設定、native/WOW64 プロファイル、外部設定を保持・復元します。
- VistaPty、Java 17–25 の補助スクリプト、VS Code / Inno Setup / Sublime Text 向けの既存互換設定を同梱します。

## 検証

Windows Server 2008 x64 VM で検証し、x86 は WOW64 で確認しました。TLS、設定管理、ログ閲覧・配布、セットアップ保持の代表正常系と主要 rollback を確認しています。項目別の証拠、残課題、検証範囲は [`docs/next-parity-high-final-report.md`](docs/next-parity-high-final-report.md) を参照してください。

このリリースの検証は全てのアプリケーション・Vista構成に対する完全互換性を保証するものではありません。追加の手動 UI 検証、TLS stress、native 32bit OS の確認などは最終レポートの TODO に記録しています。

## インストール

ZIP を展開し、管理者権限で `install.bat` を実行してください。既存のインストールを更新する場合はメニューから Update を選択してください。

SHA-256 は `VxKex_Vista-2.0.0.2232-SHA256.txt` を参照してください。

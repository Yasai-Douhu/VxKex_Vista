# VxKex NEXT 優先度「高」移植フェーズ 最終報告

判定日: 2026-10-01。ブランチ: `codex/next-parity-high`。
比較対象: NEXT 1.2.3.2463。

## 完了判定

**今回のフェーズは完了条件を満たした。**

元の差分レポートで優先度「高」とした4領域を実装し、Installerへ統合した。
各領域の代表正常系、主要な失敗時の復旧／rollback、必要なx64とx86/WOW64の
証拠を確認した。追加の非致命的な検証は下記のTODOとして分離する。
これは全アプリ・全Vista環境に対する完全互換性や、本家の全機能移植の宣言ではない。
優先度「中」以下は今回の完了条件に含めない。

## 項目別の状態

| 優先度「高」の領域 | 実装済み | 統合済み | 検証済み | 残課題 |
| --- | --- | --- | --- | --- |
| TLS 1.3 / KxSChanl | 両bit DLL、Vista CNG補助、限定SSPI登録、バンドル信頼判断 | KxSChanl/KxCryp/KxAdvapi/KexDll両bit、Certificates/ROOT.sst | TLS 1.2/1.3 HTTP往復、credential取得/解放、CredSSP保持、未適用process分離、証明書エラー拒否 | 失効・分割受信・追加cipher・peer close_notifyの独立試験 |
| 設定管理 UI | ログ設定、MSI、context menu、アプリ一覧／追加／削除／整理、閲覧と昇格writerの分離 | native KexCfg.exe、Kex32/KexCfg.exe | 標準userの閲覧、無権限writer拒否、実SIDへの保存、両GUIの保存・失敗保持・再試行、bulk rollback | UAC画面の承認／取消の手動確認、file picker/multi-selectの追加UI確認 |
| ログ閲覧・配布 | 閲覧、検索、filter、詳細、goto、Unicode export、破損log検証、関連付け | VxlView両bit、共通関連付けとsetup連携、修正reader | 両bit検索・詳細・goto・export、破損10例拒否、allocation/thread/file失敗復旧、関連付け衝突rollback、関連付けから起動 | 保存dialog／通知描画、追加resource stress |
| インストール後の設定保持 | transaction更新／解除、設定保持→復元、remove-all、外部setup cache、SID維持 | VistaSetup.exe、install.bat、Run-VxKexSetup.cmdとhelper連携 | 実install→keep解除→復元→全解除、native/WOW profile保持、無関係設定保持、lock/途中失敗rollback、統合候補配備→解除 | interactive Explorer/UACの追加手動確認、別OS構成・追加filesystem条件 |

## 検証範囲と根拠

環境はWindows Server 2008 x64の実VM。x86はWOW64で実行した。
専用cloneでは所有fixtureと事前状態を検証してから変更し、元のユーザーVMへの
統合候補のインストールは行っていない。直接の画面操作は使わず、必要なUI試験は
表示へ切り替えない専用desktop上のコマンド駆動probeで実施した。

### TLS

- `audit/tls-policy-trusted-{12,13}-{x64,x86}.txt`: protocol値、暗号化HTTPと
  復号応答の内容、client shutdownの成功を確認。
- `audit/tls-bundle-chain-wrong-host-{x64,x86}.txt`: 80090322で拒否。
  `tls-bundle-expired-*`: 80090328、不正署名`tls-bundle-bad-signature-*`: 80090327。
  接続失敗は成功扱いにしない。ROOT.sstはWindowsのglobal Rootへimportしない。
- `audit/high-package-deployment.txt`: 統合候補自身のsetup/settingsを使い、
  登録、credential、native process分離とmachine SSP registry不変を再確認。
  既存8package/CredSSPを維持し、適用processだけにKxSChanlが追加された。
- TLS source自体を変えずにreader等の共有DLLを変更したため、統合後の登録経路を
  回帰確認した。既に通った同目的の全handshake試験を無意味に繰り返していない。

### 設定管理

- `audit/gui-save-failure-retry-both-v2.txt`: 両bitの実GUIを標準userで開ける。
  無権限の実writerはtransaction前に5で拒否。operatorと標準userのhiveを区別し、
  指定SIDの設定だけを保存。両GUIの実保存と、後段競合183によるrollback、
  未保存入力の保持、競合除去後の再入力なしの再試行を確認。Failures=0。
- `audit/bulk-runtime-rollback.txt`: 後続imageのforeign Debugger競合で先行設定を
  rollbackし、foreign値を維持。追加、存在する項目を保持する整理、missing項目の
  整理、削除も確認。Failures=0。
- native writerが両physical MSI imageを扱い、x86 UIからもnative writerを使う。
  標準userへのUAC同意画面そのものを操作した証拠とは区別する。

### ログビューア

- `audit/vxl-details-{x64,x86}.txt`: 検索／case／wildcard／反転／日本語body／severity、
  詳細の本文・日本語・source・日時、goto 0/範囲外/隠れた項目の拒否と有効項目への
  移動を確認。Exit=0 Failures=0。
- `audit/vxl-reader-result-{x64,x86}.txt`: 単一／範囲readの末尾境界と全件、
  破損10例をFILE_INVALID/null handleで拒否。Failures=0。
- `audit/vxl-export-failure-{x64,x86}.txt`: 実workerで不存在directory、排他lock、
  正常UTF-16/BOM/日本語exportを検証。所有probeだけのimport差替えによるallocation/
  thread failureでもowner/cursorが復旧し、開いたlogは読み続けられる。Failures=0。
  TaskDialogは記録用stubであり、通知の描画を検証したとは主張しない。
- `audit/association-result-{x64,x86}.txt`: 登録・解除・他製品のdefault保持、
  command競合拒否、invalid defaultのrollback。Failures=0。
  `audit/viewer-association-launch.txt`で関連付けから期待するviewerの実起動を確認。

### セットアップ・設定保持

- `audit/setup-system-lifecycle-context-launcher.txt`: 実machineのinstall→keep解除→
  再install復元→remove-all。enabled/disabled profile、同basename、native/WOW両経路、
  無関係IFEO、外部custom logsを保持／解除の意味に合わせて確認。Failures=0。
- `audit/setup-transaction-{x64,x86}.txt`: 後続fileのlockで更新失敗しても先行fileと
  registry/association/shell registrationが復元。解除時のlockによる途中deletionも
  rollback。Failures=0。`vistasetup-rollback-result.txt`では実CLI check-only前後の
  registry/product file hashも不変。
- `audit/high-package-deployment.txt`: 最新統合候補の実install、両bit DLLとROOT、
  全5frontendのbyte一致、profile除去、実uninstall後のinstallation/preferences/
  cleanup handler不存在を確認。Failures=0。

## 配布物の最終状態

`Installer`は統合候補の58fileと一致する。ソースから13binaryをbuildし、ROOT.sstを
追加した。x86設定GUIもKex32へ収録。VistaPty、既存Java/Gradle等の補助物は保持。
元のInstallerは`audit/Installer-before-next-high`に保存してある。

- `audit/build-next-high-package-v2/high-package-manifest.json`: 各file/source hashと
  replacementのbuild由来。
- `audit/next-high-installer-integration.json`: 反映先、backup、変更対象、hash一致。
- `audit/next-high-completion-audit.json`: 最終Installer 58file、関連175sourceが
  manifestと一致、主要14結果の終了判定とhash。
- `audit/next-high-tls-evidence.json`: TLS正常／主要証明書エラーの両bit証拠。

この判定では末尾のFailures=0だけで判断せず、probe sourceと各assertion、
実行logの内容も照合した。初回診断・build orchestrationの失敗は成功証拠へ混ぜていない。
手順は`next-parity-high-integration.md`と領域別の検証記録を参照。

## TODO／既知の制限

1. UAC承認／取消のinteractive画面、Explorer再起動のvisible session、save-file
   dialog／TaskDialog描画の追加manual確認。主要writer拒否・rollback・GUI再試行と
   export失敗復旧は確認済みだが、これらの画面操作は未確認。
2. TLS revocation、peer close_notify、さらに多様なfragmentation/cipher、長時間の
   CNG並列stress。期限／hostname／signature／trust境界の代表拒否は検証済み。
3. native 32bit OSのVista、別Server 2008 patch levelや別filesystemでの追加試験。
   今回のx86証拠はServer 2008 x64上のWOW64。
4. 元差分表で独立に優先度「中」としたExplorer常駐bypass、非同期DNS、NT native
   追加機能等は次フェーズ。設定GUIの旧NEXT Explorer bypass項目は未対応として
   無効表示であり、VistaRunによる起動経路と同一機能とは扱わない。
5. 新規GUI／viewerの表示言語辞書の拡充とCI上のVM回帰自動化。

以上は今回の主要機能の成立を妨げる既知障害ではなく、追加確認または次フェーズの
項目である。今回の終了判定を延長する理由にはしない。
git commit、push、Releasesの公開はこのフェーズでは行っていない。

# VxKex NEXT 1.2.3.2463 と VxKex_Vista の機能差分

調査日: 2026-09-30。比較対象はこのリポジトリ内の
`VxKex_NEXT_Source_Code_1_2_3_2463` と、同日時点の VxKex_Vista 作業ツリー
（HEAD `1f7ba61` に未コミット変更を加えた状態）。これは**静的なソース・配布物の監査**であり、
列挙した全 API の VM 実行試験や TLS の相互接続試験ではない。

**後続フェーズの結果（2026-10-01）:** 優先度「高」の実装・配布統合・代表正常系と
主要失敗復旧の判定は `next-parity-high-final-report.md` を参照。
以下は2026-09-30時点の差分監査を保持したもので、現在の未移植一覧ではない。

## 要約

拡張 DLL の大部分はプロジェクト単位で存在し、`KxCrt`、`KxCryp`、`KxDx` などは
公開名の集合が本家と一致する。一方で、公開名が一致しても実装が転送に置き換わった
場合があり、**エクスポート数は動作互換性の証明にならない**。特に `KxSChanl`
（TLS 1.3）、全体設定 GUI、ログビューア、インストール／アンインストールの設定保存、
非同期 DNS、NT ネイティブ API に差がある。

独自の `VistaPty`、Vista 用の Inno Setup 対策、Java 17–25 と Gradle の設定、
PostgreSQL 18 の互換実装は、この本家ソースの構成にない Vista 側の成果である。
ただし、これらの存在だけで総合的な互換範囲が本家を上回ったとは判定できない。

## 調査方法と数値の読み方

各プロジェクト直下の `.c/.h/.asm/.rc` ファイル名、`.def` と有効な
`#pragma comment(linker, "/EXPORT:...")` の**公開名**を比較した。アーキテクチャ別
条件、転送先 DLL の Vista での存在、呼び出し時の意味論はこの数値には含まない。
以下の「未移植」は公開名の集合差であり、すべてが実用機能とは限らない。

| 領域 | NEXT の公開名 | Vista の公開名 | NEXT 側だけにある名前 | 判定 |
| --- | ---: | ---: | ---: | --- |
| `KexDll` | 113 | 103 | 13 | 新しいローダー・NT API 経路が一部欠落 |
| `KxBase` | 1942 | 1941 | 2 | AppX/package API 2 件など |
| `KxCom` | 497 | 496 | 2 | WinRT activation factory 2 件 |
| `KxMi` | 311 | 296 | 15 | TBS/TPM 群 |
| `KxNet` | 762 | 761 | 1 | `GetAddrInfoExOverlappedResult`。既存名にも実装差あり |
| `KxNt` | 2181 | 2174 | 14 | thread alert、RTL 補助関数など |
| `KxUia` | 93 | 90 | 3 | provider disconnect / changes event |
| `KxUser` | 2334 | 2333 | 1 | touch history |

上記以外の比較対象拡張 DLL は公開名の集合差が 0。`KxSChanl` はソース 26/26
ファイルがバイト単位で同一だが、DLL が配布されていない。`KxAdvapi` は公開名が
一致する一方、後述の実装が本家の互換処理から Vista ネイティブ転送へ変わっている。

## 優先して回復する機能

| 優先 | 機能・証拠 | 現状と実装可能性 |
| --- | --- | --- |
| 高 | **TLS 1.3 / `KxSChanl`**。本家 `KxSChanl` 全ソースは存在するが、`build_all_x64_log.txt` で `_byteswap_ushort/_ulong/_uint64` の未解決によりリンク失敗。`Installer` に DLL と `Certificates/ROOT.sst` がない。本家 `KxAdvapi/regqval.c` の SecurityProviders 統合、および `KexDll/dllrewrt.c` の `webio.dll`・`wininet.dll`・`sspicli.dll` の強制書換えもない。 | **可能だが一式で移植**。ビルド修正だけでは TLS は有効にならない。x86/x64 ビルド、SSPI 登録、証明書の信頼判断、TLS 1.2/1.3 接続・失敗経路の検証が必要。現在の `KxCryp/credhndl.c` は `KxSChanl` 失敗時にネイティブ Schannel へ戻す。Vista の Schannel 自体に TLS 1.3 を追加する方針ではない。 |
| 高 | **設定管理 UI**。本家 `KexCfg/gui.c` は全体のログ設定、MSI 適用、Explorer 起動制限回避、コンテキストメニュー、アプリ一覧の追加・削除・不存在項目の整理を持つ。Vista `KexCfg/main.c` は `/EXE` 等を処理する昇格用 CLI のみ。`KxCfgHlp` には設定・列挙用関数が多数残る。 | **可能**。Vista の UAC に合わせて、閲覧用 UI と昇格する書込み処理を分ける。まず CLI から設定一覧・全体設定を扱えるようにし、次に GUI を移すと検証しやすい。 |
| 高 | **ログ閲覧・配布**。本家 `VxlView` プロジェクトと `.vxl` 関連付けがない。Vista 側のログ生成コードは残る。 | **可能**。閲覧・検索・フィルタ・エクスポートを移植し、インストーラーへ関連付けを追加する。既存 `.vxl` を両アーキテクチャの VM で開く試験が必要。 |
| 高 | **インストール後の設定保持**。本家 `KexSetup` の「互換設定を保持してアンインストールし、再インストールで復元」は `KxCfgHlp/preserve.c` と連携する。Vista は `Installer/install.bat` で、該当ソース・選択 UI がない。 | **可能**。Vista のインストーラーへトランザクション相当の更新・ロールバック、IFEO 設定の保存／復元を段階的に導入。Windows 7 専用の前提条件判定はそのまま移植しない。 |
| 中 | **非同期 DNS**。本家 `KxNet/gaiasync.c` は `GetAddrInfoExW`、Cancel、OverlappedResult を連携して実装。Vista では `GetAddrInfoExW` を OS に転送し、`GetAddrInfoExCancel` は常に `WSA_INVALID_HANDLE`、OverlappedResult は未公開。 | **可能**。本家の非同期完了・キャンセル・タイムアウト処理を Vista のスレッドプールで確認して移植。ネットワーク利用アプリを試験対象にする。 |
| 中 | **NT ネイティブ互換**。本家 `KexDll/ntfile.c` は新しい rename/delete 情報クラスを古い形式へ変換する。`ntalrtid.c` は thread alert/wait を実装。Vista 側はこれらを欠き、`KexDll/ntthread.c` 内の同名関数は未実装を返す。 | **可能だが個別検証が必要**。ファイル操作のフラグ互換は移植候補。thread alert/wait は同期と終了時の競合を含むため、ストレス試験を伴う。`ntvmem.c` の `NtAllocateVirtualMemoryEx` は本家でも TODO のコメントアウトで、完成機能に数えない。 |
| 中 | **GDI+ 最新修正**。本家 1.2.3.2463 の変更は `KexDll/dllrewrt.c` の `GdiPlus.dll` 強制書換えと `KxBase/module.c` の読み込み処理に現れる。Vista 側に該当コードがない。 | **移植候補**。Vista の GDI+ の DLL 読み込み経路とフォント描画で再現試験を先に行う。Windows 7 のオフセットや前提をそのまま転用しない。 |
| 中 | **Explorer 起動制限回避**。本家 `CpiwBypa` と `CpiwBypaLdr`、KexSetup のログオンタスクが欠ける。Vista に `VistaRun` はあるが、Explorer 全体への自動適用とは別機能。 | **可能だが高リスク**。Explorer への常駐・注入で安定性への影響が大きい。まず VistaRun で満たせない起動例を限定し、VM スナップショットと Explorer 再起動試験を用意する。 |
| 中 | **アプリ別の自動対策**。本家 `KexDll/ash.c` と `dllmain.c` は静的リンク Qt 6 を PE セクションから検出して描画対策を適用し、Godot には Vulkan 回避、Zig には ConDrv エミュレーションを有効化する。Vista は動的 `Qt6WebEngineCore` 検出を持つが、これらの経路を欠く。 | **条件付きで可能**。検出対象が Vista 上でも起動可能か、対象アプリと GPU 構成を確認した上で個別に移植する。Godot の Vulkan 回避は全環境へ無条件に広げない。 |
| 中 | **低レベルのコンソール処理**。本家 `KexDll/ntcondrv.c` は対象プロセスのコンソールハンドルに対する `NtWriteFile` 等を扱う。Vista には高水準の ANSI/コンソール対策や `VistaPty` があるが、この NT 呼び出しの置換はない。 | **可能だが独立に検証**。Zig など NT API を直接呼ぶアプリで必要性を測る。`VistaPty` は VS Code のターミナル接点を WinPTY へ差し替えるため、代替範囲は一致しない。 |

## 用途を絞って評価する機能

| 機能 | 評価 |
| --- | --- |
| `KxAdvapi` の `RegGetValueA/W` 互換処理 | 本家は Windows 7 の `REG_EXPAND_SZ` 動作差を補正。Vista は OS 関数を転送。Vista で同じ差が出るかテストし、必要な場合だけ追加する。 |
| `KxAdvapi/token.c` の Vulkan 用 `OpenProcessToken` 回避 | Vista は OS 関数へ転送。特定の Vulkan ローダー構成に依存するため、該当アプリの再現がある場合に移植。 |
| MacType と Application Verifier の相互作用対策 | 本家 `KexDll/initapc.c` と `dllmain.c` に APC による回避がある。Vista には MacType 関連の一部判定が残るが、この APC 経路はない。MacType を導入した VM でのみ再現と移植価値を判断する。 |
| `KxMi` の 15 個の TBS/TPM 公開名 | Vista/Server 2008 に TBS 自体はあるが、元 DLL に各公開名があるかを VM で照合してから転送・互換実装を決める。`Tbsi_GetDeviceInfo` の本家互換実装も Vista 側にはない。 |
| `KxUia` の 3 API | Vista Platform Update の UI Automation を利用できる範囲でエミュレーション可能か調べる。`UiaDisconnectProvider` は Windows 8 以降の API で、単なる成功スタブではクライアントの参照解放が保証されない。 |
| `KxUser/GetPointerTouchInfoHistory` | Windows 8 以降のポインター入力履歴 API。Vista には同じ入力モデルがなく、マウス互換や単一サンプルの限定実装なら可能だが、完全互換は難しい。 |
| `KxCom/RoRegisterActivationFactories` 等と `KxBase` の package API | DLL 名だけの追加では WinRT/AppX の OS インフラを作れない。必要なアプリが判明したときに限定的な実装を検討する。 |
| `KexKMSD` | 本家の `DriverEntry` はメッセージを出して unload routine を登録する程度。現状で利用者向けの機能差とみなさず、移植優先度は低い。 |

## 配布・品質面での差

- 本家 `KexSetup` は GUI、前提条件確認、ログビューア関連付け、設定保存を統合。
  Vista のバッチインストーラーは動くが、このライフサイクル機能は未移植。
- 本家は中国語簡体・繁体、チェコ語、ロシア語、ウクライナ語の辞書を持つ。
  Vista には `KexMLS` のソースがあるが、`Installer` に辞書と `Globalization`
  配置がなく、表示言語の実運用は別途確認が必要。
- 本家の `KexCfg` はアプリ一覧を管理できる。Vista ではファイルごとの
  プロパティタブと昇格 CLI はあるが、一覧の閲覧・クリーンアップ UI はない。
- 本家の `01-Tests` は少数のログ用テスト、Vista の `tests/` と `docs/` は
  実機で発見した問題の回帰プローブが充実している。ただし CI で両 VM・両ビット幅を
  継続検証する仕組みまでは確認できない。

## Vista 側だけの価値と次の開発順

`VistaPty` は VS Code の `conpty.node` 接点を WinPTY へ差し替える任意の補助部品。
本家ソースには同等の ConPTY アダプターがない。Windows の
`CreatePseudoConsole` は Windows 10 1809 / Server 2019 以降の API なので、
NT 6.0 ではこの方式の補助部品が実用的である。一方、VistaPty は VS Code の
特定の Node-PTY 接点に依存し、一般アプリ全体の ConPTY を提供するものではない。
`tools/VistaPty/README.md` の検証範囲を超える版は個別確認する。

推奨順序は **(1) 配布物・ソース・公開名の自動照合を CI 化 → (2) `KxSChanl`
のビルドと SSPI/TLS 一式 → (3) 設定 GUI・ログビューア・安全な更新／復元 →
(4) 非同期 DNS と NT ファイル／同期 API → (5) アプリの不具合で必要と判明した
WinRT・入力 API**。単純な公開名追加より、VM 上のアプリと小さな API プローブで
戻り値と副作用を確かめる方が互換性の改善につながる。

参考: [Microsoft の Vista Platform Update](https://learn.microsoft.com/en-us/windows/win32/win7ip/platform-update-for-windows-vista-overview)、
[CreatePseudoConsole の動作要件](https://learn.microsoft.com/en-us/windows/console/createpseudoconsole)、
[GetPointerTouchInfoHistory の動作要件](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getpointertouchinfohistory)、
[UiaDisconnectProvider の動作要件](https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcoreapi/nf-uiautomationcoreapi-uiadisconnectprovider)。

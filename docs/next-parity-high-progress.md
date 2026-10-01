# VxKex NEXT 優先度「高」の移植・検証状況

更新: 2026-10-01。作業ブランチ: `codex/next-parity-high`。
対象一覧は `vxkex-next-2463-parity-audit.md` を参照。

**最終状態: 今回の優先度「高」フェーズは完了条件を満たした。**
実装／統合／検証／残課題の最終判定は
`next-parity-high-final-report.md`を参照。以下は作業時点ごとの履歴であり、
過去の「未配布」「残工程」記載は最終状態を示すものではない。

## TLS / KxSChanl

Server 2008 x64 VM で、32bit / 64bit のテストプロセスから TLS 1.3
ハンドシェイクが成功した。`QueryContextAttributesW` の
`SECPKG_ATTR_CONNECTION_INFO` は `dwProtocol=0x00002000`
(`SP_PROT_TLS1_3_CLIENT`) を返した。ホストの Python/OpenSSL サーバーでも
TLSv1.3 / TLS_AES_256_GCM_SHA384 を確認した。

実装内容:

- KxSChanl の x86/x64 ビルドを修正。
- SecurityProviders の読み込みを KxAdvapi で拡張。Vista では sspicli.dll が
  存在しないため、secur32.dll の advapi32.dll インポートを限定して書き換える。
  secur32.dll の全インポートを書き換える試作では CREDSSP が消えたため、
  限定した実装に変更。SSPI のネイティブ 8 パッケージと KxSChanl の計 9 個を
  x86/x64 で確認済み。
- Vista の CNG はハッシュ・対称鍵オブジェクトの自動メモリ確保に対応しない。
  KxCryp の CreateHash、DuplicateHash、GenerateSymmetricKey、ImportKey、
  DuplicateKey が必要なバッファを確保し、DestroyHash / DestroyKey の成功後に
  解放する。呼出元が渡したバッファは解放しない。複製に必要なサイズも記録する。
- KxCryp 自身のハッシュ破棄も所有メモリの解放処理へ通す。
- x86 KxSChanl を新しい KxCryp import library で再リンク。旧リンク結果では
  DuplicateHash / DestroyHash がネイティブ ncrypt.dll を直接呼び、互換処理を
  通っていなかった。
- x64 ビルドスクリプトの引数引用と成否判定を修正。コンパイルに失敗した場合は
  停止し、古いオブジェクトでリンクを続けない。
- SSPI の EncryptMessage / DecryptMessage 用テーブル補正を Vista の
  secur32.dll 経路にも適用。構造体を検証し、書換え完了後に完了フラグを立てる。
- 証明書ポリシーの結果構造体を初期化。検証 API の失敗を正の Win32 エラーとして
  返す経路と、未知の証明書エラーを成功扱いする経路を修正。

テスト:

| 検証 | x64 | x86 |
| --- | --- | --- |
| SSPI 登録・CREDSSP を含む既存パッケージ保持 | 成功 | 成功 |
| KxSChanl credential の取得・破棄 | 成功 | 成功 |
| SHA-256 `abc` の既知値 | 成功 | 成功 |
| ハッシュの複製と元ハッシュ破棄後の使用 | 成功 | 成功 |
| 呼出元が確保したハッシュメモリからの自動複製 | 成功 | 成功 |
| AES の既知値、鍵の作成・複製・インポート・破棄 | 成功 | 成功 |
| TLS 1.3 ハンドシェイク | 成功 | 成功 |
| TLS 1.3 暗号化 HTTP 要求・復号応答の内容確認 | 成功 | 成功 |
| TLS 1.2 暗号化 HTTP 要求・復号応答の内容確認 | 成功 | 成功 |
| 証明書の自動検証を有効にした TLS 1.2 / 1.3 接続 | 成功 | 成功 |
| 未信頼の自己署名証明書の拒否 | 成功 | 成功 |
| サーバーによるクライアント close_notify 検証 | 成功 | 成功 |
| ROOT.sst のルートだけで CA 署名済みサーバー証明書を検証 | 成功 | 成功 |
| バンドルで信頼したチェーンのホスト名不一致を拒否 | 成功 | 成功 |
| Windows で信頼済みでもバンドルにないルートを拒否 | 成功 | 成功 |

プローブソースは `tests/cng_object_probe.c`、
`tests/kxschanl_registration_probe.c`、`tests/kxschanl_tls_client.c`。
ハンドシェイク用サーバーは `tests/tls_test_server.py`。
生ログは Git 対象外の `audit/cng-result-x64.txt`、`cng-result-x86.txt`、
`tls-handshake-x64-auto-objects.txt`、`tls-handshake-x86-auto-objects.txt`。

TLS プローブを拡張し、EncryptMessage / DecryptMessage による HTTP の往復を
確認した。TLS 1.3 は TLS_AES_256_GCM_SHA384、TLS 1.2 は
ECDHE-RSA-AES256-GCM-SHA384。自動証明書検証の成功試験では、ローカルの
自己署名証明書を VM の Root ストアに一時登録し、試験後に削除した。
削除後は x86/x64 とも SEC_E_UNTRUSTED_ROOT (0x80090325) で拒否された。
この試験はシステムのルートストアを使う経路の検証であり、ROOT.sst の検証ではない。
サーバーはクライアントの close_notify を確認して正常終了した。クライアント側の
相手の close_notify の認証・応答処理はまだ独立に検証していない。

最新のログは `audit/tls-policy-trusted-13-x64.txt`、`tls-policy-trusted-13-x86.txt`、
`tls-policy-trusted-12-x64.txt`、`tls-policy-trusted-12-x86.txt`、
`tls-policy-untrusted-x64.txt`、`tls-policy-untrusted-x86.txt`。
最新 x64 KxSChanl は `audit/build-policy-20261001/KxSChanl/KxSChanl.dll`、
x86 は `Win32/Release/KxSChanl/KxSChanl.dll`。

残る検証・配布作業:

- 相手の close_notify の認証・応答と切断状態の確認。
- 分割受信、複数暗号スイート、証明書の期限切れ・失効・不正署名の拒否。
- クロス署名など、代替チェーンのルート選択とエラー時の回帰試験。
- 証明書ストアと ROOT.sst の配布方針。
- CNG API の無効引数・失敗経路・反復／並行呼出しの回帰確認。
- VM の正式配置と Installer の x86/x64 バイナリ・インストール処理への反映。

VM の修正前スナップショット `Before_NEXT_TLS_port_20260930` と DLL の
バックアップを保持している。最新の KxCryp は診断用フォルダと x86 の
`C:\VxKex\Kex32` に配置した。Installer はまだ TLS 一式として完成していない。

Microsoft の説明でも CNG オブジェクトの自動メモリ管理は Windows 7 以降の
機能とされている:
[BCryptDuplicateHash](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptduplicatehash)。

ROOT.sst 経路の本家コードは CERT_CHAIN_ENGINE_CONFIG.hExclusiveRoot を使用する。
[Microsoft の仕様](https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/ns-wincrypt-cert_chain_engine_config)
では、このメンバーは Windows 7 / Server 2008 R2 以降。Vista 向けには、
メモリ上のコレクションストアに受信した中間証明書とバンドルを加え、ネイティブ
API にチェーンを構築させる処理に変更した。チェーン末尾が自己発行のルートで、
バンドル内の証明書と DER 全体が一致する場合のみ、そのルートを信頼する。
SSL ポリシーでは未知 CA の扱いだけを補正し、ホスト名・有効期限・用途などの
検証は維持する。システムストアへの永続登録は行わない。

`tests/create_tls_fixture.ps1` で CA と別のサーバー証明書を生成し、VM で
CA を Windows Root ストアに登録せず、バンドルだけからチェーンを構築する
TLS 1.3 HTTP 通信を x86/x64 で確認した。ホスト名不一致は両方で
SEC_E_WRONG_PRINCIPAL (0x80090322)。さらにテスト CA を Windows Root に
一時登録し、バンドル無しなら通信成功、NEXT の実際の ROOT.sst を使うと
SEC_E_UNTRUSTED_ROOT (0x80090325) で拒否することを確認した。
テスト CA の Windows Root 登録は削除済み。VM の ROOT.sst は NEXT の元の
ファイルに置き換え、テスト CA を含むファイルは ROOT.test-fixture-disabled.sst
という非アクティブな名前で保持している。

ログ: `audit/tls-bundle-chain-valid-x64.txt` / `-x86.txt`、
`tls-bundle-chain-wrong-host-x64.txt` / `-x86.txt`、
`tls-bundle-system-baseline-x64.txt`、`tls-bundle-excluded-x64.txt` / `-x86.txt`。
最新 x64 KxSChanl は `audit/build-bundle-20261001/KxSChanl/KxSChanl.dll`。
クロス署名など代替チェーンが複数ある場合の選択はまだ独立に検証していない。

## その他の優先度「高」

### 2026-10-01: クロス署名の代替チェーン

`tests/create_tls_cross_signed_fixture.ps1` で、同じ中間 CA 鍵をルート A/B が
それぞれ署名する証明書セットを生成した。Windows の機械 Root ストアに A を
一時登録し、ROOT.sst には B だけを配置。TLS サーバーは両中間証明書を送信。
修正前は x64 が 0x80090325 で拒否した（`audit/tls-cross-signed-before-x64.txt`）。
A を Windows に登録する前には同じバンドルで成功しており、Windows の優先する
ルートとバンドルのルートが異なる場合に問題が発生することを切り分けた。

チェーン構築で `CERT_CHAIN_DISABLE_PASS1_QUALITY_FILTERING` と
`CERT_CHAIN_RETURN_LOWER_QUALITY_CONTEXTS` を指定し、主候補と代替候補を調べる。
バンドル内ルートに完全一致する候補だけを SSL ポリシーで検証し、候補の全検証が
成功した場合だけ接続を許可する。フラグの動作は
[Microsoft の CertGetCertificateChain 文書](https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-certgetcertificatechain)
を参照した。

| 修正後の試験 | x64 | x86 |
| --- | --- | --- |
| バンドル B への代替経路で TLS 1.3 HTTP 往復 | 成功 | 成功 |
| 同じ代替経路でホスト名不一致 | 0x80090322 で拒否 | 0x80090322 で拒否 |
| NEXT の本来のバンドル（A/B なし） | 0x80090325 で拒否 | 0x80090325 で拒否 |

ログ: `audit/tls-cross-signed-{after,wrong-host,excluded}-{x64,x86}.txt`。
最新 DLL: x64 `audit/build-cross-signed-20261001/KxSChanl/KxSChanl.dll`、
x86 `Win32/Release/KxSChanl/KxSChanl.dll`。VM の診断配置を更新済み。
試験後は A を Windows Root ストアから削除し、ROOT.sst を NEXT の元の
バンドルへ復元した。証明書の秘密鍵と試験用バンドルは配布対象に含めない。

### 2026-10-01: 証明書エラーとバンドル読込みの回帰確認

Server 2008 VM に最新の診断用 KxSChanl を配置し、以下を確認した。

| 検証 | x64 | x86 |
| --- | --- | --- |
| 期限切れ証明書 | 0x80090328 で拒否 | 0x80090328 で拒否 |
| 証明書の署名を破損 | 0x80090327 で拒否 | 0x80090327 で拒否 |
| clientAuth のみの証明書 | 0x80090349 で拒否 | 0x80090349 で拒否 |
| 不正な内容の ROOT.sst | 資格情報取得が 0x80090304 | 資格情報取得が 0x80090304 |
| 正常な ROOT.sst で TLS 1.3 HTTP 往復 | 成功 | 成功 |
| ROOT.sst 不在時のシステムストア経路 | 資格情報取得成功、未信頼 CA を 0x80090325 で拒否 | 今回は未再検証 |

`SppLoadRootCertificates` は、ファイル／ディレクトリが存在しない場合だけ
システムストアへのフォールバックを許可するよう変更した。既存バンドルの
破損・アクセス失敗とパス構築失敗は資格情報取得を失敗させる。
空の正常なストアは空の信頼集合として扱い、システムストアには切り替えない。

clientAuth の x86 初回ログは接続タイムアウトであり、拒否の証拠として
採用しなかった。再試験では資格情報取得後に 0x80090349 で拒否された。
証明書生成は `tests/create_tls_negative_fixtures.ps1`。
ログは `audit/tls-bundle-{expired,bad-signature,client-only,corrupt}-{x64,x86}.txt`、
`audit/tls-store-errors-valid-{x64,x86}.txt` と `audit/tls-store-missing-x64.txt`。
最新 x64 DLL は `audit/build-store-errors-20261001/KxSChanl/KxSChanl.dll`、
x86 DLL は `Win32/Release/KxSChanl/KxSChanl.dll`。
VM の診断フォルダに配置済み。試験後、VM の有効な ROOT.sst は NEXT の
元のバンドルに復元した。正式配布への反映は引き続き未完了。

残る TLS 検証は代替チェーン、失効、受信分割、相手の close_notify、
CNG の失敗／並行経路、正式配置。設定 UI・ログビューア・設定保持／復元も
優先度「高」の対象として引き続き未完了。

設定管理 UI、ログビューア、インストール時の設定保持／復元は未移植。
TLS 一式の検証・配布を終えた後、これらを順に進める。
優先度「高」の移植全体は未完了。

### 2026-10-01: 設定管理 UI の前提となる列挙処理

画面移植に先立ち、Vista 側の設定一覧の取得を修正した。本家の一覧は IFEO の
UseFilter / FilterFullPath を使うが、Vista の設定は画像名のキーへ直接保存する。
旧列挙関数では通常の Vista 設定が見えず、WOW64 側の設定も対象外だった。

- ベースキーの Application Verifier 設定も列挙。
- Vista x64 は IFEO の 64bit / 32bit 両ビューを走査。
- 新規書込みで `KEX_ConfigPath` に最後に設定したフルパスを記録。
  これは一覧の表示用で、IFEO の画像名による適用範囲は変えない。
- パス情報のない既存設定は basename 項目として返す。
  呼出元はこの項目を不存在ファイルとして自動削除してはならない。
- 内部の `{VxKexPropagationVirtualKey}` はアプリ一覧から除外。
- legacy コールバック中止時の未初期化ハンドル Close と、フィルター読取り
  バッファの未初期化を修正。
- `KexCfg /LIST:<絶対出力パス>` に UTF-16 TSV 出力を追加。
  ファイルを上書きしない。書込み用 CLI の動作は維持。

Server 2008 VM で `/LIST` により既存設定を取得した後、専用の x64/x86
ConfigEnumFixture 実行ファイルを設定。両方がフルパス項目として各一回列挙された。
試験後に両設定を削除し、一覧が試験前の 108 項目に戻ることを確認した。
内部伝播キーは最終一覧には含まれない。
ログは `audit/config-list-{before,fixtures,final}.tsv`。
専用の試験 EXE を置いただけで、ユーザーの既存アプリ設定は変更していない。

KxCfgHlp のビルドにリテラル引用符と標準出力の戻り値混入があり、コンパイル失敗を
見落としていたため修正。x64/x86 とも再ビルド成功。ホストが既存の elevate.obj を
書込み拒否したため、x64 は新規出力ディレクトリを使用した。
最新版の診断 KexCfg は `audit/build-enum-final/KexCfg/KexCfg.exe`。
VM の `C:\VxKexProbe\NextParity\KexCfg-enum.exe` に配置済み。

GUI 自体はまだ未移植であり、今回の CLI を完成した管理画面とは扱わない。
次は本家の画面・追加／削除／整理操作と Vista の昇格処理を接続する。
ログビューア、設定保持／復元、TLS の残る検証・配布も引き続き対象。

### 2026-10-01: 本家 KexCfg GUI の初期移植

本家 1.2.3.2463 の `KexCfg/gui.c`、resource.h、アイコン、英語のダイアログ／
メニュー定義を移植した。既存の `/EXE` と `/LIST` の CLI を維持し、引数なしで
GUI を開く。Vista 側 KxCfgHlp の関数シグネチャに合わせ、KexGui / KexMls / KexSmp
をリンクする。Common Controls v6 マニフェストとコントロール初期化を追加した。

変更点:

- パス不明の既存設定を一覧に表示し、整理による自動削除から除外。
  パスが不明な項目の実行・プロパティ・削除は拒否し、フルパスで追加するよう案内。
- 全体設定でログ設定の保存失敗を確認し、トランザクションをロールバック。
- 全体設定とアプリ追加／削除の CommitTransaction の失敗を確認。
- NEXT 専用 Explorer BHO の設定は無効なコントロールとして保持し、その API を
  Apply から呼ばない。VistaRun の設定を別の BHO 設定で変更しない。
- Vista のタブ背景テーマを使用し、COM 初期化を終了時に解放。

`tests/kexcfg_gui_probe.c` は指定の診断用ビルドだけを起動し、Windows メッセージ
でコントロールの存在・アプリ一覧件数・Cancel 後の終了を確認する。
画面への直接入力やユーザー設定の Apply は行わない。
Server 2008 x64 でタイトル `VxKex Vista Global Settings`、全 15 コントロール、
108 件の設定、子プロセス終了コード 0、プローブ結果 0 を確認。
終了後の `/LIST` 出力は試験前の一覧と一致した。

ビルド: `audit/build-gui-20261001/KexCfg/KexCfg.exe`。
VM: `C:\VxKexProbe\NextParity\KexCfg-gui.exe`。
ログ: `audit/config-gui-smoke-final.txt`、`audit/config-list-after-gui.tsv`。
正式な KexCfg や Installer の EXE はこの段階では置き換えていない。

残る GUI 作業:

- 管理者資格情報を切り替える UAC とユーザーごとのログ設定の確認。
  現段階では既存の requireAdministrator マニフェストを維持している。
  閲覧と昇格して書き込む経路の分離はまだ実装していない。
- 専用 fixture で追加・削除・整理・全体設定の成功／失敗／ロールバック検証。
- ファイル削除後に GetBinaryType が使えない WOW64 設定のビュー選択を確認。
- 画面表示、x86 UI ビルド、正式配布への反映。

GUI 起動の成功だけで設定管理全体を検証済みとはしない。
ログビューア、設定保持／復元、TLS の未完了項目も引き続き残る。

### 2026-10-01: 消失した WOW64 画像の設定削除と x86 GUI

GUI の整理処理が呼ぶ設定削除について、画像ファイルが消えた場合のビュー選択を
修正した。GetBinaryType が使えない場合は KEX_ConfigPath を両ビューで照合し、
32bit の一致だけなら 32bit 設定を選ぶ。両ビューに同じパスが記録されていれば
ERROR_DUP_NAME で拒否する。削除開始時にビューを保存し、途中で KEX_ConfigPath を
消しても削除対象を変更しない。設定削除の最終処理が常に 64bit ビューを開いていた
箇所も修正した。

Vista の IFEO 読取りは、呼出プロセスのビット幅にかかわらず対象画像のビューを
明示して開くよう統一した。WOW64 のネイティブ LdrOpenImageFileOptionsKey に
64bit 対象の読取りを任せる経路を廃止。Vista 以外は従来のネイティブ経路を維持。

`tests/config_missing_image_probe.c` は同名の x64/x86 専用画像を別ディレクトリに
置き、既存の試験キーがある場合は変更せず停止する。Server 2008 VM で x64/x86
両プローブを実行し、以下の全チェックが成功した（Failures=0）。

- 両方の画像を設定し、x86 画像を一時的に別名へ移動。
- 消失した x86 画像の VxKex 設定だけを削除。
- 同名 x64 画像の有効設定と、x86 キーの無関係な DWORD 値を保持。
- 両方の記録パスが同じで画像が消えた場合は拒否し、両キーのメタデータを保持。
- 画像を復元し、試験用設定だけを削除。

ビルド手順は `tests/build_config_missing_image_probe.ps1`。
ログは `audit/config-missing-image-result-{x64,x86}.txt`。
これは削除 API とビュー選択の検証であり、GUI の Clean 操作全体の試験ではない。

`build_kexcfg.ps1 -Architecture x86` も追加し、最新の共通コードを両ビット幅で
ビルド。Server 2008 上で x64/x86 の GUI に全 15 コントロールと 108 件の一覧が
生成され、Cancel 後に終了コード 0 で閉じることを再確認した。
ログは `audit/config-gui-missing-final-{x64,x86}.txt`。
診断用配置: `C:\VxKexProbe\NextParity\KexCfg-gui.exe` と
`C:\VxKexProbe\NextParity\x86\KexCfg-gui.exe`。
最新 x64 のビルドは `audit/build-missing-image-final/KexCfg/KexCfg.exe`、
x86 は同ディレクトリの `KexCfg-x86/KexCfg.exe`。

GUI からの Apply・追加・削除・Clean とロールバック、標準ユーザー／UAC、
正式配布の検証は引き続き残る。今回の試験ではユーザーの既存の有効アプリ一覧は
試験前と一致した。優先度「高」の移植全体は未完了。

### 2026-10-01: VxlView 初期移植と両ビット幅のログ読込み

NEXT 1.2.3.2463 の VxlView ソースとリソースを `VxlView/` に移植。
Vista 用の Common Controls v6・asInvoker マニフェストを追加し、
`build_vxlview.ps1` で VS2010/SDK7.1 による x64/x86 ビルドを追加した。
名称は VxKex Vista に変更。Common Controls 初期化・ダイアログ生成の失敗と
GetMessage のエラーを確認し、ウィンドウがない状態で待ち続けたり、エラーを
正常終了として扱ったりする起動処理を修正した。

Server 2008 VM の診断用ディレクトリに両ビット幅を配置。
`tests/vxl_fixture_probe.c` は実際の Vxl API で重要度 6 種のログを生成し、
件数・重要度・行番号・日本語本文を再読込みして確認する。
既存ファイルを上書きしない FILE_CREATE を使用。x64/x86 の両方で Failures=0。
ログは `audit/vxl-fixture-result-{x64,x86}.txt`。

`tests/vxlview_gui_probe.c` は指定の診断用ビューアだけを起動し、
Windows メッセージで検索・フィルターの件数と終了コードを確認する。
ビューア x64/x86 × ログ生成側 x64/x86 の全 4 組合せで以下が成功。

- 全 6 件の読込みと警告だけの除外・再表示。
- 通常検索、該当なし、検索クリア、反転検索。
- 大小文字を区別する検索とワイルドカード検索。
- 日本語を含む本文検索（ヘッダー限定では 0 件、本文を含めると 6 件）。
- 終了コード 0、Failures=0。

ログ: `audit/viewer-search-{x64,x86}-{x64,x86}.txt`。
診断プローブのビルド手順: `tests/build_vxlview_probes.ps1`。
ビューア: `audit/build-vxlview/{x64,x86}/VxlView.exe`。
VM: `C:\VxKexProbe\NextParity\VxlView.exe` と同ディレクトリの
`x86\VxlView.exe`。正式 Installer バイナリはまだ変更していない。

未完了: テキスト書出し、詳細表示／移動、破損ログの扱い、ファイル関連付け、
インストール／アンインストール連携と正式配布。
書出しコードでは非同期書込み完了前のバッファ解放や書込みエラー確認を
追加調査する必要がある。今回の読込み・検索成功で書出しまで検証済みとはしない。
設定 UI・TLS・設定保持／復元の残項目も継続する。

### 2026-10-01: 書出しの同期化と日付表示の Vista 対応

`ExportLogToFile` に実際の書出し処理を分離し、GUI の書出しスレッドからも
この同じ処理を呼ぶようにした。FILE_SYNCHRONOUS_IO_NONALERT で開き、本文
バッファを解放する前に書込みを完了。変換失敗、短い書込み、保存完了の失敗を
返し、失敗後に次のレコードへ進んで成功扱いにする処理を廃止した。
出力には UTF-16 LE BOM を追加。進捗の最終値が 100% を超える計算も修正。
NEXT と同じく表示フィルターにかかわらず全レコードを出力する。

GUI 側は保存先文字列をスレッド専用に複製し、スレッド開始前にウィンドウを
無効化。開始失敗時は文字列を解放し、操作可能な状態へ戻してエラーを表示する。

出力の直接確認で日時文字列の未初期化データを発見。
DATE_AUTOLAYOUT は Windows 7 以降のフラグであり、Vista では使わない。
日付・時刻の変換戻り値を確認し、失敗時は数値表記へフォールバックする。
GetTimeFormatEx に渡す文字数が別バッファのサイズになっていた箇所も修正。
仕様: https://learn.microsoft.com/en-us/windows/win32/api/datetimeapi/nf-datetimeapi-getdateformatex

`tests/vxlview_export_probe.c` はビューアと同じバックエンドオブジェクトを使用。
Server 2008 VM で x64/x86 の双方について、全 6 レコードの見出し・日本語本文・
日付・BOM を確認し、Failures=0。
排他ロック中の保存先は STATUS_SHARING_VIOLATION (c0000043)、不存在の保存先
ディレクトリは c000003a を返した。出力は両ビット幅とも 1352 バイトで同一。
SHA256: 3DC63798BCB48286CEA3FC93FF999D9C322849366A37AACD67D0C1DA3446C792。
ログ: `audit/viewer-export-result-{x64,x86}.txt`。
出力: `audit/viewer-export-{x64,x86}.txt`。
ビルド: `tests/build_vxlview_probes.ps1`（先に VxlView 本体をビルド）。

更新したビューアを VM の診断用ディレクトリへ再配置し、x64/x86 両方で
読込み・各検索・重要度フィルター・終了コードの回帰検証も Failures=0。
ログ: `audit/viewer-search-after-export-{x64,x86}.txt`。

この試験は実際の書出しバックエンドの検証。
GUI の保存先選択・完了ダイアログ・スレッド生成失敗の経路は未検証。
詳細表示／移動、破損ログ、ファイル関連付け、正式インストールへの組込み、
他の優先度「高」項目は未完了。コミットや配布バイナリ更新はまだ行っていない。

### 2026-10-01: .vxl 関連付けとインストーラー経路

`VxlView/association.c` に Vista のレジストリトランザクションを使う登録・解除を
追加。`VxlView.exe /register` と `/unregister` は GUI を表示せず終了コードを返す。
通常のビューアは asInvoker のまま。登録・解除は管理者で実行し、
HKLM\Software\Classes の 64bit ビューを使用する。
コマンド解析は CommandLineToArgvW で引用符付きの引数にも対応した。
当初の生文字列比較では VM の登録コマンドが GUI 経路に入り、終了しなかった。
このプロセスだけを終了して解析を修正後、登録コマンドは正常終了した。

固有 ProgID `VxKexVista.Log` に起動コマンド・アイコン・所有パスを記録。
`.vxl` の既定値が空の場合に既定の関連付けを設定する。
他アプリの既定値がある場合は保持し、OpenWithProgids に追加する。
登録・解除の途中失敗はロールバック。解除は所有する値と空キーだけを削除し、
他アプリが起動コマンドを変更した場合は拒否する。
Explorer に関連付け変更を通知する。
使用 API は Vista/Server 2008 対応:
https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regcreatekeytransactedw

`tests/vxl_association_probe.c` は HKCU 内の専用 fixture だけを操作し、
実際の Classes やユーザーの関連付けには変更を加えない。
Server 2008 で x64/x86 とも登録、繰り返し登録、解除、繰り返し解除、既存の
他アプリの既定値と OpenWith 値の保持、変更済み起動コマンドの保護、
不正な拡張子既定値による途中失敗のロールバックに成功。fixture は削除した。
ログ: `audit/association-result-{x64,x86}.txt`、どちらも Failures=0。

VM の `C:\VxKex\VxlView.exe` へ x64 ビューアを新規配置し、実際の関連付けを
登録。`tests/vxlview_gui_probe.c` に ShellExecuteEx による関連付け起動の試験を
追加。起動された画像が指定の VxlView.exe と一致することを確認してから、
ログ読込み・各検索・重要度フィルター・終了を検証し、Failures=0。
ログ: `audit/viewer-association-launch.txt`。
その後 `/unregister` と `/register` の両方が正常終了し、最終状態は登録済み。

Installer フォルダに `VxlView.exe` と `Kex32/VxlView.exe` を追加。
`install.bat` のインストール・更新は native x64 ビューアの存在確認とコピー、
登録を行う。アンインストールはビュ－アが存在する場合だけ所有する関連付けを
解除し、解除失敗時はファイルを削除せず停止する。
旧パッケージにビューアがない場合のアンインストール経路は維持。
KexCfg のコピー失敗判定を別コピーの後へ移動しないよう確認した。
ビルドと Installer のビューアの SHA256 は両ビット幅とも一致。

VM にはインストーラーを `C:\VxKexProbe\NextParity\install-next-parity.bat` として
診断用配置。インストーラー全体のインストール・更新・アンインストール試験は
まだ実施していない。正式配置したビューアと登録コマンドの実行検証だけをもって
インストール全体の検証済みとはしない。
詳細表示／移動、破損ログ、GUI 書出し、設定保持／復元、TLS と設定 UI の残項目を
継続する。コミットや Releases 更新は行っていない。

### 2026-10-01: 設定保持の前提となる IFEO 削除処理の修正

NEXT の preserve.c と KexSetup の保持／復元経路を調査。
互換設定を別キーへ保存し、IFEO から無効化し、再インストールで戻す構成。
Vista では両 IFEO ビュー、古い basename のみの設定、消失した画像、
VistaRun 所有の Debugger を考慮する必要がある。保存・復元そのものと選択 UI は
この時点では未移植。安全な無効化に必要な既存削除 API の問題を先に修正した。

- KexDll.dll の部分一致削除を完全な空白／タブ区切りトークンの削除へ変更。
  prefixkexdll.dll、kexdll.dll.backup、他のディレクトリの同名 DLL は変更しない。
  大文字／小文字は区別せず、重複した KexDll トークンもすべて削除する。
  他の DLL 名の大文字／小文字と区切り文字は保持する。
- VerifierDlls がない場合も KEX_ 値削除後の空キー整理を行う。
  以前はこの経路で空キーが残り、次の試験の既存キー検出で発覚した。
- 値がなくても外部の子キーがある場合は IFEO キーを削除しない。
  子キーをまとめて削除する RegDeleteTree をこの整理処理から除去。
- FLG_SHOW_LDR_SNAPS は当 helper の所有と断定できないため保持。
  VxKex が唯一の verifier の場合は FLG_APPLICATION_VERIFIER だけを解除する。
- RegReOpenKey が閉じた元ハンドルを再度閉じる処理を除去。
  再オープン失敗時はまだ有効なハンドルを閉じる。

`tests/verifier_tokens_probe.c` で 14 種の入力と繰り返し削除を検証。
Server 2008 の x64/x86 で Failures=0。
ビルド: `tests/build_verifier_tokens_probe.ps1`。
ログ: `audit/verifier-tokens-{x64,x86}.txt`。

既存の `tests/config_missing_image_probe.c` に実際の IFEO API 試験を追加。
外部 verifier、共有フラグ、loader snaps、外部子キーの値を保持できることを
確認した。消失画像・同名の別ビュー・曖昧なパスの拒否の回帰試験も成功。
両アーキテクチャで Failures=0、最後に試験キーと画像を整理／復元。
ログ: `audit/config-verifier-result-{x64,x86}.txt`。
最初の x86 試験は残った空 fixture キーを検出して変更せず停止した。
新しい削除処理でその試験キーを整理後、両方の試験を正常完了した。

最新の KexCfg ビルド:
`audit/build-preservation-foundation/KexCfg/KexCfg.exe` と
`audit/build-preservation-foundation/KexCfg-x86/KexCfg.exe`。
VM 診断用配置: `C:\VxKexProbe\NextParity\KexCfg-preservation.exe` と
`x86\KexCfg-preservation.exe`。正式な KexCfg／Installer は置き換えていない。

引き続き必要: 両ビューと basename 設定を保存できる版付きデータ形式、復元時の
外部 Debugger との競合処理、途中失敗時の保存データ保持、アンインストール時の
保持選択、再インストールでの復元と全体試験。現在のバッチによる LogDir の
上書きと VXsoft 親キー削除も改善が必要。優先度「高」の移植全体は未完了。

### 2026-10-01: キー単位の設定保存・復元エンジン

`KxCfgHlp/preserve.c` にトランザクション内の保存／無効化／復元処理を追加。
呼出側が同一トランザクションの Source / Backup ハンドルを渡し、失敗時は
必ずロールバックする内部 API。版付きの保存形式は SavedVersion=1。

- KEX_ 値は型と生データを保持。未知のバイナリ設定も失わない。
- KexDll の verifier トークン、実際の有効状態、VerifierFlags、所有する
  VistaRun Debugger を保存し、外部 verifier / Debugger / 値を保持して無効化。
- 復元時は版・必須値・値の型と長さ・文字列終端を検査する。
  SavedDebugger は保存した KEX_VistaDebugger と一致する場合だけ復元する。
- 新しい設定、外部 Debugger、変更された共有 VerifierFlags が競合する場合は
  上書きせず失敗する。既存 Backup への保存も拒否する。
- 復元後も Backup は残す。全件成功後の消費は上位処理の責任とする。
  元から無効だった verifier を復元時に有効化しない。

`tests/preserve_configuration_probe.c` を Server 2008 の x64 / x86 で実行し、
両方 Failures=0。保存の取消し／コミット、競合、欠落・未対応版・破損データ、
復元途中のロールバック、未知の設定の完全な復元、無効状態の保持を確認。
試験は専用 HKCU キー内で実行し、最後にキーを削除する。
実際の全 IFEO プロファイルの保存・復元を検証したという意味ではない。

最初の破損文字列試験は RegSetValueEx が終端を補ったため、実際には正常な
文字列になっていた。NtSetValueKey で終端なしの 2 バイト値を作成し、
RegQueryValueEx による保存長の確認と ERROR_INVALID_DATA の確認を追加した。

ビルド: `tests/build_preserve_configuration_probe.ps1`。
最新ログ: `audit/preserve-result-x64.txt` / `audit/preserve-result-x86.txt`。
VM 診断用 EXE: `C:\VxKexProbe\NextParity\preserve-configuration-probe.exe` と
`C:\VxKexProbe\NextParity\x86\preserve-configuration-probe.exe`。
正式な KexCfg とインストーラーへの保存／復元コマンドの組込みはまだ行っていない。

次に必要な上位処理: 両 IFEO ビューと basename / フィルターキーの識別を維持する
全件保存・復元、全件を一つのトランザクションで処理する経路、保存領域の所有と
消費、アンインストール時の保持選択、再インストールでの復元、全体試験。
TLS、設定 UI、VxlView の残項目も継続対象。コミット／Releases 更新は未実施。

### 2026-10-01: ビュー単位／全件保存・復元と KexCfg CLI

`KxCfgpPreserveIfeoView` / `KxCfgpRestoreIfeoView` を追加。
IFEO の basename キーと UseFilter の子キーを列挙し、番号付きレコードに
相対キー名・フィルターの適用先・UseFilter・保存済み設定を記録する。
実行ファイルの存在やビット幅検出に依存しない。
ビュー番号を保存し、反対のビューへの復元を拒否する。
32bit の呼出元でも、すべての子キーを明示したビューで開く。
内部の `{VxKexPropagationVirtualKey}` は一般のアプリ設定として保存しない。

`KxCfgPreserveAllConfigurations` / `KxCfgRestoreAllConfigurations` を追加。
Vista x64 では両ビュー、32bit OS と共有 IFEO の OS では一つのビューを扱う。
呼出側の一つのトランザクションで全件を処理し、成功後にコミットする構成。
復元は全件成功後に保存領域を消費する。競合や破損時には呼出側が取り消し、
保存データを残す。保存先は HKLM 64bit 側の
`Software\VXsoft\VxKexVistaPreserved`。
VxKex 本体の設定キーを削除しても保持できる別キーだが、既存バッチの
VXsoft 親キー削除はまだ修正していないため、このまま組み込んではならない。
保存領域は保護 DACL と子キーへの継承により Administrators / SYSTEM のみ許可。

KexCfg に昇格済み CLI を追加:

- `/PRESERVE`: 全件を保存して IFEO の VxKex 設定を無効化し、コミットする。
- `/RESTORE`: 全件を復元して保存領域を消費し、コミットする。
  保存領域がない初回インストールでは成功する。
- `/CHECKPRESERVE`: 保存・復元を同一トランザクション内で実行し、必ず取り消す。
  既存の保存領域がある場合は上書きせず停止する。

この段階で正式な Installer / KexCfg は置き換えていない。
新しい KexCfg は `audit/build-preservation-engine/KexCfg/KexCfg.exe` と
`KexCfg-x86/KexCfg.exe`、VM では
`C:\VxKexProbe\NextParity\KexCfg-preservation.exe` と
`x86\KexCfg-preservation.exe` に配置。両方で `/CHECKPRESERVE` 終了成功。
実際の全件保存をコミットする `/PRESERVE` は VM で実行していない。

検証:

- `tests/preserve_view_probe.c`: 専用 HKCU の複数プロファイルを二つのビューに
  見立てて保存・無効化・復元をコミット。二つ目のビューの破損／設定競合で、
  先に処理したビューも取り消されることを確認。
  basename のみの設定、消失したフィルターの再作成、外部設定の保持、内部キーの
  除外、レコード数の不一致、不正な相対キー名、未対応の旧ローダーの拒否を検証。
  実際の IFEO に専用の診断キーも作り、明示的な両ビューを x86/x64 呼出元で
  検証。後者はトランザクション取消しにより、両ビューとも診断キーが残らない。
  x86/x64 とも Failures=0。ログ `audit/preserve-view-{x64,x86}.txt`。
- `tests/preserve_all_probe.c`: VM の既存全プロファイルを保存・復元して取り消す。
  64bit 側 91 件、32bit 側 18 件。実際の保存ルート・ビューキー・設定キーの
  DACL を検証し、両 IFEO の全値・全子キーのフィンガープリントが試験前後で
  一致することを確認。保存キーも外部には残らない。
  x86/x64 とも Failures=0。ログ `audit/preserve-all-{x64,x86}.txt`。
- キー単位の診断の回帰試験も x86/x64 とも Failures=0。

Vista の挙動として、一つのトランザクション内で作成後に削除したキーは、
同じトランザクション内やコミット後に空キーとして開ける場合があることを
専用 HKCU のキーで確認した。全件の取消し試験ではデータと子キーが消費済みで
あることを確認した後、必ず取消す。既にコミットされたキーを別トランザクションで
削除する経路では、コミット後に ERROR_FILE_NOT_FOUND となることを両ビット幅で
確認している。実際の保存コミット→再インストール→復元コミットの全体試験は残る。

制限: 旧 VxKexLdr.exe を Debugger の実行対象とする設定は、所有を安全に確認する
移行経路が未実装のため ERROR_NOT_SUPPORTED で全操作を取り消す。
黙って除外して完全保存と扱わない。内部伝播キーの無効化／再構築、全体設定や
カスタム LogDir の保持、旧形式の移行、アンインストール時の保持選択、
再インストール経路への組込み、正式パッケージとライフサイクル試験を継続する。
優先度「高」の移植全体はまだ完了していない。コミット／Releases 更新は未実施。

### 2026-10-01: アンインストール準備 API とバッチの設定保持経路

`KxCfgPrepareUninstall(KeepSettings, Transaction)` を追加。
保持する場合は両ビューのアプリ設定を保存して無効化し、内部伝播テンプレートの
KexDll provider も無効化する。削除する場合は保存せず無効化し、以前の版付き
保存領域があれば明示的に消費する。いずれも外部 verifier / Debugger / 値と
無関係な GlobalFlag のビットを保持する。一つのトランザクションで処理。
内部テンプレート自体のキーや外部子キーを丸ごと削除しない。

KexCfg に `/UNINSTALLKEEP` / `/REMOVEALL` と、必ず取消す診断モード
`/CHECKUNINSTALLKEEP` / `/CHECKREMOVEALL` を追加。
Server 2008 の両ビット幅で診断 CLI 終了成功。
`preserve_all_probe` に実際の両 IFEO の保持／削除準備と内部テンプレートの
無効化、取消し後の全体一致を追加し、x86/x64 とも Failures=0。
`preserve_view_probe` に専用領域の保存なし削除と外部値／フラグ保持を追加し、
両方 Failures=0。キー単位の回帰プローブも両方 Failures=0。

`Installer/install.bat` に以下を組込み:

- アンインストール時に保持／削除／取消しを選択。
  更新パッケージの KexCfg で取消し試験を行い、成功後に IFEO 無効化をコミットして
  から DLL を撤去する。古い CLI や不足したパッケージでは停止する。
- インストールと更新の DLL／基本設定配置後に `/RESTORE` を実行。
  失敗時には保存データを消費せず停止する。
- LogDir が既存なら上書きしない。保持アンインストールでは全体設定と
  per-user preferences を残し、InstalledVersion / KexDir の設置情報を除去する。
  削除モードでは VxKex / VxKexLdr の個別キーだけを除去する。
  VXsoft 親キーは削除しない。
- System32 の `Kx*.dll` 一括削除を、既知のライブラリ名の削除へ変更。
  ロックされた DLL や撤去できないディレクトリがあれば失敗として停止する。

自己削除の問題も検証。最終ブロックを先に読み込むだけでは、実行中のバッチを
削除した後の呼出元復帰でエラーが生じた。新しい
`Installer/Remove-VxKex-Files.cmd` を保護された外部キャッシュ
`%ProgramData%\VxKexVistaSetup` にコピーし、CALL を使わずバッチ実行コンテキストを
引き継ぐ方式に変更した。この補助バッチはインストール先に依存せず最後まで
実行できる。キャッシュは stateless な撤去コードと Owner.txt を保持して残る。
キャッシュの所有者を Administrators とし、Administrators / SYSTEM のみ許可する。
再利用時は明示的な旧 ACE を消去し、子ファイルも再作成して所有者を設定する。

バッチの専用領域検証:

- `tests/build_installer_registry_fixture.ps1` が実際のバッチから LogDir サブルーチンと
  設定削除ブロックを抽出し、専用 HKCU にマッピング。
  初期 LogDir、カスタム LogDir の保持、保持／削除時の全体設定・ユーザー設定、
  他製品のキーと保存領域を検証。ログ `audit/installer-registry-result.txt`、Failures=0。
- `tests/build_installer_delete_fixture.ps1` が実際の撤去補助バッチを専用フォルダーに
  マッピング。実行中の installed\install.bat を消して正常終了を呼出元へ返すこと、
  指定 DLL の撤去、無関係な KxUnrelated.dll の保持を確認。
  最初の旧方式の失敗領域は所有マーカーと内容を確認して整理。
  修正後ログ `audit/installer-delete-result.txt`、Failures=0、専用領域を最後に削除。
- `tests/build_installer_stage_fixture.ps1` が実際のステージング処理を専用フォルダーで
  実行。初回作成と、意図的に Users の明示的 ACE を追加した既存キャッシュからの
  再作成の両方を確認。フォルダー／マーカー／補助バッチは所有者 Administrators、
  許可 ACE は Administrators / SYSTEM の 2 件だけ。
  ログ `audit/installer-stage-result.txt`、Failures=0、専用領域を最後に削除。

ホストの `Installer/KexCfg.exe` を検証済み x64 ビルドへ更新。
SHA256: `4ADA7F198FF54E6371391B1FD1044495A7BF67CD4E090CE10197682936A84EAE`。
VM では新しい CLI とバッチ／補助バッチを `C:\VxKexProbe\NextParity` の診断用に配置。
正式な VM インストール先の KexCfg / install.bat は置き換えていない。
実際の System32 / SysWOW64 のファイルを撤去するアンインストールは実行していない。

残る重要事項: パッケージ全体の配備・更新・アンインストール・再インストールの
通し試験、設定を保持してコミットした後の別トランザクションでの復元、ファイル
配備／撤去の途中失敗時の自動ロールバック。現在はロックやコピー失敗を検出して
停止する経路を改善した段階で、全ファイルの自動復旧までは実装していない。
旧ローダー形式の移行、別ユーザーとしての UAC とユーザー設定の扱い、TLS と
設定 UI・VxlView の残項目も継続する。優先度「高」の全体完了とはしない。

### 2026-10-01: ファイル配備・撤去のトランザクション基盤

`KxCfgHlp/setupfile.c` にファイル／ディレクトリーのコピー・削除を追加。
ネイティブ TxF を使い、レジストリ処理と同じ KTM トランザクションを受け取る。
自身は確定／取消を行わず、通常のファイル操作へのフォールバックも行わない。
短い不正パス入力の長さ検査も追加した。

`tests/setup_transaction_probe.c` とビルドスクリプトを追加し、Server 2008 の
専用フォルダー・専用 HKCU キーで32bit／64bitを検証した。
両方 `Failures=0`。ログは `audit/setup-transaction-x64.txt` と
`audit/setup-transaction-x86.txt`（各69行）。

- 確定前は外部に元の内容、トランザクション内に更新後の内容が見える。
- ファイルとレジストリを同時に確定／取消できる。
- 日本語名・ネストしたディレクトリーをコピーし、無関係な既存ファイルと
  コピー元を保持する。
- 第2ファイルのロックによるコピー失敗（32）後、先行したファイル更新と
  レジストリ変更を取り消し、元の内容が復旧する。
- 先行した削除の後、ディレクトリー撤去がロックで失敗（32）しても、取消後に
  削除対象が復旧する。
- ハードリンク先の上書きを拒否。ジャンクション祖先・列挙中のジャンクション
  を拒否し、部分コピー／削除を取り消して両方のツリーを保持する。
- ボリュームルート、相対パス、短い不正パス、包含関係のあるコピー先を拒否する。
- 最後に専用ファイル領域とレジストリキーを削除した。

これはファイル処理基盤の検証であり、`Installer/install.bat` はまだこの API を
利用していない。正式な配備／撤去の全体ロールバックは未実装。
次にキャッシュ側のセットアップ処理へ組み込み、IFEO・関連付け・DLL 配備を含む
全体の確定／復旧経路と通しのライフサイクルを検証する。
本番 VM の C:\VxKex・System32 の配備ファイルは今回変更していない。

### 2026-10-01: ログ関連付けをセットアップと共有するトランザクションへ移行

VxlView の関連付けロジックを `KxCfgHlp/logassoc.c` の
`KxCfgpUpdateLogAssociation` へ移した。呼出元が KTM トランザクションを渡し、
確定／取消を所有する。VxlView の既存コマンドはこの共通処理を呼ぶラッパーに
変更した。確定失敗時にも明示的に取消を要求する。
両ビルドスクリプトとプローブのリンクに構成ライブラリを追加。
KxCfgHlp の Visual Studio プロジェクトにも新規3ソースを登録した
（Visual Studio 経由の全体ビルドは未検証、スクリプトで両アーキテクチャをビルド）。

Server 2008 の専用領域で確認:

- `setup_transaction_probe` に関連付けを加え、ファイル・レジストリ・関連付けを
  同時に確定／取消。第2ファイルのコピーがロックで失敗した場合、先行した
  関連付け削除も取り消され、元のコマンドが復旧することを確認。
- 最後の撤去では関連付けとファイルを同時に確定して削除した。
- x64／x86とも `audit/setup-transaction-*.txt` は77行、`Failures=0`。
- VxlView の既存関連付けプローブも共通実装へリンクして再実行。
  外部アプリの既定関連付けの保持、変更されたコマンドの削除拒否、不正値での
  取消、繰り返し登録／解除を含め、両方 `Failures=0`。
  ログは `audit/association-result-x64.txt` と `audit/association-result-x86.txt`。

`Installer/install.bat` の正式な配備・撤去経路への組み込みは引き続き未完了。
シェル拡張登録・全体設定・IFEO・ファイルをまとめるセットアップ実行部と、
インストール先の外に配置した実行部による通し検証が次の工程。
今回も本番 VM と Installer の配布バイナリは置き換えていない。

### 2026-10-01: シェル拡張登録を共有トランザクションへ追加

`KxCfgHlp/shlsetup.c` に `KxCfgpUpdateShellExtension` を追加し、CLSID・DLL パス・
ThreadingModel・Approved・EXE／LNK のプロパティーハンドラーを同じ KTM
トランザクションで登録／解除する。従来の Vista バッチが登録していた
`VxKex` ハンドラー名と CLSID を使う。NEXT perform.c の登録構造も参照した。

既存 DLL パスの不一致を拒否し、登録時には変更された値を上書きしない。
解除は一致する値だけを消し、空になった所有キーだけを削除する。
共有 Approved キー、無関係な値と子キーを保持する。通常の I/O への切替なし。
ログ関連付け・シェル登録の子キー操作にも64bitレジストリビューを明示した。
ビルドスクリプト・VSプロジェクトへ新規ソースを登録。

Server 2008 専用領域の setup_transaction_probe を拡張し、両アーキテクチャで
各101行 `Failures=0` を確認。

- ファイル・設定・ログ関連付け・シェル拡張登録を同時に確定／取消。
- コピー失敗後に先行したシェル登録解除を取り消し、元の DLL パスが復旧。
- ThreadingModel の競合で登録が失敗した場合も先行したファイル更新を復旧。
- 異なる DLL パスを指定した解除を拒否。
- 解除の確定後も無関係なクラス値・子キーを保持。
- 最新の共通ログ関連付け実装に再リンクした既存プローブもx64／x86とも
  Failures=0（audit/association-result-*.txt）。

正式な Installer/install.bat はまだこの処理を呼ばない。全体設定、IFEO と
ファイル配備をまとめるセットアップ実行部への組み込み、全ライフサイクルの
通し検証は未完了。今回のVM変更は専用診断領域のみで、本番配備は変更なし。

### 2026-10-01: パッケージのファイル配備・撤去を統合

`KxCfgpDeploySetupFiles` / `KxCfgpRemoveSetupFiles` を setupfile.c に追加。
呼出元が認可した Package・Target・NativeSystem・WowSystem の各ルートを受け取る。
ルートの相互包含、ジャンクション、システムルートの欠如／非ディレクトリーを
拒否し、すべて既存の TxF 基盤で実行する。Target だけは未作成を許可する。
本番ではネイティブセットアッププロセスから物理的な System32／SysWOW64 パスを
渡す前提。x86プローブの専用パス検証は、本番の WOW64 リダイレクト検証ではない。

配備前に13種のライブラリを両アーキテクチャで確認し、KexShlEx・VistaRun・KexCfg・
VxlView・install.bat・撤去補助バッチ・Kex64/dwrw10.dll の存在も確認する。
オプションの KxSChanl が片方だけの場合は拒否する。パッケージのツリーを Target に
マージし、定義済みのライブラリだけを両システムルートへ配備する。
撤去ではシステムルートの既知ファイルだけを削除し、Target のツリーを撤去する。
確定／取消は外側のセットアップ処理が所有し、通常 I/O への切替はしない。

新しい tests/setup_package_probe.c（専用のダミーファイル群）でファイル処理を検証:

- 必須x86ファイルが欠けるパッケージを配備前に拒否し、配備先の未変更を確認。
- TLS DLL の片方だけの構成を拒否。
- Kex64 と VistaPty のサブツリー、13種の両アーキテクチャ、TLSペアを確定して配備。
- 配備終盤の WOW 側 KxUser.dll をロックし、エラー32後に取消。
  先行した Target・Native・Wow の全13ファイルが元の内容であることを照合。
- 撤去終盤の同じロックによるエラー32後も、先行した削除を同様に復旧。
- 撤去の確定、繰返し撤去、両システムルートに同じパスを渡した場合の拒否を確認。
- 無関係な KxUnrelated.dll を両システムルートで保持。
- 専用領域を最後に清掃。

Server 2008 x64／x86とも audit/setup-package-*.txt は252行、Failures=0。
ここで使ったDLL名のファイル内容は診断用文字列であり、PE妥当性や実DLLの読込み・
実システムディレクトリーへの配備の検証ではない。
本番セットアップ実行部のパッケージ検証と、全体設定・IFEO・関連付けを含む
通しの組み込み試験は未完了。Installer/install.bat の正式経路はまだ置換していない。
本番VMと配布バイナリは変更していない。

### 2026-10-01: 実バイナリの事前検証と配備

Microsoft PE Format 仕様を参照して `KxCfgpValidateSetupBinary` を追加。
https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
ファイルをロード／実行せず、トランザクション内で DOS／PE署名、Machine、
DLL／EXE区別、Optional Header のサイズとMagic、セクション数（1〜96）、
ヘッダーとセクションのファイル／イメージ境界、エントリーポイント境界を確認する。
範囲の足算は64bitで計算し、負の e_lfanew も拒否する。
これはPEレイアウトの検査であり、署名・ハッシュ・インポート解決・実行時ABIの
保証ではない。任意のあらゆるPEを受入れる一般ローダーでもない。

`KxCfgpValidateSetupPackage` は主要DLL、KexShlEx、dwrw10、VistaRun、KexCfg、
VxlView の各スロットで期待するアーキテクチャを検証する。
オプションのTLSペアも検証する。実運用用入口 `KxCfgpDeploySetupPackage` は
同じトランザクションでパッケージ検証→ファイル配備を呼ぶ。
従来の `KxCfgpDeploySetupFiles` は低レベルのファイル処理として残す。

現在の Installer 全52ファイル（約6.3MB）を VM の専用
C:\VxKexProbe\NextParity\RealPackage へコピーし、実バイナリで検証した。
転送マニフェストは audit/real-package-copy-manifest.json。
RealPackage は後続試験の入力として保持し、本番 C:\VxKex とは独立している。

新しい tests/setup_binary_probe.c をServer 2008のx64／x86で実行。
両方 audit/setup-binary-*.txt は78行、Failures=0。

- 現在の配布パッケージの主要バイナリを受入れ。
- 32bit／64bitの取り違え、DLL／EXEの取り違えを193で拒否。
- 切詰め、巨大／負の e_lfanew、PE署名破損、Optional Header Magic の破損、
  セクション生データ範囲オーバーフローの6検体を193で拒否して取消。
- 実パッケージを専用Installed／Native／Wowへ配備して確定。
  KexDllの両アーキテクチャとWinPTYを元ファイルと全バイト比較し一致を確認。
- コピーしたパッケージのnative DLLをx86へ置換した場合、事前検証と実運用用入口の
  両方で拒否。新規配備先を作らず、既存配備先も元のままであることを確認。
- 専用配備を撤去・確定し、検証領域を清掃。

正式なセットアップ実行部で全体設定・IFEO・関連付けとまとめる工程、および本番の
System32／SysWOW64を対象とするライフサイクル試験は未完了。
今回もInstaller配布バイナリ／本番VMの配備は変更していない。

### 2026-10-01: 全体設定と伝播テンプレートを共有トランザクションへ追加

`KxCfgHlp/setupreg.c` に以下を追加し、ビルドスクリプト／VSプロジェクトへ登録。

- KxCfgpConfigureSetupSettings: 呼出元指定のMachineSoftware／UserSoftwareルートに
  InstalledVersion・KexDirを登録。既存LogDirは型・終端を確認して保持し、未設定時
  だけ既定値を設定。異なる既存KexDirは拒否。保持撤去ではインストールマーカー
  だけを削除。全削除ではVxKex／VxKexLdr製品キーだけを削除し、VXsoft親は保持。
  別ユーザーのハイブを推測しない。UserSoftware=NULLの場合はユーザー設定に触れない。
- KxCfgpConfigurePropagationTemplate: 渡されたIFEOルート・明示ビュー内で
  KexDllトークンを統合し、APPVERIFIERビットを設定。外国のプロバイダー・検査フラグを
  保持。KexのみでVerifierFlagsが0の場合は0x80000000を設定してVista標準検査を避ける。
  撤去は既存の所有設定削除プリミティブを利用し、外国値・子キーを保持。

テストで再登録時の区切り空白の増加を発見し、既存末尾の区切りを使うよう修正。
空白のみの一覧もKexのみとして処理する。長すぎるDWORD値の読取りエラーは
ERROR_INVALID_DATAに統一した。

新しい tests/setup_registry_probe.c をServer 2008 x64／x86で実行し、
両方 audit/setup-registry-*.txt は101行、Failures=0。

- 新規設定の取消／確定、既定LogDir、カスタムLogDir・ユーザー設定の保持。
- 外国プロバイダーとフラグ、重複Kexトークン、繰り返し更新の区切り保持。
- 不正LogDirで失敗した場合、先に配備した実DLLへの変更が元のファイル内容へ復旧。
- 不正GlobalFlagで失敗した場合、先に書いたInstalledVersionが取消で復旧。
- 空白のみの一覧の安全なVerifierFlags、異なるKexDirの拒否。
- 保持撤去・再インストール、全削除撤去、外国製品・外国テンプレート子キーの保持。
- 専用HKCUキーと診断ファイルを最後に清掃。

本試験のIfeo64／Ifeo32は独立した専用HKCUルートであり、本番HKLMの実IFEO二ビュー
への登録・アプリ起動の証明ではない。既存の実IFEO保存試験とは別の構成API試験。
正式なセットアップ実行部へこれらのAPIと保存／復元・関連付け・ファイル処理を
まとめる作業、実システムでの通し検証は引き続き未完了。
今回も本番VM／Installerバイナリは変更していない。

### 2026-10-01: セットアップの統合ステージと確定済み保存からの復元

`KxCfgHlp/setup.c` / `KXCFG_SETUP_CONTEXT` / `KxCfgpStageSetup` を追加。
呼出元が認可したファイル・レジストリのルートを受け取り、単一トランザクションで
以下を順にステージする。自身は確定／取消しない。現段階では64bit OS用のコア。

- インストール: 実パッケージ検証・配備、Logsフォルダー、全体設定、保存済み
  全ビュー設定の復元、伝播テンプレート、シェル拡張、ログ関連付け。
- 保持撤去: 全ビューの通常プロファイルを保護ストアへ保存して無効化し、
  テンプレート・関連付け・登録を解除、全体設定はマーカーだけ削除、ファイルを撤去。
- 全削除撤去: 所有プロファイルと以前の保存ストアを削除し、同じ登録／ファイル撤去を
  ステージ。外国のプロバイダー・共有フラグ・共有システムファイルは保持。

preserve.c の保存ストア処理を `KxCfgpProcessConfigurationRoots` に共通化。
従来の実HKLMラッパーと統合コアが同じ形式・保護ACL・検証を使う。
IFEOルートを明示ビュー・同一トランザクションで再オープンする。
統合試験で従来の復元にも空白区切りの増加を発見し、末尾区切りがある場合は
追加しないよう修正。単体試験の期待値も「既存の区切りを保持」に修正。

新しい tests/setup_stage_probe.c をServer 2008 x64／x86で実行。
両方 audit/setup-stage-*.txt は103行、Failures=0。
実Installer入力を専用Installed／Native／Wowへ配備し、専用HKCUルートで:

- 新規インストールの確認（取消）と確定、両伝播テンプレート、関連付け、実ファイル。
- 撤去終盤でWOW DLLをロックし、保存済み設定・マーカー・関連付け・ファイルまで
  全体を取消。元の有効なプロファイルと無効なWOWプロファイルを保持し、失敗時に
  保存ストアが残らないことを確認。
- 保持撤去を確定してストアを残し、次の別トランザクションで再インストール。
- 再インストール時にDebugger競合を導入して183で拒否。ストアと外国Debuggerを
  保持し、先に配備したファイル・マーカーの変更を取消。
- 競合除去後の再インストールで有効／無効状態、WinVerSpoof、管理Debugger、
  外国プロバイダー・フラグ、カスタムLogDirとユーザー設定を復元。
  前のトランザクションで確定したストアが消費されてなくなることを確認。
- 全削除撤去の確定、外国のプロバイダーとKxUnrelated.dllの保持、専用領域の清掃。

保存の共通化・区切り修正に伴い既存試験を最新版へ再リンクして再実行。
両アーキテクチャとも preserve-result（74行）、preserve-view（178行）、
preserve-all（45行）がFailures=0。実IFEO全体の保存／復元と撤去準備は必ず取消し、
処理前後の全体状態が一致することを再確認。

統合コアのライフサイクル試験は専用ルート内の証拠であり、本番のインストール先・
System32／SysWOW64での通し動作を証明するものではない。
キャッシュ上で動く正式なセットアップ実行ファイル、バッチの呼出し置換、
本番相当のVMでの通し検証とUAC別ユーザー処理は引き続き未完了。
Installerの配布バイナリと本番VMのインストールは今回変更していない。

### 2026-10-01: ネイティブセットアップ実行部と実環境の取消観測

`VistaSetup/main.c`、リソース、asInvokerマニフェスト、ビルドスクリプトを追加。
NT 6.0 x64・昇格済みのプロセスで統合ステージを呼び出し、成功時だけ確定する。
確認モードおよび失敗時は取消し、取消自体のエラーも別途表示する。
VxKex DLLを依存先に持たず、Vista標準のDLLで動作する。
実行ファイルはC:\VxKexの外から実行し、作業ディレクトリもWindowsへ移す。
インストール入力のKexCfg.exeと自身のバージョン一致を要求する。

UAC前に使う `--current-user-sid` と、操作に明示する `--user-sid` を実装。
明示されたローカル／ドメインユーザーSIDのロード済みHKEY_USERSを開き、
昇格後のユーザーから元ユーザーを推測しない。全削除にはSIDが必須。
実際の別ユーザー資格情報によるUAC試験と、そのSIDを引き渡すバッチは未実装。

`KXCFG_SETUP_CONTEXT.StageName` を追加し、失敗時の最後のステージを表示する。
構造体変更に合わせてx64／x86ライブラリと統合試験を再ビルド。
Server 2008でsetup-stage-x64／x86を再実行し、双方103行、Failures=0。

実環境の確認コマンドでは、インストールはdeploy-packageで32、
保持撤去と全削除撤去はremove-filesで5を返した。これは実環境のファイル操作に
失敗した証拠であり、正式インストール／撤去成功の証拠ではない。
エラー5の詳細な対象ファイルと原因はまだ特定していない。
全削除のSID省略・システムSID指定を87、不明引数を87、存在しない入力を2で拒否。
結果はaudit/vistasetup-cli-result.txt。

新しいtests/vistasetup_rollback_probe.cと専用CLIランナーで、実行前後の状態を観測。
双方の実IFEO、VXsoft、対象のシェル拡張／ログ関連付け、AdministratorのVXsoft、
C:\VxKexの製品ファイル、System32／SysWOW64の13DLLと任意KxSChanlの内容・属性が
処理前後で一致した。最新版のaudit/vistasetup-rollback-result.txtはFailures=0、
Before／Afterのハッシュはdc03b36197a601df。
稼働中のLogs・ACL・タイムスタンプは観測範囲に含めていない。

実行部の使用条件と検証範囲をVistaSetup/README.mdに記録。
正式バッチの保護キャッシュ／呼出し置換、実システムへの確定を伴うライフサイクル、
別アカウントUAC試験は未完了。Installerのバイナリと本番VMのインストールは
今回も変更せず、診断用の外部フォルダーだけに新しい実行ファイルを配置した。
git diff --checkは成功（既存ファイルの改行警告のみ）。コミットは行っていない。

### 2026-10-01: 保護キャッシュとネイティブセットアップへのバッチ接続

VistaSetup/stage.cと `--prepare-cache` を追加。ProgramData配下の所有キャッシュに
実行ごとにPackage-{GUID}を作成し、入力一式をトランザクションで複製する。
既存のキャッシュは所有マーカー、保護DACL（BA／SYのみ）、BAまたはSY所有者を
要求し、不適切なACLやマーカーなしの既存フォルダーは変更せず拒否する。
複製前後に重要PEを検査し、全複製オブジェクトをBA所有・保護BA／SY DACLにする。
新規キャッシュ／マーカー／パッケージはステージ失敗時にまとめて取消する。
以前の実行ファイルを上書きせず、使用中でも別のGUID領域へ複製できる。

初回試験で、新規キャッシュのマーカーを通常APIで確認すると、同一トランザクション
の新規ディレクトリが見えない問題を確認し、トランザクション内の属性確認へ修正。
またSetSecurityInfoのエラー5を確認し、必要なREAD_CONTROLをハンドルに追加して修正。

tests/setup_cache_probe.cをServer 2008で実行し、audit/setup-cache-result.txtはFailures=0。
実パッケージの複製、全子ファイルの所有者／ACL、VistaPty同梱、前のヘルパーを
置換できない状態での再ステージ、入力PEのロック、複製中の非必須ペイロードのロック、
失敗後の旧二パッケージ保持、不適切キャッシュの拒否、専用領域の清掃を確認した。

Installer/install.batをネイティブ実行部へ接続し、Run-VxKexSetup.cmdを追加。
UAC要求はVistaSetupの `--elevate-installer` が元トークンSIDを取得して引き渡す。
一時VBSを生成する方式を置換。直接昇格実行した場合はその呼出元を元ユーザーとする。
インストール／更新／保持撤去／全削除撤去は、保護キャッシュへの複製後に
CALLを使わず外部ディスパッチャーへ移り、単一トランザクションの実行部を呼ぶ。
正式操作ではExplorerを停止した場合に成功／失敗の双方で再起動する。
診断用 `install.bat --check-install` は昇格済み限定でExplorerを停止しない。
Resetは既存の専用バッチを呼ぶ操作として残している。

新しいバッチ経由の確認モードを実VMで観測試験内から実行。
保護キャッシュを作成して外部ディスパッチャーへ移り、配備で32を返し、呼出元にも
32が伝わった。audit/vistasetup-batch-result.txtに記録。
audit/vistasetup-rollback-result.txtはFailures=0、実IFEO・登録・製品ファイルの
Before／Afterはdc03b36197a601dfで一致。これも確定成功の証拠ではない。

Installer/VistaSetup.exeをビルド成果で新規追加し、他の配布DLLは今回置換していない。
本番VMのインストールは未変更。診断用RealPackageには新しいバッチ／実行部を配置し、
ProgramDataの保護キャッシュには確認実行用のGUID領域が残る。
キャッシュの実行後清掃、実システムの確定を伴うライフサイクル、標準ユーザー・
別資格情報によるUAC、Explorer復旧の実動作試験は引き続き未完了。
ソースのInstallerは開発段階であり、公開・コミットは行っていない。

### 2026-10-01: 独立複製VMで実システムへの確定ライフサイクルを検証

元Server 2008を動かしたままNEXT_Setup_Lifecycle_Source_20261001を取得。
MCPは300秒でタイムアウトしたが、同じvmrunプロセスとVMwareログを観測し続け、
約9分38秒後のメモリ保存終了・スナップショット成功コールバックを確認した。
タイムアウトを失敗とみなして取得要求を再発行していない。

起動中VMからの直接cloneは拒否されたため、完了済みスナップショットの閉じた
2k8-000005.vmdkを参照する、起動しないVMX記述を別ディレクトリへ作成。
そこから完全複製を作成した。元VMの停止・復元は行っていない。
複製はVxKex-Next-Parity-Test/Server2008-SetupParity.vmx、CPU2・RAM4096MB、NICなし。
複製ディスクはparentCID=ffffffff、親参照なしの独立monolithicSparseであることを確認。
audit/setup-test-clone.jsonに元スナップショット／ディスク／複製先を記録。

tests/setup_system_lifecycle_probe.cと32／64bitのinstalled_version_probe.cを追加。
本番ルートへの書込み前に明示的な使い捨てVMマーカーを必須とする。
元の実VMにはマーカーを置かずに試験入口を実行し、最初のガードで拒否されることを確認。
結果audit/setup-system-guard-result.txtのFailures=1は、この意図した拒否の結果。

複製だけにマーカー・最新54ファイルのInstaller入力・診断実行ファイルを配置。
複製のPostgreSQLを正常停止し、Explorerを終了して、実System32／SysWOW64と
HKLM／対象ユーザーのルートでインストール確定→保持撤去確定→再インストール確定→
全削除確定を実行。初回は既存インストールの更新から始め、全四段階が成功。
初回結果audit/setup-system-lifecycle-initial.txtはFailures=0。

実行部のSTDOUT経路はIFEOランチャーを経由すると引き継がれず出力が空になったため、
子自身が独立レポートを書く方式を追加。再試験は新規インストールから開始し、
実32／64bitのKexDllLoaded=1、GetVersionExとRtlGetVersion双方の10.0.19045、
インストール直後・設定復元後双方の実返却値をレポートから独立に確認した。
audit/setup-system-lifecycle.txtはFailures=0。実シェル拡張／ログ所有登録、両実IFEOの
伝播テンプレート、両システムDLL、保持ストアの確定後存続と再インストール時の消費、
最後の全削除も確認。成功時には試験専用の実IFEOキーを清掃する。

手順・証拠の範囲をtests/setup_system_lifecycle.mdに記録。
標準ユーザー／別資格情報のUAC、対話バッチとExplorer復旧、実ルートのカスタム設定・
無効プロファイル・全既存プロフィールの復元、キャッシュ自動清掃は引き続き未完了。
元VMのインストールと稼働サービスは変更していない。コミット・公開は行っていない。
結果収集後に複製を正常シャットダウンし、元Server 2008だけが起動中であることを確認。
完全複製の独立性確認後、誤起動を防ぐため一時的な参照用VMXとコピーNVRAMを削除した。

### 2026-10-01: 実システムの無効設定・カスタム設定・別ユーザー範囲を追加検証

独立複製VMを再起動し、ライフサイクル試験を拡張して再実行。
最新audit/setup-system-lifecycle.txtは152行、Failures=0。
両実IFEOで有効・無効設定、無関係な値／フラグ、実LogDirとHKCU設定を検証。
インストール直後と復元後の各32／64bit子の独立レポートを計8件回収。
有効はKexDllLoaded=1、両API 10.0.19045、無効はLoaded=0、両API 6.0.6003。
保持撤去と復元でカスタム設定が保持され、全削除では製品設定のみ除去され、
無関係IFEO値／ビットと外部カスタムログディレクトリは残った。
初期・有効のみの旧実行結果も別名で保持。最新四段階ログも回収済み。

さらにtests/setup_user_scope_probe.c／ビルドスクリプトを追加。
マーカー付き複製に限り新規標準アカウントと実ユーザープロファイルを作り、
実トークン非管理者・昇格必要・実IFEO書込み拒否を確認。
管理者と標準ユーザーに異なる設定を用意し、管理者から標準ユーザーの実SIDを
明示したインストール／全削除を確定。指定ユーザー設定だけが削除され、
管理者設定は保持された。試験所有設定・新規アカウントと検証済みパスの
プロファイルは成功後に清掃。audit/setup-user-scope.txtはFailures=0。
初回は診断用グループ追加が名前解決で失敗し、製品処理前に中断。
作成アカウントを確認し、未作成プロファイルを確認してそのアカウントのみ清掃後、
SIDによるグループ追加へ修正して再実行した。

これは別ユーザー指定と権限の検証であり、UAC画面・ShellExecuteの昇格引継ぎ・
対話バッチ・Explorer復旧の実証ではない。それらと全既存プロファイル復元、
キャッシュ自動清掃、TLS正式配備、GUI等の残工程は引き続き未完了。
元VMは稼働を維持し、製品インストール・サービスは変更していない。
コミット・公開は行っていない。

### 2026-10-01: 完了済み保護キャッシュの自動回収を追加

VistaSetup/stage.cへ完了記録と回収処理を追加。Run-VxKexSetup.cmdは成功／失敗／
確認モードのいずれも、終了時に--complete-cacheを呼び、起動元cmdのPIDと作成時刻を
保護されたCompleted.binへ記録する。次回の準備成功後に、同じプロセスが終了した
パッケージのみを個別トランザクションで回収。PID再利用は作成時刻で判別。
プロセスの照会失敗・実行中・不明な名前・未完了・壊れた記録は保持し、
今回の新規パッケージとキャッシュルート／Owner.txtは回収しない。
入力が既存キャッシュの場合も、その古い完了記録を新規パッケージから除去する。
ロックによる途中失敗はパッケージ全体をロールバックし、セットアップ結果を変えない。

拡張tests/setup_cache_probe.cを複製Server 2008 VMで実行しFailures=0。
初回は完了記録が継承ACLのみだったため保護ACL確認で失敗し、明示的な保護ACL・
管理者所有へ修正。失敗ログもaudit/setup-cache-completion-acl-failure.txtに保持。
実プロセス生存中の保持、終了後の削除、後段ファイルのロック時の全体保持、
keep指定、未完了保持、不明フォルダ保持、古い記録を継承しない再ステージを確認。
二つの別cmdプロセスから実install.bat --check-installを順次実行し、両方status=0。
2回目の準備で1回目の完了キャッシュ削除、今回の完了記録保持、チェック後の
C:\VxKex不在をaudit/cache-cli-verify.txtで確認（Failures=0）。
最新VistaSetup.exeをInstallerへ配置。他の既存DLLの置換は行っていない。
完了記録のない過去／異常終了キャッシュは安全側に保持される。
対話メニュー・UAC引継ぎ・Explorer復旧、および他の高優先度移植は未完了。
コミット・公開は行っていない。
追加の壊れた完了記録（1バイト）も削除を許可しないことを再検証しFailures=0。
Installer/VistaSetup.exeとauditビルドのSHA256は双方
59DF44E99F5CD99FAD43B3D6F8884FBE00B14AD772F4CB5F45699FE624E09E26。
最新実行部を複製のRealPackageへ配置し、結果収集後に複製を正常停止。
元Server 2008だけが稼働していることをVMware MCPで確認した。

### 2026-10-01: GUI 権限分離の前提となるログ設定の読み取りを修正

KxCfgHlp/logging.cを修正。従来のQueryはユーザーキーを作成し、値欠落時の
未初期化EnableLoggingと空LogDirを利用していた。新QueryはKEY_READのみで
HKLMの既定値へHKCUの存在する値だけを重ね、実KexDllと同じ優先順にする。
欠落時は決定的な無効／空の既定値とし、キー作成を行わない。
不正な型・短いバッファ・無効な引数は失敗として返す。
初回の明示的保存だけが欠落ユーザーキーをトランザクション内に作成する。
環境変数展開でMAX_PATHを超える出力先は書込み前に拒否する。

保存後のディスク清掃登録は、通常Queryで古い確定済み設定を再読込みせず、
そのトランザクションへ書いた新しい出力先を直接渡す。登録処理失敗も伝える。
tests/logging_settings_probe.c／build_logging_settings_probe.ps1を追加。
診断プロセス内だけのRegOverridePredefKeyで専用のマシン／ユーザー領域へ分離し、
実マシン製品設定を変更しない。32／64bitともServer 2008複製VMでFailures=0。
キー未作成、マシン継承、部分ユーザー上書き、任意出力引数、不正値／短い出力、
書込みを実際に拒否する読み取り専用ACLでのQuery、初回保存のrollbackとcommit、
過長出力先拒否、清掃登録に新しい出力先が確定されることを検証した。
証拠はaudit/logging-settings-x64.txt／logging-settings-x86.txt。

最新x64ライブラリとGUIをaudit/build-read-only-logging以下へ再ビルド成功。
GUIのmanifestと起動時の強制昇格はまだ変更していない。標準ユーザー閲覧／
書込みだけを昇格するGUIへの切替には、全体設定・複数追加／削除・整理を扱う
昇格実行部と、別資格情報でも元ユーザーを扱う経路の実装／検証が残る。
今回の試験はそれらやGUI実操作の成功を意味しない。InstallerのKexCfgは未置換。
コミット・公開は行っていない。

### 2026-10-01: 本家ランチャー相当のメニュー起動と安全な登録更新を移植

全体設定の昇格保存へ接続する前に、残存するNEXT用コンテキストメニュー処理を修正。
以前は配布されていないVxKexLdr.exeを呼び、MSIも直接実行ファイルとして起動する
登録だった。EXEはVistaRun --with-kexで起動、MSIはそのモードから明示的に
Windows\System32\msiexec.exe /iへ渡す。パス／引数は引用符付きで組み立てる。

KxCfgHlp/ctxmenu.cを実HKLM\Software\Classes／64bitビューの処理へ変更。
KxCfgpUpdateContextMenuは明示ルートを受け、両形式を単一トランザクション内で更新。
無効化は自製ラベル／認識済みコマンドの値だけを除去し、外国の値・子キーは保持。
空になったキーだけを削除する。外部登録の上書きは183として拒否し、
呼出元のrollbackでEXE側の先行変更も戻す。既知の旧NEXTコマンドは更新可能。
空／未完成のメニューをQueryで有効と表示しない。文字列の不正型、埋込NUL等を拒否。

tests/context_menu_probe.c／build_context_menu_probe.ps1を追加し、32／64bitとも
複製Server 2008でFailures=0。実コマンド・通常／拡張切替、初期rollback、
他製品との後段競合rollback、無関係値／子キー保持、旧NEXT登録更新を確認。

実起動で、VistaRunを呼ぶだけでは未設定アプリにKexDllが届かないことを確認。
NEXT VxKexLdr/main.cのKexInitializePropagationを起点に、VistaRunへ専用
--with-kexモードを追加。このモードだけが伝播を明示初期化し、既存IFEOモードは
そのまま保持する。修正後、未設定の32／64bit子がLoaded=1を独立レポートで返した。
バージョン偽装を設定しないため、両APIのバージョンは6.0.6003である。
診断プローブに--unspoofedを追加し、Loaded=1かつ実Vistaバージョンという期待を
明示して検証。従来の既定10.0.19045確認と--disabled確認は変更していない。
MSIは存在しない診断用パッケージを/qnで渡し、msiexecの1619を確認。
これは実MSIインストールやインストーラーGUIの成功の証拠ではない。

tests/run_context_menu_runtime.cmdは使い捨てマーカーのバイト一致と新規状態を
確認して実インストールし、上記起動後に撤去。audit/context-runtime-status.txtは
EXE／WOW64とも0、MSI1619、Failures=0。実IFEOの拡張ライフサイクルも新ランチャーで
再実行し、audit/setup-system-lifecycle-context-launcher.txtはFailures=0。
最新GUI／ライブラリはaudit/build-context-menuへビルド。新VistaRun.exeは
Installerへ配置したが、GUIの標準閲覧／保存昇格への切替は未完了で未配布。
昇格実行部・元ユーザーSIDを用いた全体設定保存・複数追加削除／整理の接続、
実UAC引継ぎと他の高優先度工程は引き続き残る。元VMは変更していない。
コミット・公開は行っていない。

### 2026-10-01: 明示ユーザーへのログ保存と全体設定の昇格用CLIを実装

KxCfgpStageLoggingSettings(UserSoftware, MachineSoftware, KexDir, Enabled,
ResolvedLogDir, Transaction)を追加。元ユーザーのSoftware領域を明示して書込み、
昇格した別管理者のHKCUや環境変数を使わない。清掃登録も同じtxに含め、
外部CLSIDの所有登録は拒否し、全フィールドの書込み結果を確認する。
既存のKxCfgConfigureLoggingSettingsも同じ実装に統合し、tx指定なしでは
自身のtxを作り、HKCU設定と清掃登録をまとめて確定／取消する。

専用領域の32／64bit logging_settings_probeを拡張し、選択ユーザーだけの更新、
管理者HKCU保持、選択ユーザーと清掃Folderの同時rollback、外部清掃登録の拒否を
確認。audit/logging-settings-explicit-x64.txt／x86.txtともFailures=0。

KexCfg/global.cを追加して/GLOBALと/CHECKGLOBALを実装。
SID、展開済み絶対LogDir、Logging、MSI、ContextMenu、Extendedを全て必須とし、
重複・欠落・未知引数・非boolean・相対パス・system/service SIDを拒否する。
実HKUの正規化したSIDで読み込まれた元ユーザーのSoftwareキーを開く。
ログ設定、清掃登録、物理System32／SysWOW64双方のmsiexec、EXE／MSIメニューを
単一txで更新。確認モードと失敗は取消。native x64 writer限定で、x86版CLIは
NOT_SUPPORTED。GUIからnative writerを呼ぶ接続は今後実装する。

複製VMでtests/global_settings_cli_probe.cから実CLIを実行。
確認後の初期設定保持、全体保存、両MSI適用、後段MSIメニュー競合183でログと
両MSI・先行EXEメニューを元へ戻すこと、無効SID87を確認しFailures=0。
初回の診断は既定loggingをTRUEと仮定して失敗。実設定はFALSEで、元状態を
記録して比較する診断へ修正。失敗ログも別名で保持。仕様の既定値は変更していない。
証拠はaudit/global-settings-cli.txt／initial-expectation-failure.txt。

さらに実標準アカウントのprofileをロードするsetup_user_scope_probeを拡張。
管理者から標準ユーザーの実SIDを指定した/GLOBALを実行し、そのユーザーだけに
Logging／LogDirが保存され、管理者側LogDirは未作成のままであることを確認。
標準ユーザートークンへ偽装してRegOpenCurrentUserでキャッシュされていない実HKCUを
開き、標準ユーザー自身が新しい設定を読めることも確認した。
audit/global-settings-real-user-scope.txtはFailures=0。試験作成の設定、清掃登録、
ユーザーprofile、アカウントは検証後に清掃。元VMは変更していない。

詳細はKexCfg/README.md。最新GUI／ライブラリはaudit/build-global-settingsにビルド。
GUIのasInvoker化、保存時だけのUAC要求、キャンセル／失敗処理、複数アプリの
追加削除／整理への接続は未完了。InstallerのKexCfgは未置換、コミット・公開なし。

### 2026-10-01: 複数アプリの昇格用保存CLIを追加

KexCfg/bulk.cの/ADD、/DELETE、/CLEANを追加。絶対パス引数を全件検証してから
単一txで処理し、途中失敗／commit失敗時はrollbackする。権限確認を明示し、
各アプリのライブラリ内から個別に昇格する経路を呼ばない。
整理は保存側でも不在を再確認。FILE_NOT_FOUND／PATH_NOT_FOUNDかつ固定ドライブ
だけを対象とし、既存・アクセス拒否・ネットワーク・取得失敗の設定を保持する。

現在のnative x64 GUI/CLIをaudit/build-bulk-settingsへビルドし、独立複製VMで
実CLIを実行。後続の相対パス87が先行アプリを変更しないこと、2アプリの追加、
実在2件の整理保持、片方のファイルを削除した後の混在整理、2件削除を確認。
既存の全体設定保存／両MSI／後段競合rollbackの回帰確認も成功。
audit/bulk-global-settings-cli.txtはFailures=0。試験後は実インストールを削除し、
自身の設定・ファイルを清掃した。元Server2008/Vista VMには変更していない。

これは保存側CLIの実装・検証であり、GUIの呼出し接続・起動時昇格の除去は
引き続き未完了。bulk途中失敗rollbackと整理のアクセス拒否／ネットワーク分岐も
専用runtime検証が残る。Installer/KexCfg.exeは未置換、コミット・公開なし。

### 2026-10-01: 通常権限GUIと昇格writerを接続

KexCfg/writer.cを追加。HKLMで設定されたインストール先のnative KexCfg.exeを
ShellExecuteExのrunasで実行し、プロセス終了コードを取得する。待機中は親画面を
無効化し、メッセージを処理して再入保存／終了を抑止する。
引用はCommandLineToArgvWの規則に合わせ、末尾バックスラッシュも保護する。
32767文字のコマンド容量を超える要求は保存を開始せずにエラーにする。

GUIの全体保存は元プロセスのSIDと昇格前に展開したLogDirを渡す。複数追加・削除・
整理も単一writerへ接続。保存成功時だけ未保存フラグを消去／一覧を再読込し、
失敗またはUAC取消なら入力／一覧を保持。manifestをasInvokerへ変更し起動時の
強制昇格を除去した。native／x86を個別出力先へビルド。x86 GUIもinstalled native
writerを使うが、x86実行時のクライアント呼出しは未検証。

複製VMの実インストールへ診断用native buildを配置し、引用の往復（空白・Unicode・
引用符・末尾バックスラッシュ・空引数）とoverflow拒否、実writerの87終了コード
取得、既存global／bulk保存の回帰を確認。audit/gui-writer-global-cli.txtはFailures=0。

実標準ユーザーのprimary tokenでinstalled asInvoker CLIを起動するテストを追加。
初回はnetwork logon tokenから管理者の対話desktopを継承し0xC0000142で終了した。
この失敗はaudit/gui-writer-standard-user.txtに保持。専用recoveryはVM識別marker、
作成済みaccountのコメント、SID、profile絶対パス、元設定を確認し清掃した
（audit/gui-writer-scope-recovery.txt、Failures=0）。その後、テスト専用window station／
desktopに該当SIDのACLを付与して非対話起動を再検証。CLIは起動して5を返し、
設定を変えなかった。以降の昇格保存、元ユーザー読取り、全fixture清掃も成功。
audit/gui-writer-standard-user-isolated.txtはFailures=0。

この検証は対話UACの承認／取消、GUIボタン操作や全体設定の実UI反映を証明しない。
GUI接続は実装したが配布前検証は残る。Installer/KexCfg.exeは未置換、元VM変更なし、
コミット・公開なし。TLS、VxlView、setupを含む高優先度goal全体も未完了。

### 2026-10-01: native／WOW64 GUIの標準ユーザー閲覧を実行検証

setup_user_scope_probeの隔離desktopに実GUIを起動する診断を追加。
画面を切り替えず、EnumDesktopWindows／GetDlgItemで実ダイアログを検出し、
15コントロールとLoggingチェックを確認。元管理者Logging=0、標準ユーザー=1の
実hiveを用意した状態で、GUIは標準ユーザーの1を表示。未保存変更がないApplyと
非対応BHOが無効で、コマンドからCancelを送信して保存せず0で終了した。

nativeに加え、最新x86 KxCfgHlpをaudit/build-gui-reader-x86へ新規ビルドし、
同じ標準token・隔離desktopでWOW64 GUIにも全チェックを実施。
既存Win32/Releaseライブラリを置換しないため、x86 helperのビルドにも
OutputDirectory指定を追加した。
audit/gui-standard-reader.txtは最初のnative実行証拠、gui-standard-reader-both.txtは
最新native／WOW64実行とその後の昇格保存／設定scope／全fixture清掃の証拠。
いずれもFailures=0。元VMは変更なし、Installerへ未反映、コミット・公開なし。
GUI保存／選択dialog、対話UAC引継ぎと取消、bulk途中失敗rollback等は残る。

### 2026-10-01: 実GUIからnative writerへの保存を両アーキテクチャで検証

複製VMの隔離desktopで実GUIを既に昇格したoperatorとして起動し、Loggingを有効に
してログ保存先を編集、通常message loopへApplyをqueueした。入力は
%USERPROFILE%\\VxKex GUI 保存\\で、環境変数・Unicode・空白・末尾バックスラッシュを含む。
native／WOW64双方からinstalled native writerへの保存が完了し、実hiveに展開済み
ディレクトリとLogging=1が保存された。成功後はApplyが無効となり、Cancelで
未保存確認なしに0で終了。各GUI実行後は事前に作成したoperatorの0／LogDirなしへ
戻し、全体テスト後にinstall・cleanup登録・profile・accountを清掃した。
audit/gui-save-both-production.txtはFailures=0。実標準ユーザーへのUAC承認とは異なる検証。

初回は外部からWM_SETTEXTを送りcaptionだけが変更された一方、GUIが実際に読む
edit bufferは元のC:\\VxKex\\Logsのままだった。gui-save-both.txt、gui-save-values.txt、
gui-save-posted.txtを失敗証拠として保持した。元設定・VM marker・専用accountと
profile等を確認するrecoveryで、残った自身のfixtureを清掃（gui-save-recovery.txt）。
writer引数を記録する一時診断ビルドでも元のLogDirが渡されたことを確認。
EM_REPLACESELで実編集bufferを更新すると保存が成功し、一時記録には正しいUnicode
ディレクトリが含まれた（gui-save-command-edited.txt、UTF-16LE）。記録コードは
ソースから除去して通常ビルドで再度全チェックを実行して成功した。
WM_GETTEXTの外部観測はこの隔離desktopではcached captionなので、buffer検証には
実保存された文字列の厳密比較を用いた。通常GUI保存の不具合を示す結果ではなかった。

元VM、Installerは変更していない。コミット・公開なし。対話UACの承認／取消、
GUIの失敗保持・複数ファイル操作、bulk途中失敗rollbackと他の高優先度工程は残る。

### 2026-10-01: 実GUI保存失敗保持／再試行とbulk途中失敗rollbackを検証

native／WOW64両GUIの成功保存後に、事前に存在しないことを確認したMSI menuへ
専用foreign labelを作成。Loggingを0と新LogDirへ編集し、ContextMenuを有効にして
Applyした。後段の競合で元のLogging／LogDirへrollbackし、先行EXE menuも未作成に
戻った。実GUIの所有するエラーdialogが生成され、診断からそれだけを閉じた後、
MainWindowとApplyが有効で未保存状態を保持。専用foreign label／空keyだけを除去し、
再入力せずApplyすると保持されたLogging=0／新LogDirが保存され、Applyが無効化。
自身のmenuを除去して全fixtureを清掃。gui-save-failure-retry-both-v2.txtはFailures=0。

初回probeはTaskDialogのphysical IDOK controlを要求したためdialogを検出できず、
GUI failure保持のチェックが失敗した。一方その時点でも実tx rollbackは成功。
失敗ログはgui-save-failure-retry-both.txtに残し、marker・account・SID・profileと
exact foreign label／元設定を確認するrecoveryで専用fixtureを清掃した
（gui-failure-recovery.txt、Failures=0）。識別をtest PID・owner・dialog classに修正し、
TaskDialogのTDM_CLICK_BUTTONを用いて成功した。実UACを自動承認した検証ではない。

global CLI probeも拡張。専用コピー2つ目のPE subsystemを6.1に変更し、専用IFEOへ
foreign Debuggerを注入。実/ADDの1件目は保存段階へ進み、2件目で183が返る。
1件目のprofileがrollbackされ、foreign Debuggerも保持されることを確認。
注入したDebuggerだけを除去し/ADD再試行・混在整理・/DELETE・global保存回帰も成功。
audit/bulk-runtime-rollback.txtはFailures=0。実アプリのPEを変更していない。

元VM・Installerの変更、コミット・公開はなし。標準ユーザー対話UAC、GUIの複数
ファイル操作、整理のアクセス拒否／network分岐と他の高優先度工程は残る。

### 2026-10-01: TLS一式の再現可能な検証用パッケージを作成

tools/Stage-NextParityTls.ps1を追加。Installerの内容を新しい別directoryへコピーし、
current sourceからKexDll／KxAdvapi／KxCryp／KxSChanlのx86／x64計8DLLをビルドして
置換し、NEXTのROOT.sstをCertificatesへ追加。既存stage directoryは拒否し、全8DLLの
PE machine／DLL属性を検査。初回完全stageはaudit/NextParityTlsPackageの57ファイル。
audit/build-next-tls-package/package-manifest.jsonには全file hashと置換元を記録。
ROOT.sstはNEXTの3F39EA...335AFD2と一致。Windows Root storeへ登録していない。

x64 extended buildのNoStageとx86 OutputRootを追加し、既存VistaDLLs／Releaseを
上書きせず同じbuild root内の新KxCryp import libraryを優先。KexDllのKexPathCch依存は
architectureごとの明示libraryへ修正した。

最初のPowerShell pipelineではcompiler diagnosticの書込みが待機。生存したclと
parent chainを確認し、通常minidumpをWindbg MCPで解析してNtWriteFile／WriteFile／
msvcr100 write stackを確認した（compiler dumpのsynthetic breakpointは実crashではない）。
自身のbuild processのみ終了してOS直接stdout／stderr記録へ修正。さらに完了済み
子buildをStart-Process -WaitがMSPDB descendantの終了待ちしていたため、子buildが
terminalであることを確認後、orchestratorのみ終了しProcess.WaitForExitへ修正。
共有MSPDB serverを終了せず、一式のbuild／stageが成功した。

SSPI登録probeも、従来のEnumerate成功だけではなくactual provider／module load、
credential取得／解放、CredSSP保持を必須化。--nativeではproviderとKexDllが不在で
あることを要求する。両architectureのビルドは成功したが新stageに対するVM実行は
未実施。詳細はdocs/next-parity-tls-package.md。

Installer／元VMは未変更、commit／公開なし。TLSの実配備／runtime登録・native分離、
残るTLS失敗／分割／失効試験、設定GUI・VxlView・setupの残工程を継続する。

### 2026-10-01: TLS候補の実配備・32/64bitプロセス分離を検証

独立Server 2008 cloneへ57ファイルの候補を転送し、実VistaSetupと実KexCfg
を使うguard付きnative x64 deployment probeを追加。事前のfresh stateと
marker/elevation/所有fixture不存在を確認し、実caller SIDでインストールした。
8個のsource-built DLLをphysical System32/SysWOW64とbyte比較し、ROOT.sstも
実KexDirへのコピーと一致。32/64bitのnative-before/applied/native-after全6回で
Passed=1。appliedではKxSChanl追加、KexDll/KxAdvapi/KxSChanl読込み、資格情報の
取得・解放が成功。nativeは8packageのまま互換DLLなし。CredSSPを維持し、
machine SecurityProvidersはtype/size/bytesが全工程で不変。

初回のruntimeは成功したがcleanup handlerの既削除を失敗としていたため、
元結果をtls-package-deployment-first.txtに保存。実absenceを要求する判定へ
修正して全工程再実行、tls-package-deployment.txtはFailures=0。
作成したimage/profileだけを削除し、実uninstall後のinstall/prefs/cleanup handler
不存在も確認した。元VM/Installerの配布物は未変更、commit/publicationなし。
この候補でのhandshake/分割/失効/negative pathsおよびGUI/VxlView/setupの
残工程は未完了であり、高優先度goalは継続する。

### 2026-10-01: VXL読取り境界と破損ログの検証を修正

VxlView残工程の調査から、KexDllの単一readがcountと等しいindexを許容し、
multiple readは二重incrementで一件おきに飛ばし最後も読まない問題を確認。
本家2463にも同じrange loopがある。Vista側はinclusive range、末尾clipping、
write-mode/null destination拒否と正しいboundaryへ修正した。
index構築前／各entry走査時に実file size、64bit count合計、source文字列終端、
entry extent、severity/source index、文字数と終端、severity count一致を検証。
既存32bit file offset形式の上限も拒否し、不正fileはnull handleで返す。

独立buildのKexDllをprobe隣に置き、cloneの32/64bitで正常6件、日本語本文、
write-mode/one-past-end、全件clipped range、最終一件、null outputを実行。
専用CREATE_NEW copyによる破損10例を全てc0000098/null handleで拒否し、
vxl-reader-result-x64.txtとx86.txtはFailures=0。新しいprobeは既存viewer fixture
probeから分離して、過去のGUI/export試験fixtureのパスを変えていない。
Installer/元VM/TLS候補は未変更。詳細・再現手順は
docs/next-parity-vxl-reader-validation.md。GUI詳細/goto/export等の残工程は継続。

### 2026-10-01: VxlView詳細表示とraw navigationを専用desktopで検証

画面を切り替えないprivate window station/desktopの専用probeを追加し、
所有viewerのみを起動・操作・終了。既存search/wildcard/Unicode/severity試験に加え、
実goto dialogとdetailsを32/64bitで確認した。gotoが0を先頭として扱う不具合と、
filter lookupが配列参照前にdisplay indexを検証しない問題を修正。
6件で0/7を拒否、6へ移動してrecord5/日本語5/source file/line105/function/
severity/dateを表示。warning非表示中の3は拒否し、復帰後には受け入れる。
両viewerが正常終了、vxl-details-x64.txtとx86.txtはExit=0 Failures=0。

初回details文字列取得はcross-process GetDlgItemTextで空、実edit内部の
EM_GETLINEでは本文を確認できた。probeを実line読みへ修正し日本語本文まで検証。
初回nativeリンクは古いhelper archiveのassociation symbol不足だったため、
current helper archiveを指定して再build。診断結果も保存し、成功とは扱っていない。
詳細はdocs/next-parity-vxl-details-validation.md。Installer/元VM/TLS候補未変更。
export dialog/thread failureと他の高優先度残工程、最終配布統合は継続する。

### 2026-10-01: エクスポートの代表正常系と主要失敗復旧を確認

ExportLogのfilename allocation未確認によるnull copyを修正。
actual viewer objectsをリンクしたprivate desktop probeで、所有exeのimportに限り
allocation/thread creation failureを注入。実workerでは保存先不存在/排他lock/
成功exportを確認し、各statusと通知、一時disable後のowner/cursor復旧、開いたlogの
継続readを検証。workerを診断wrapper内でsuspended作成してcaller stack filenameを
変更後resumeすることで、copied filenameのlifetimeも実証した。通知TaskDialogは
probe内で記録し、描画確認とは区別。両architectureのvxl-export-failure結果はFailures=0。
manifest欠落とordinal import resolverの初期probe問題は修正後再実行した。

ユーザーの明確化した完了条件に従い、save dialog描画/追加stress等は非致命的TODOへ
整理し、同じ目的の再試験を追加しない。主要機能・rollback・必要なbitnessの証拠を
軸に残る配布統合と回帰を進め、最終Markdownに実装/統合/検証/残課題を記載する。
現時点はInstaller統合が未完でありgoalは未完了。詳細は
docs/next-parity-vxl-export-validation.md。

### 2026-10-01: 高優先度機能をInstallerへ統合

Stage-NextParityHigh.ps1を追加し、TLS一式・両bit設定GUI・両bitviewer・native setup
をcurrent sourcesからbuild。native/WOW helperもfresh build、VistaSetupへoutputdir
指定を追加。58file/13binary replacementのcandidateとsource/file hash manifestを作成。
初回はchild環境のGet-FileHash autoload失敗でpackage作成前に終了したため、SHA256を
.NETに変更し新builddirで実行。元artifactを削除せず、v2が成功した。

cloneに全candidateを配備し、guarded deployment probe --integratedでcandidate自身の
VistaSetup/KexCfgを使用。system DLL/ROOT/全5frontendのbyte一致、x64/WOW64 registration
分離、machine SSP不変、profile/install/prefs/cleanup teardownを確認。
high-package-deployment.txtはFailures=0。

Installerの既存全体をaudit/Installer-before-next-highへ保存し、宣言13binaryとROOTだけ
を反映。その他既存assetの変更を事前拒否し、反映後全58file hash一致を確認。
記録はaudit/next-high-installer-integration.json。元VM未変更、git commit/releaseなし。
今回の完了条件について最終audit/reportを残している。詳細は
docs/next-parity-high-integration.md。

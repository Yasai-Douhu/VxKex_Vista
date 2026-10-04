# KxNt 移植・検証の進捗

基準: [移植可能性調査](kxnt-next-2463-portability-audit.md)。作業ブランチ: `codex/next-parity-high`。
このファイルを機能ごとに更新し、検証が完了した機能をそれぞれコミット・プッシュする。

## 状態一覧

| 機能 | 実装 | 検証 |
|---|---|---|
| RtlIsProcessorFeaturePresent | 完了 | Server 2008 x86 / x64 成功 |
| RtlCanonicalizeDomainName | 完了 | Server 2008 x86 / x64 成功 |
| RtlGetDeviceFamilyInfoEnum | 完了 | Server 2008 x86 / x64 成功 |
| RtlGetPersistedStateLocation | 完了（既定パスへのフォールバック） | Server 2008 x86 / x64 成功 |
| RtlIsPackageSid | 完了 | Server 2008 x86 / x64、native 参照との比較成功 |
| RtlIsCapabilitySid | 完了 | Server 2008 x86 / x64、native 参照との比較成功 |
| RtlCheckTokenMembershipEx | 完了（NT 6.0 の通常トークン） | Server 2008 x86 / x64、native 比較と反復検証成功 |
| ZwCompareObjects / NtCompareObjects 精度改善 | 実装済み | Server 2008 x86 / x64 native 比較・並列検証成功。DLL 未読込みの対照でもコンソール初期化資源の増加を確認 |
| 拡張 rename / delete | 実装済み（通常操作。追加フラグは拒否） | Server 2008 x86 / x64、Nt / Zw 双方96ケース・各1,000回反復成功 |
| スレッド通知・待機 / Zw 別名 | 実装済み（プロセス内の状態管理） | Server 2008 x86 / x64、Nt / Zw の native 比較・4,096スレッド反復・実 ID 再利用・終了後回収成功 |
| RtlQueryPerformanceCounter / Frequency（既存の未解決転送） | 実装済み | Server 2008 x86 / x64、native 参照・未整列出力・例外・並列照会成功 |
| RtlTryAcquireSRWLockExclusive / Shared（既存の未解決転送） | NT 6.0 の native SRW と併用する試行取得を実装 | Server 2008 WOW64 / x64、待機者・条件変数・並行取得・例外・native 参照を検証 |
| RtlUTF8ToUnicodeN / RtlUnicodeToUTF8N（既存の未解決転送） | 部分変換・不正文字置換・サイズ照会を実装 | Server 2008 WOW64 / x64、native比較4,850呼出し・guard・全scalar往復・並行・overlap成功。初期化資源の完全な帰属は未確定。通常IFEOで値比較一致、両形式warm handle delta1で資源gate失敗 |
| RtlReportSilentProcessExit（起動時のリンク依存） | 入口のみ追加。NT 6.0 の終了監視は未対応 | 不正ハンドル9ケース比較・未対応エラー・反復・Zig インポート解決を検証。通常IFEOでも両形式の9case・1000反復を確認。WER の報告成功とは扱わない |
| ConDrv 向け NtWriteFile / ZwWriteFile | NT-I/O 内容プロファイルに限定した同期書込みを実装 | Server 2008 x86 / x64、実 Zig 標準出力・ファイル・パイプ、境界・衝突・並行・VT 成功。native の UTF-8 描画制限、通常 IFEO 起動の統合検証は残る |
| WNF / ZwQueryWnfStateData | 調査段階 | 本家にも未実装があるため実機能の対応を判断する必要あり |
| 既存の未解決 native 転送 | 調査段階 | 呼び出すアプリと API ごとに検証予定 |
| NtOpenKeyEx（既存の未解決転送） | 通常openを実装。拡張optionsはNT6で拒否 | Server / Vista両形式140case・1000反復成功。Server使い捨てcloneの通常IFEOも成功。backup / restore等は未対応 |

## 2026-10-02: RtlIsProcessorFeaturePresent

コミット・プッシュ済み: `e1a3a52`。

### 実施内容

- KexDll に `KexRtlIsProcessorFeaturePresent` を実装し、共通宣言とエクスポートを追加。
- KxNt に `RtlIsProcessorFeaturePresent` の公開経路を追加。実際の KxNt → KexDll 経路で確認した。
- 64 以上の機能番号は FALSE。CPUID だけで判断せず、Vista の `IsProcessorFeaturePresent` に基づいて OS が利用可能とする機能を返す。
- NEXT の共有データ直接読み取りは x64 の MMX (番号 3) を TRUE とした。一方、VM の Win32 API と現行 Windows ホストの native RTL は FALSE。この違いを検出したため、単純コピーから OS API の利用へ修正した。
- KxNt の両ビルドスクリプトが固定パスの別作業ツリーのライブラリを参照していた問題を修正。同じブランチの KexDll import library でリンクする。

### 検証範囲

Windows Server 2008 x64 (NT 6.0.6003) の専用フォルダー `C:\KxNtParity` に配置した DLL を利用。インストール済みシステム DLL の置き換えはしていない。

| 実行形式 | native RTL の存在 | 0〜63 の値と LastError 維持 | 範囲外 5 値 | 結果 |
|---|---|---|---|---|
| 32bit WOW64 | なし | 64 件成功 | 64、65、255、0x7fffffff、0xffffffff すべて FALSE | PASS |
| 64bit | なし | 64 件成功 | 同上 | PASS |

native RTL がある場合は native RTL、ない場合は documented Win32 API を期待値として使用。ホストで native RTL 自体の 64 機能照会と範囲外 5 値も確認した。

ビルド: KexDll / KxNt の x86 / x64 成功。静的ライブラリの PDB 不足警告はあるがリンクは成功。Vista クライアント VM と実アプリでの回帰検証はまだ実施していない。

### 再実行

```powershell
# KexDll を先にビルドし、次に KxNt をビルドする。
./build_kexdll.ps1
./build_kexdll_x86.ps1
./build_kxnt.ps1
./build_kxnt_x86.ps1
./tests/build_kxnt_probes.ps1 -Architecture x86
./tests/build_kxnt_probes.ps1 -Architecture x64
./tests/run_kxnt_processor_feature_vm.ps1 -VMX '<VM の vmx パス>' -GuestPassword '<ゲストのパスワード>'
```

既存の KexSmp / KexMLS / KexPathCch のライブラリが必要。検証ログとバイナリ SHA256 は `audit/KxNtParity/processor-feature-receipt.json` に保存する。プローブの終了コードとログの PASS を両方確認し、成功を偽装しない。

### VMware の状況

REST API はタスクスケジューラの `YamaR` / 最上位の特権で実行されている。ファイル転送・診断 EXE・本プローブの実行は成功した。cmd.exe のコマンド実行だけは終了コード 1 が続いたため、検証はネイティブ EXE を直接起動するスクリプトで行う。タスクスケジューラへの移行を、この失敗の原因とは断定していない。

## 2026-10-02: RtlCanonicalizeDomainName

### 実施内容

- NEXT の実装を `KexDll/rtldomain.c` に移植。公開エクスポート、共通宣言、Visual Studio プロジェクトを更新した。
- ドメイン名の IDN 変換・小文字化、IPv4 / IPv6 の正規化、IPv4 mapped IPv6 の IPv4 への変換、strict フラグの扱いを実装。
- VS2010 同梱 SDK には ip2string.h がないため、既に VM で存在確認した 4 つの IP 変換関数の宣言を追加した。Windows Sockets DLL の読み込みやネットワークアクセスは不要。
- SourceString の NULL / 不正な Length / MaximumLength を検査。不正 UTF-16 の下位 API の `STATUS_NO_UNICODE_TRANSLATION` を、native RTL の `STATUS_INVALID_IDN_NORMALIZATION` に合わせた。
- 追加関数の ordinal を固定し、移植前 (`24a03ae`) の既存の公開名の番号を維持した。新規公開 API を自動採番すると既存番号がずれるため、以降の追加も固定番号を用いる。KxNt の今回の追加は 2200 / 2201、KexDll は 300 / 301。

### 検証範囲

ホストの native `ntdll!RtlCanonicalizeDomainName` で参照結果を取得し、Server 2008 の 32bit / 64bit の KxNt → KexDll 経路と比較。25 ケースすべてで NTSTATUS、Length、出力 UTF-16 が一致した。

対象: ASCII 大文字、末尾ドット、Punycode、ドイツ語・日本語 IDN、IPv4、省略形式・16進・8進 IPv4 と strict フラグ、IPv6、IPv4 mapped IPv6、スコープ、括弧・ポート付き表記、空文字、連続ドット、空白・アンダースコア、孤立サロゲート。

追加確認: ガードページ直前で終わる非 NULL 終端の入力、256 文字の長い入力の拒否、出力の長さ・NULL 終端、2,000 回の確保と RtlFreeUnicodeString による解放、プロセスヒープの検査。いずれも両アーキテクチャで成功。

参照結果は `docs/validation/kxnt-domain-reference.json`、VM のログ・検証バイナリ SHA256 は `docs/validation/kxnt-domain.json` に保存。実際の終了コード、期待する全行との比較、PASS 表示を確認する。CPU 機能照会の回帰検証も両アーキテクチャで成功。

`tests/check_kxnt_export_ordinals.py` により、KxNt / KexDll の x86 / x64 全4バイナリの既存の公開名の ordinal を比較し、変更ゼロを確認。結果は `docs/validation/kxnt-export-ordinals.json` に保存した。

```powershell
./tests/run_kxnt_processor_feature_vm.ps1 -Probe domain -VMX '<VM の vmx パス>' -GuestPassword '<ゲストのパスワード>'
```

制約: Vista の IDN テーブルを使用しているため、今回のケース外の新しい Unicode 文字や正規化仕様まで現行 Windows と同一とは保証しない。メモリ不足の注入、Vista クライアント VM、実アプリの回帰は未実施。

## 次の作業

SID 分類とトークン検査を実装・検証する。スレッド通知・待機などの状態管理を伴う機能も、調査の対象範囲として引き続き進める。

## 2026-10-02: RtlGetDeviceFamilyInfoEnum

コミット・プッシュ済み: `17c252f`。

- KexDll の実装と KxNt の公開経路を追加。固定 ordinal は KexDll 302、KxNt 2202。
- UAP バージョンはプロセスの PEB の major / minor / build を 16bit ごとに格納する。本家の固定値 3570 は使わない。NT 6.0 に unified build revision がないため、revision は 0。
- Vista クライアントは Desktop (3)、Server / DC は Server (9) として分類。フォームは本家と同じ Desktop (3) の互換既定値。ノート PC / タブレットなどの筐体を検出する実装ではない。
- [Microsoft の関数仕様](https://learn.microsoft.com/en-us/windows/win32/devnotes/rtlgetdevicefamilyinfoenum)と [SDK 定義](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/um/winnt.h)で宣言・定数・Windows 10 以降の API であることを確認。VM の native ntdll には関数がないことを再確認した。

### 検証範囲

Server 2008 x86 / x64 で次を確認した。

- 任意出力の NULL を含む全8組み合わせ。NULL の出力には触れず、値の前後のガードと LastError を保持。
- RtlGetVersion が報告する 6.0.6003 に一致するバージョン構成、Server 系列の分類。
- プローブ自身の PEB だけを一時的に 10.0.19045 / 10.0.26100 / 6.1.7601 に変更した3ケースで追従を確認。書き込み前に PEB のレイアウトを RtlGetVersion と照合し、終了時に元の値へ戻した。システム設定は変更していない。
- KexDll / KxNt の両アーキテクチャのビルドと、既存エクスポート番号の維持。

ログ・SHA256 は `docs/validation/kxnt-device-family.json`。再実行は `tests/run_kxnt_processor_feature_vm.ps1 -Probe device-family`。Vista クライアントでの分類と実アプリでの回帰は未検証。

## 2026-10-02: RtlGetPersistedStateLocation

### 実施内容

- NEXT の既定パスへのフォールバックを移植。NT 6.0 には StateSeparation のリダイレクトマップがないため、DefaultPath があればコピーし、なければ `STATUS_OBJECT_NAME_NOT_FOUND` を返す。マップによるリダイレクト機構全体の実装ではない。
- STATE_LOCATION_TYPE を共通ヘッダーに追加。レジストリ (0) / ファイルシステム (1) を受け付ける。
- 必要サイズは終端 WCHAR を含むバイト数。不足時は `STATUS_BUFFER_OVERFLOW` と必要サイズを返し、出力バッファには書き込まない。
- WCHAR 数から ULONG のバイト数へ変換する際の整数オーバーフローを検査。本家の省略していた検査を追加した。
- 公開 ordinal は KexDll 303、KxNt 2203。既存の公開番号を維持。
- [Microsoft の仕様](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-rtlgetpersistedstatelocation)を確認した。kernel mode の資料だけに依存せず、ホストの user mode native ntdll でも返却値を測定して比較した。

### 検証範囲

Server 2008 x86 / x64 と native Windows の参照結果で、16 ケースの NTSTATUS、必要サイズ、出力 UTF-16、例外結果が一致した。

対象: 両保存先種別、不正な種別と負の値、NULL バッファでのサイズ照会、1 / 2 バイト不足、ちょうどのサイズ、余裕のあるサイズ、任意のサイズ出力の省略、空の既定パス、既定パスなし、CustomValue、日本語を含むファイルパス、不正な NULL 出力先。

失敗時のバッファ全体の不変性と前後のガードも検査。不正な NULL 出力先に十分なサイズを指定するケースでは、native 同様にアクセス違反が起こることをプローブ内の SEH で捕捉した。このケースを通常の成功として返す実装ではない。

資料: `docs/validation/kxnt-persisted-state-reference.json`（native 参照）、`docs/validation/kxnt-persisted-state.json`（VM ログと SHA256）。再実行は `tests/run_kxnt_processor_feature_vm.ps1 -Probe persisted-state`。

残る検証: 整数オーバーフローを起こす巨大な実文字列の動的検証、Vista クライアント、実アプリ、StateSeparation マップによるリダイレクト。CPU 機能照会、ドメイン正規化、デバイス情報の既存3機能は最新 DLL で再検証済み。

### ビルドの自動化

`tests/build_kxnt_parity.ps1` で KexDll → KxNt → プローブを x86 / x64 で順番にビルドする。途中のコンパイラー・リンカーの終了コードが非ゼロなら停止し、古いバイナリで検証を続けない。詳細ログは `audit/KxNtParity/` に保存する。必要な KexSmp / KexMLS / KexPathCch のライブラリは事前にビルドしておく。

## 2026-10-03: RtlIsPackageSid

- NEXT の native 転送を、Vista にない API の独立した実装へ置き換えた。SID revision 1、identifier authority 15、subauthority count 2 以上、最初の RID 2 を判定する。
- この関数は SID 系列の分類であり、SID 全体の妥当性やトークンの権限を検査するものではない。native は通常の SID 上限を超える count 16 / 255 も分類するため、この挙動を維持した。権限検査には別途 SID 妥当性検査が必要。
- 宣言・導入時期は [phnt の RTL 定義](https://github.com/winsiderss/phnt/blob/master/ntrtl.h)で確認。NEXT も Windows 8 以降の native API を転送しており、Vista の native にない関数として移植した。
- KexDll 304、KxNt 2204 の固定 ordinal で公開。既存公開番号の検査を実施。

### 検証範囲

ホスト native ntdll の x86 / x64 と Server 2008 の KxNt → KexDll を比較。revision、count、authority、RID を組み合わせた 660 ケース、NULL、読み取り可能長 0～16 バイトのガードページ 17 ケースで返却値・例外結果が一致した。NULL や不足した入力は、native と同じアクセス違反をプローブ内の SEH で捕捉した。

参照結果: `docs/validation/kxnt-sid-package-reference.json`。VM ログ・DLL とプローブの SHA256: `docs/validation/kxnt-sid-package.json`。再実行: `tests/run_kxnt_processor_feature_vm.ps1 -Probe sid-package`。SID 分類に AppContainer の作成・隔離機能は含まれない。Vista クライアントと実アプリでの回帰は未実施。
## 2026-10-03: RtlIsCapabilitySid

- Package SID と共通の分類処理を利用し、S-1-15-3-… を識別する関数を追加。SID revision、authority、count の条件は native に合わせた。
- KexDll 305、KxNt 2205 の固定 ordinal で公開。全4 DLL の既存公開番号に変更がないことを確認。
- [phnt の宣言](https://github.com/winsiderss/phnt/blob/master/ntrtl.h)を確認。NEXT の Windows 8 以降の native 転送を Vista 上の独立実装に置き換えた。

### 検証範囲

Server 2008 x86 / x64 で 660 組み合わせ、NULL、17 ガードページ境界ケースの返却値・例外を native Windows と比較し、一致した。ガードページは Capability SID 自体を入力する。Package SID の全ケースも最新 DLL で再実行し成功。

参照結果: `docs/validation/kxnt-sid-capability-reference.json`。VM ログ・SHA256: `docs/validation/kxnt-sid-capability.json`。再実行: `tests/run_kxnt_processor_feature_vm.ps1 -Probe sid-capability`。分類処理は AppContainer の作成・権限付与・隔離機能ではない。Vista クライアントと実アプリでの回帰は未実施。
## 2026-10-03: RtlCheckTokenMembershipEx

### 実施内容

- NEXT の native 転送に対し、Vista / Server 2008 の通常トークン向けの独立実装を追加。KexDll 306、KxNt 2206 で公開する。
- 対象 SID にアクセスを許可する一時 ACL と security descriptor を作り、NtAccessCheck で判定する。SID 一覧の単純検索ではないため、制限 SID と deny-only 属性の判定を NT 6.0 自体に委譲できる。入力サイズに比例するスタック確保はしない。
- NULL token は現在のスレッドを対象とし、スレッドトークンがない場合だけプロセストークンを偽装トークンに複製する。開いたハンドルを閉じ、呼び出し側のトークンを閉じない。
- BOOLEAN 出力に 1 バイトだけ書き込み、Win32 LastError を変更せず NTSTATUS を返す。不正フラグを拒否する。
- Vista の RtlValidSid は読めない SID を FALSE に変えるため、そのまま使用すると native のエラーと異なった。ヘッダー・整列・長さを検査して SID を取り込み、読み取り失敗を NTSTATUS として返すように修正した。

### フラグと対応範囲

CTMF_INCLUDE_APPCONTAINER (1) / CTMF_INCLUDE_LPAC (2) と両者の組み合わせは、NT 6.0 の通常トークンでは通常のアクセスチェックを行う。native Windows でも今回の通常トークンに対して同じ判定になることを測定した。NT 6.0 には AppContainer / LPAC token class がなく、これらの作成・隔離・権限モデルを実装したものではない。その他のフラグは STATUS_INVALID_PARAMETER。API の存在を増やしただけで AppContainer 対応と扱わない。

宣言とフラグ: [phnt](https://github.com/winsiderss/phnt/blob/master/ntrtl.h)。導入時期・通常トークンと AppContainer の区別: [Microsoft CheckTokenMembershipEx](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-checktokenmembershipex)。アクセスチェックの偽装トークン・TOKEN_QUERY・security descriptor の要件: [Microsoft AccessCheck](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-accesscheck)。Ex は Windows 8 / Server 2012 以降、元の CheckTokenMembership と AccessCheck は Vista でも利用できる。

### 検証範囲

ホスト native ntdll と Server 2008 x86 / x64 を比較し、114 ケースの NTSTATUS・BOOLEAN・例外・前後ガード・LastError が一致した。

- 通常の偽装トークン、NULL token、primary token の拒否、query-only / query 権限なし、匿名偽装。
- 制限トークン（World と現在ユーザー）、World を deny-only にしたトークン、スレッド偽装中の NULL token。
- フラグ 0 / 1 / 2 / 3 / 4 / 0xffffffff、不正・別オブジェクト型のハンドル、不正 SID / NULL SID / NULL 出力。
- subauthority count 0 / 16、読み取り可能長 0～12 バイトの guard page と SID 整列。native 同様に読み取り不可は STATUS_ACCESS_VIOLATION、整列違反は STATUS_DATATYPE_MISALIGNMENT として返す。NULL 出力だけは native と同様にプローブ内で例外を捕捉した。
- NULL token への 2,000 回の呼び出しで判定が維持され、終了時のプロセスハンドル数増加は 0。

参照結果: `docs/validation/kxnt-membership-reference.json`。VM ログと SHA256: `docs/validation/kxnt-membership.json`。再実行: `tests/run_kxnt_processor_feature_vm.ps1 -Probe membership`。両アーキテクチャのビルドと既存エクスポート番号の維持も確認。Vista クライアント・実アプリ・AppContainer / LPAC は検証範囲に含めていない。
## 2026-10-03: ZwCompareObjects と NtCompareObjects の精度改善

### 問題の再現

旧版 e992afd の実バイナリを別フォルダーへ配置して再検証した。両イベントに同数の複製ハンドルを作ると、別々の無名イベントを STATUS_SUCCESS（同一）と誤判定した。同じ名前付きイベントの再オープン、疑似ハンドルと権限ゼロの実ハンドルなども誤判定した。x86 は 16 ケース中 6 件、x64 は 5 件失敗。バイナリ SHA256 と出力は `docs/validation/kxnt-compare-before.json`。

### 実施内容

- ZwCompareObjects を KxNt ordinal 2207 で公開し、既存 NtCompareObjects と同じ KexDll 実装へ転送した。既存 ordinal を変更しない。
- 名前・参照数・属性の近似比較を廃止。両入力ハンドルを現在のプロセスに複製して対象オブジェクトを保持し、SystemExtendedHandleInformation の識別情報を照合する。対象が同じファイル名でも、別々に open した FILE_OBJECT は異なるものとして扱う。
- コピーしたハンドルだけを閉じる。呼び出し側のハンドルやイベント状態・ファイル内容を変更しない。識別情報が取得できない場合は STATUS_NOT_SUPPORTED など実際の失敗を返し、名前一致で成功扱いしない。
- 情報バッファはヒープ上で拡張し、回数・サイズを制限する。ハンドル数と返却長の整合性も検査する。情報取得はシステム全体のハンドル表を走査するため、native の直接比較よりコストが高い。
- WOW64 の通常の情報取得は 64bit の kernel object pointer を 32bit に切り詰める。この値だけで同一性を決めない。native 64bit ntdll の関数を PEB64 / loader 情報と PE エクスポートから取得し、4 引数の x64 ABI ブリッジで元の 64bit ハンドル表を取得する。
- NtWow64QueryInformationProcess64 / NtWow64ReadVirtualMemory64 を利用し、アプリのファイル名やビルドごとの API アドレスは固定しない。native ntdll の PE を x86 から解析する前に、イメージ全体が 4GB 未満にあることを検査する。今回の NT 6.0 では成立した。成立しない環境ではアドレスを切り詰めず STATUS_NOT_SUPPORTED とする。
- native に NtCompareObjects が存在する OS では native を利用する。native API と 64bit 関数の検索結果は atomic にキャッシュし、繰り返しの loader 検索を避ける。Win32 LastError を保持する。

構造体の根拠: [phnt SystemExtendedHandleInformation](https://github.com/winsiderss/phnt/blob/master/ntexapi.h)。同一の kernel object を比較し、特定の照会権限を要求しない意味と導入時期: [Microsoft CompareObjectHandles](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-compareobjecthandles)（Windows 10 / Server 2016 以降）。NT 6.0 の native API は先の存在調査で不在を確認済み。

### 検証範囲

Server 2008 x86 WOW64 / x64 で次を検証した。

- 同一・複製ハンドル、同じ参照数の別々の無名イベント、異なるオブジェクト型、同一の名前付きイベントの再オープン。
- 権限ゼロの同一・別オブジェクト、current process / thread の疑似ハンドルと実ハンドル。
- ファイルの複製と、同じパスを別々に open したハンドル。
- NULL（左右・両方）、同じ不正ハンドル値、閉じたハンドル。
- 16 ケースについて Nt / Zw 双方の返却 NTSTATUS と LastError がホスト native に一致。
- 4 スレッド同時の初期化・比較（ウォームアップ 800 回＋計測 800 回）。全返却値が正しく、計測区間のハンドル数増加は 0。さらにハンドル値・object ID・オブジェクト型の前後の集合が不変であることを VM runner のゲートで検査した。
- KexDll / KxNt の x86 / x64 ビルドと、全4配布 DLL の既存エクスポート番号維持。

### 継続確認する事項

初回の並列処理ではプロセス内ハンドルが増えた（当初の保存ログは x86 6、x64 7）。以下の対照テストを追加し、比較 API を呼ばず KexDll も読み込まないプロセスでも、コンソール初期化資源が増えることを確認した。初回から資源増加ゼロとは扱わない。個々のイベントの作成スタックまで特定したものではない。

Vista クライアント、native 32bit OS、標準ユーザー / 制限トークン、大規模なハンドル表、native ntdll が 4GB 以上にある配置、他スレッドによる入力ハンドルの close / 再利用競合は未検証。複製後のオブジェクトは保持する設計だが、複製前の入力変更まで atomic な native syscall と同じに扱えると主張しない。

参照結果: `docs/validation/kxnt-compare-reference.json`。VM 出力・DLL / プローブ SHA256: `docs/validation/kxnt-compare.json`。再実行: `tests/run_kxnt_processor_feature_vm.ps1 -Probe compare`。diagnostic 行はポインターや環境固有の初期化を記録し、native 参照の固定値比較から除く。計測前後の識別情報・型は別途 runner 内で厳密に比較している。

### 初期化資源の対照検証

`tests/run_kxnt_compare_controls_vm.ps1` で x86 / x64 の各6モードを実行し、初期・計測前・計測後のハンドル集合、キー名、取得できるプロセスイメージを保存した。

- NoQuery: worker は比較処理を呼ばない。LookupOnly: native エクスポートの検索のみ。AllocateOnly: ヒープ確保と解放のみ。SystemOnly: ヒープ上のバッファへ native NtQuerySystemInformation のみ。
- IdleOnly: worker は各区間で2秒待機。ConsoleOnly: 同じ待機を行い、KxNt の LoadLibrary 自体を省略する。ConsoleOnly は KexDllLoaded=0、CompareCases=0 をスクリプトで検査する。
- ConsoleOnly の x86 でも SysWOW64 の conime.exe、Nls\\CustomLocale キーが追加された。x64 では System32 の conime.exe と CurrentVersion / AppCompatFlags のキーが追加された。互換 DLL と比較 API がなくても再現するため、これらの増加を今回の比較用複製ハンドルの漏れとは扱わない。
- ConsoleOnly を含め診断用の native 情報照会は行う。他5モードは資源計測を終えた後に共通の16ケースを検証するため、プロセスの全期間で比較 API 未使用という意味ではない。
- 保存した12モードの終了コードはすべて0、計測前後の集合変化も0。本検証も再実行し、Nt / Zw の16ケースと並列計測800回が両アーキテクチャで成功した。

ログとバイナリ・プローブ SHA256 は `docs/validation/kxnt-compare-controls.json`。短い区間では非同期初期化のタイミングが変わるため、対照スクリプトは非ゼロ結果も記録する。本検証の native 比較・資源不変ゲートを置き換えたり緩和したりしない。コンソール作成の背景: [Microsoft](https://learn.microsoft.com/en-us/windows/console/creation-of-a-console)、Vista の conime に関する開発者資料: [ConEmu](https://conemu.github.io/en/FAQ-8.html)。OS の IME 設定変更や conime 無効化は実施していない。

## 2026-10-03: 拡張 rename / delete

### 実施内容

- NEXT の Ext_NtSetInformationFile を基に、FileDispositionInformationEx (64) / FileRenameInformationEx (65) を NT 6.0 の旧情報クラス13 / 10へ変換する実装を追加した。Server 2008 の native では両拡張クラスが STATUS_INVALID_INFO_CLASS となることも実測した。
- 削除フラグ0 / DELETE (1)、rename フラグ0 / REPLACE_IF_EXISTS (1) を扱う。POSIX、readonly 属性無視、ON_CLOSE、その他の追加フラグは STATUS_NOT_SUPPORTED。NEXT のように無視して成功させない。新しいファイルシステム機能を再現したものではない。
- rename はヘッダーと名前だけをヒープに取り込み、入力バッファを変更しない。名前長と入力長を減算で検査し、整数オーバーフローを避ける。奇数・空・不足した名前は拒否、65,534バイトを超える名前は STATUS_NAME_TOO_LONG。入力長に比例するスタック確保や、不要な末尾バイトの読み取りをしない。
- RootDirectory と可変長 Unicode 名を保持する。通常の情報クラスは native NtSetInformationFile へ委譲する。NTSTATUS / IO_STATUS_BLOCK を返し、Win32 LastError を保持する。
- KexDll ordinal 307 で公開。KxNt の既存 NtSetInformationFile / ZwSetInformationFile をこの関数へ転送し、x86 / x64 双方の既存 ordinal を維持した。全4配布 DLL の公開番号検査は変更0件。

仕様と導入時期: [Microsoft FILE_INFORMATION_CLASS](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ne-wdm-_file_information_class)、[rename のレイアウトとフラグ](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_rename_information)、[拡張削除の意味](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/2e860264-018a-47b3-8555-565a13b35a45)。拡張クラスは Windows 10 世代の機能であり、旧操作の API 自体が Vista にないという意味ではない。

### 検証範囲

`tests/kxnt_file_information_probe.c` を VS2010 / SDK 7.1 でビルドし、Server 2008 x86 WOW64 / x64 の実際の KxNt → KexDll 経路を検証した。各アーキテクチャ96ケース、失敗0。通常操作は別々に作成したファイルへ拡張クラスと同じ VM の native 旧クラスを適用し、返却値・IO_STATUS_BLOCK・元と先のファイル存在・内容を比較する。成功すべきケースは STATUS_SUCCESS と実際の移動・削除も必須条件にする。

- 絶対名、Unicode 名、RootDirectory 相対名、衝突・置換、readonly の置換先、DELETE 権限なし。
- 削除設定・解除、readonly ファイル、DELETE 権限なし。FILE_FLAG_OVERLAPPED で開いたハンドルも通常操作を確認（今回のローカル NTFS では同期的に完了）。
- 追加フラグ、短い・長い削除構造、短い rename ヘッダー、NULL、空・奇数・不足・オーバーフローする名前長、長すぎる名前。
- ヘッダー・名前・削除フラグのガードページ、書き込み不可の IO_STATUS_BLOCK。拒否後も元ファイルと置換先の内容が維持される。
- readonly 入力と直後のガードページに、0xffffffff の入力長と短い実名を指定。巨大確保・末尾読み取りをせず成功し、入力は不変。
- 通常の FilePositionInformation と不正ハンドルの結果を native と比較。全呼び出しで LastError を確認。
- Nt / Zw ごとに1,000回の交互 rename を行い、計測ハンドル増加0、最後のファイル内容も維持。初回の短い計測は x86 で増加5、続く Zw 区間は0だった。先のコンソール初期化対照を踏まえ、成功呼び出し後に2秒待機してから測定するようにした。初期資源の個々の作成スタックを、このファイルプローブで分類したものではない。

ホストの native ntdll でも通常操作34ケースを Nt / Zw 双方で実行し、x86 / x64 成功。未対応フラグの拒否などは Vista 版の明示的な方針を検証しており、現行 Windows の拡張機能すべてとの結果一致を主張しない。

VM 出力と DLL / プローブ SHA256: `docs/validation/kxnt-file-information.json`。ホスト native 出力: `docs/validation/kxnt-file-information-reference.json`。再実行は `tests/build_kxnt_parity.ps1` → `tests/run_kxnt_processor_feature_vm.ps1 -Probe file-information`。ホストの通常操作参照は `file-information.exe ntdll.dll <出力ファイル> Native`。

残る範囲: Vista クライアント、native 32bit OS、標準ユーザー、実アプリ、ネットワーク・他ファイルシステム、実際に pending となる I/O、同時 rename / delete 競合、追加フラグの意味の実装。新 DLL は作業ブランチの Installer と VM の専用検証フォルダーへ配置し、システム配備や Releases 公開は行っていない。

## 2026-10-03: スレッド通知・待機の実装前検証

NEXT の ntalrtid.c と現行の ntthread.c / dllmain.c を確認した。現行の両関数は STATUS_NOT_IMPLEMENTED の本体で、KxNt の公開経路にもない。NEXT が使う TEB 末尾の状態領域と共有 keyed event は現行 Vista 版にない。また現行 DllMain は通常ロード時にスレッド通知を無効にするため、NEXT の thread attach 初期化だけをコピーしても成立しない。

`tests/kxnt_alert_probe.c` と `tests/run_kxnt_alert_reference.ps1` を追加し、実装の基準になる native の挙動を測定した。ホスト x86 / x64 は Failures=0。Server 2008 の system ntdll を直接ロードしたプローブは、両アーキテクチャとも Alert=0 / Wait=0（公開名なし）、予定した診断終了コード6。これは互換実装の成功ではなく、移植が必要なことの確認。

- 自スレッドおよびまだ待機していない worker への通知が、次の待機を STATUS_ALERTED にする。15回の重複通知は1回分にまとまり、次のゼロ待機は STATUS_TIMEOUT。
- Timeout=0、短い相対時間、過去の絶対時刻は通知がなければ TIMEOUT。無効な timeout ポインターは ACCESS_VIOLATION。通知済みでも先に timeout の読取り失敗を返し、その後の正常な待機では通知が維持される。
- unreadable な Hint 値を渡しても、この試験の待機は成立した。User APC をキューに入れてもこの待機は APC を実行せず、明示的な SleepEx(TRUE) で初めて実行された。
- 自スレッド ID の下位2ビットを付けた値でも通知が成功した。NULL、存在しない ID、-1、プロセス ID は INVALID_CID。別プロセスの suspended thread は ACCESS_DENIED。
- 終了した worker のハンドルを保持している時点では通知が SUCCESS、閉じた直後の観測では INVALID_CID。ID の寿命に影響するため、互換実装が自身で持つスレッドハンドルを無期限に残さない設計が必要。この単発観測だけで終了後の全タイミングを固定しない。
- 1,000回の短い待機と1,500回の通知を競合させ、ALERTED / TIMEOUT 以外の返却がなく、両結果を実際に観測した。初回ログの x86 は632 / 368、x64 は647 / 353。件数はスケジューリング依存の診断値であり、固定参照値にはしない。

結果とプローブ SHA256: `docs/validation/kxnt-alert-reference.json`。再実行: `tests/build_kxnt_probes.ps1 -Architecture x86` / `x64` → `tests/run_kxnt_alert_reference.ps1 -VMX <VMX> -GuestPassword <パスワード>`。宣言・導入時期: [phnt](https://github.com/winsiderss/phnt/blob/master/ntpsapi.h)、開発者による挙動記述: [wait](https://github.com/m417z/ntdoc/blob/main/descriptions/ntwaitforalertbythreadid.md) / [alert](https://github.com/m417z/ntdoc/blob/main/descriptions/ntalertthreadbythreadid.md)。Windows 8 以降の API である。

この調査を基に、以下のプロセス内状態管理を実装した。

### 実装と VM 検証

- KexDll に NtAlertThreadByThreadId / NtWaitForAlertByThreadId を追加し、既存の未実装本体を削除した。KexDll ordinal 308 / 309、KxNt の Nt / Zw 4名は2208～2211。全4配布 DLL の既存 ordinal は変更0件。
- 各利用スレッドに auto-reset event と実スレッドのハンドルを保持する。1件の保留通知、重複通知の合流、timeout と通知の競合を NT 6.0 のイベント待機に委譲する。待機は non-alertable、SUCCESS を STATUS_ALERTED へ変換する。Hint を逆参照せず、timeout は保留通知の消費前に取り込む。
- TEB 末尾を拡張せず、DLL の thread attach 通知も前提にしない。SRW lock は状態表の操作に使い、無期限待機中は保持しない。既に確認した生存スレッドは保持した実体を利用し、繰り返しの open を避ける。
- 未登録の対象を NtOpenThread / NtQueryInformationThread で確認し、別プロセスは ACCESS_DENIED、不正 ID は INVALID_CID。低位2ビットのタグを除いて扱う。終了したスレッドは次の API 呼び出しで状態・イベント・保持ハンドルを回収してから ID を再照会し、保持ハンドルによって死んだ ID を残し続けない。
- 外部のハンドルによって終了スレッドの ID が残る場合は native 同様 SUCCESS とし、新しい状態を保持しない。明示的 DLL unload では状態を閉じ、プロセス終了時には kernel の回収に任せる。native API が存在する OS では native へ委譲する。LastError を保存する。

イベントの根拠: [Microsoft Event Objects](https://learn.microsoft.com/en-us/windows/win32/sync/event-objects)。本家の keyed event / TEB 状態機械のコピーではなく、Vista の既存の同期機能を使う実装である。

Server 2008 x86 WOW64 / x64 の KxNt → KexDll を Nt / Zw 双方で実行し、ホスト native と固定結果が一致した。参照選択の PowerShell 5.1 の配列処理を修正し、アーキテクチャごとに参照1件であることも検査する。

- 待機前の通知、15回の重複、自スレッド・他スレッド、無期限待機、0 / 相対 / 過去の絶対 timeout、不正 timeout と保留通知維持、APC 非実行、ID タグ、不正 ID、別プロセス。
- Nt / Zw それぞれ、1,000待機と1,500通知の競合。ALERTED と TIMEOUT の両方を観測し、その他の返却0件。
- Nt / Zw それぞれ4,096スレッドを生成・終了。新しいスレッドが古い通知を受け取らず、自分への通知を受け取ることを確認。初期256回では再利用が観測できなかったため4,096回へ拡大し、実際の ID 再利用を両アーキテクチャで多数観測した。再利用件数はログの診断値に保存し、固定件数との比較はしない。計測ハンドル増加0。
- 通知を消費せず終了したスレッドも、次の API 呼び出しで回収され、計測ハンドル増加0。
- 別の検証プロセス内で worker を無期限待機へ進ませ、100ms後に強制終了。残るスレッドの API が TIMEOUT を返し、回収後のハンドル増加0。親の10秒 watchdog を用意した。これは任意の命令位置での強制終了を安全にしたという意味ではなく、メモリ確保・状態表のロック保持中の強制終了は検証していない。[Microsoft の TerminateThread の制約](https://learn.microsoft.com/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminatethread)。

### 配備差と残る範囲

WOW64 の検証親から子を作る試験では、公開名不足の終了6と検証 DLL の LoadLibrary エラー127を観測した。親の DLL 読込みで有効になる伝播と、旧 system DLL の混在を切り分けるため、終了試験のプロセスを VMware Tools から直接起動するようにした。強制終了試験を省略したものではなく、Nt / Zw 双方の結果を別ログで必須確認する。既存 system 配備は更新していない。新 DLL を system 配備した親子起動全体の回帰と、この混在エラーの詳細解析は未完了。

未登録の対象には THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE での open が必要。特殊な制限 DACL、標準ユーザー / 制限トークン、Vista クライアント、native 32bit OS、実アプリ、任意位置の強制終了・他モジュールが DLL を使用中の unload は未検証。終了状態は次の API 呼び出し時に回収するため、呼び出しが止まった時点で直ちにすべての状態を解放する設計ではない。

VM ログ・DLL / プローブ SHA256・直接起動した終了試験のログ: `docs/validation/kxnt-alert.json`。ホスト参照と VM native の不在確認: `docs/validation/kxnt-alert-reference.json`。再実行: `tests/build_kxnt_parity.ps1` → `tests/run_kxnt_alert_reference.ps1` → 参照 JSON を更新 → `tests/run_kxnt_processor_feature_vm.ps1 -Probe alert`。runner は基本試験を CoreOnly モードで実行し、終了試験を別プロセスで実行して双方をゲートにする。単体診断の `--provider` / `--termination` モードも用意した。

## 2026-10-03: ConDrv の実装前検証

本家の `ntcondrv.c` / `dllmain.c` を確認した。`.buildid` セクションで Zig を判定し、アプリ専用フラグを有効にした場合だけ、型名 Console のハンドルへ同期 NtWriteFile を WriteConsoleA に変換する。イベント / APC / Key が指定された経路は native のまま。現行 Vista 版にはこのフラグ・検出・置換経路がない。`docs/inno-content-profile.md` の Qt / Godot / Zig の記述は本家の説明であり、Vista 版に実装済みという意味ではない。

`tests/kxnt_console_classification_probe.c` と VM runner を追加した。Server 2008 x86 WOW64 / x64 の native ntdll と kernel32 のみを使い、KexDllLoaded=0 を確認。診断 EXE の直接実行は再起動後も成功するため、cmd.exe の終了1はこの検証を妨げない。ファイル転送とプロセス取得も成功した。一方 MCP guest_run / guest_ls は vmcli のファイル・引数エラーになるので、VMTools の vmrun 経路を使用した。REST API をタスクスケジューラへ移したことを原因とは断定しない。

### 実測した制約

- Vista の stdout は Win32 コンソールとして有効でも、NtQueryObject は Key と報告した。作成した screen buffer も Directory / File、入力は Directory / File だった。Console 型名を検査する本家の方法では識別できない。
- 書込み専用 screen buffer は GetFileType=CHAR / VerifyConsoleIoHandle=TRUE / WriteConsoleA 成功だが、GetConsoleMode は ERROR_INVALID_HANDLE。GetConsoleMode のみの判定では正常な出力を落とす。NULL に VerifyConsoleIoHandle が TRUE を返すことも観測したため、この非公開関数の結果だけでも不足する。
- 所有する使い捨てファイルハンドルに下位タグ3を付けても、native NtWriteFile はそのファイルへ成功した。これは不正ハンドルとして扱えない。
- さらに screen buffer を追加作成し、タグ付きファイルと同じ数値の有効なコンソールを得た。NtQueryObject は File、GetConsoleMode 成功、GetFileType=CHAR、VerifyConsoleIoHandle=TRUE。同じ値への NtWriteFile は使い捨てファイルへ N を追記し、WriteConsoleA はコンソールへ C を出力した。ファイルは NN の2バイトとなった。この数値衝突を両アーキテクチャで runner の必須観測として検査した。
- 数値タグ・Win32 console の判定だけで NtWriteFile を全アプリに変換すると、本来のタグ付きファイルの書込み先を変更する。native 書込みを先に試してから fallback する方法も、別ファイルへの書込みを成功させてしまう。実アプリの呼出し経路と明示的な適用条件を確認してから実装する。

NT コンソールとカーネルの名前空間を区別し、意図が曖昧な呼出しの方針を決める必要がある。今回、所有確認できない native オブジェクトと数値が衝突する console への NtWriteFile はプローブで実行しない。実行した衝突書込みはすべてプローブ所有の使い捨てファイルに限定した。通常ファイル、パイプ、不正ハンドル、NULL、stdout / stdin、読み書き用 / 書込み専用 screen buffer を観測した。

証跡: `docs/validation/kxnt-console-classification.json`（生ログ、プローブ SHA256、VMX）。再実行: `tests/build_kxnt_probes.ps1 -Architecture x86` / `x64` → `tests/run_kxnt_console_classification_vm.ps1 -VMX <VMX> -GuestPassword <パスワード>`。Result=MEASURED は制約の測定であり、ConDrv 互換実装の成功を意味しない。DLL / Installer / システム配備は変更していない。

公開仕様: [Microsoft Console Handles](https://learn.microsoft.com/en-us/windows/console/console-handles)、[GetConsoleMode の必要権限](https://learn.microsoft.com/en-us/windows/console/getconsolemode)。非公開 VerifyConsoleIoHandle の観測を公式な将来保証とは扱わない。

### Zig の対象選択

公式 Zig 0.15.2 の Windows 配布を取得し、公式 download/index.json の SHA256 と一致することを確認した。配布内 std/os/windows.zig の WriteFile は kernel32.WriteFile を呼び、通常の console 出力に直接 NtWriteFile を使う根拠にはならない。NEXT が対象とする、より新しい NT I/O 経路を調べている。ダウンロード・静的調査だけを実アプリの VM 実行成功とは扱わない。部分書込み・コードページ・非同期・不正バッファ・実アプリはまだ未検証。

公式 Zig 0.16.0 を追加取得し、公式 archive SHA256 `68659eb5f1e4eb1437a722f1dd889c5a322c9954607f5edcf337bc3684a75a7e` と一致を確認。配布内 `lib/std/Io/Threaded.zig` の `fileWriteStreamingWindows` は同期 console 出力で NtWriteFile を呼ぶ。`tests/kxnt_zig_console_smoke.zig` はこの標準 I/O を使い、手書きの syscall 再現ではない。x86-windows.vista / x86_64-windows.vista、ReleaseSafe でビルドした（PE の OS / subsystem version 6.00 も確認）。`.buildid` と NtWriteFile のインポートを確認した。

`tests/kxnt_native_launch_probe.c` から VM 内で実際にこのプログラムを起動した。両アーキテクチャとも CreateProcess 成功、子の終了 `c0000139` (ENTRYPOINT_NOT_FOUND)。native ntdll の NtAlertThreadByThreadId / NtWaitForAlertByThreadId、RtlQueryPerformanceCounter / Frequency、RtlGetSystemTimePrecise、RtlReportSilentProcessExit は公開されていない。スレッド API と SystemTimePrecise は作業ブランチの KxNt で提供済みだが、PerformanceCounter / Frequency と ReportSilentProcessExit の現行転送は native を参照している。この既存未解決転送を先に評価してから、標準 I/O の実行トレースへ進む。今回の欠落一覧は依存関係検査であり、どのエントリが loader の最初の失敗になったかを dump で確定したものではない。

結果・EXE / ソース / コンパイラ SHA256: `docs/validation/kxnt-zig-native-reference.json`。再実行: 両アーキテクチャのプローブをビルド → `tests/run_kxnt_zig_native_reference.ps1 -ZigExecutable <公式0.16.0のzig.exe> -VMX <VMX> -GuestPassword <パスワード>`。テストのコンパイラ版は再現のため固定し、製品へのファイル名・版の固定選択は実装していない。

また同じ native launch probe から `cmd.exe /d /c exit 0` を CreateProcess の明示的な command line で起動すると終了0になった。vmrun の直接起動では終了1だったため、cmd.exe 自体が動作しないとは結論しない。引数伝達・起動経路の差を含めた詳細原因は未確定。必要なゲストコマンドはこのネイティブ起動プローブを使って実行可能になった。証跡: `docs/validation/kxnt-cmd-native-launch.json`。

## 2026-10-03: RtlQueryPerformanceCounter / RtlQueryPerformanceFrequency

ConDrv の実アプリ検証用にビルドした Zig 0.16.0 標準 I/O プログラムが、両関数を直接インポートすることを確認した。現行 KxNt のエクスポートは存在するが転送先の native ntdll は Server 2008 の両アーキテクチャで非公開だったため、既存未解決転送の実装を先行した。

### 実装

- `KexDll/kexrtl.c` に両関数を追加。Vista にある QueryPerformanceCounter / Frequency で値を取得する。実際の OS のカウンタ・周波数を使い、壁時計や固定値で代用しない。VM は14,318,180Hz、ホストは10,000,000Hzで、別々の OS の値を混ぜない。
- 整列したローカル LARGE_INTEGER へ照会してから8バイトの出力を書き込む。Vista の Win32 API に呼出し元ポインターを直接渡すと、未整列ポインターを STATUS_DATATYPE_MISALIGNMENT として拒否する。一方ホスト native RTL は未整列出力を扱い、不正出力先にはアクセス違反を起こす。この差を事前測定し、単純な Win32 転送を避けた。
- 返却は32bitの LOGICAL。正常時1、元の照会が失敗した場合0。BOOLEAN の1バイト返却と混同しない。LastError と LastStatus は正常時・出力コピーでの例外時とも finally で維持する。不正出力の例外を FALSE に変換して隠さない。
- KexDll ordinal 310 / 311 で公開。KxNt の既存 RtlQueryPerformanceCounter / Frequency 転送だけを変更し、既存 ordinal 1094 / 1095、1114 / 1115 を保持。共通宣言を追加。新しいプロジェクトファイルや DLL 書換えルールは不要。

存在・意味の根拠: [phnt の LOGICAL 宣言](https://github.com/winsiderss/phnt/blob/master/ntrtl.h)、[Microsoft QueryPerformanceCounter](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter)、[QueryPerformanceFrequency](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancefrequency)。Win32 の両 API 自体は Windows 2000 以降に存在する。RTL の両エクスポートは VM で存在しないことを別途測定した。

### 検証

VS2010 / SDK 7.1 の x86 / x64 ビルド成功。`tests/kxnt_performance_probe.c` をホスト native ntdll と VM の実際の KxNt → KexDll 経路で実行し、両アーキテクチャとも失敗0。計測値・経過時間などの環境依存行を除き、VM runner がアーキテクチャごとの native 出力と固定行を照合する。

- 各関数の出力先を0～7バイトずらした計16ケース。返却値は32bitの1、8バイト以外の前後のガード値は不変。周波数は同じ OS の QueryPerformanceFrequency と完全一致。カウンタは同じ OS の前後の QueryPerformanceCounter の間に入る。
- 各関数で NULL、アドレス1、PAGE_NOACCESS、4バイトでガードページに跨がる出力、readonly 出力の計10ケース。native 同様 ACCESS_VIOLATION、LastError と LastStatus が維持される。例外直前の部分書込みバイトの順序までは照合していない。
- 4スレッド各1,000反復。カウンタ計4,000回、周波数計4,000回。各回で値の照合・返却値・LastError・LastStatus を確認。タイムアウトせず、worker 終了0。ハンドルリークの専用計測はこのプローブでは実施していない。
- 30ms の Sleep 前後でカウンタが進み、周波数で換算した経過が10ms以上。各環境の実測値を診断行に記録し、特定の経過値との完全一致は要求しない。
- Server 2008 の native RTL は未公開（診断終了5）、Win32 への単純転送は上記の pointer / error 契約に一致しない（予定した比較失敗・終了1）。これらは互換実装の失敗とは別の対照結果。Win32 の不正 counter 出力は例外ではなく LastError 998 となる。
- 全4配布 DLL の既存 ordinal 変更0。Installer の各 DLL の SHA256 が VM で検証したビルドの値と一致することを確認した。

証跡: `docs/validation/kxnt-performance.json`（VMログ、DLL / プローブ SHA256）、`docs/validation/kxnt-performance-reference.json`（host native、VM の native 不在と Win32 の差）、`docs/validation/kxnt-export-ordinals.json`。再実行: `tests/build_kxnt_parity.ps1` → `tests/run_kxnt_performance_reference.ps1 -VMX <VMX> -GuestPassword <パスワード>` → 参照 receipt を docs/validation に保存 → `tests/run_kxnt_processor_feature_vm.ps1 -Probe performance -VMX <VMX> -GuestPassword <パスワード>`。診断終了コードを無視せず、参照 runner 内で予定した5 / 1を明示的に照合する。

Vista クライアント、native 32bit OS、標準ユーザー、実アプリ全体の回帰、OS の時計照会自体が失敗する状況は未検証。システム DLL の配備と Releases 公開は行わず、作業ブランチの Installer と専用 VM フォルダーに配置した。Zig 標準 I/O プログラムには RtlReportSilentProcessExit 等の依存が残るため、今回の API 単体成功を Zig や ConDrv 全体の起動成功とは扱わない。

## 2026-10-03: Zig の残る依存と終了監視の扱い

`tests/kxnt_import_resolution_probe.c` を追加し、Zig 標準 I/O プログラムの全インポートを現在の KxNt 経由で実際に GetProcAddress した。x86は61、x64は63インポート。性能カウンタ修正後に残る未解決名は両アーキテクチャとも RtlReportSilentProcessExit の1件だけだった。プローブは自作の診断 EXE を SEC_IMAGE で直接マップし、実行やディスクの編集はしない。最初の DONT_RESOLVE_DLL_REFERENCES 方式では KexDll のロード通知が x86 マップのインポート名を書き換えたため、純粋な SEC_IMAGE マップへ修正し、元の ntdll.dll / KERNEL32.dll 名を使って再測定した。

### 移植の判断と限定した入口

Zig 0.16.0 の `childKillWindows` は RtlReportSilentProcessExit の返却値を無視してから NtTerminateProcess を呼ぶ。通常の標準 I/O だけのプログラムでも backend vtable を通じて静的インポートに含まれ、ロード時の欠落で先に終了する。

[Microsoft の終了監視仕様](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/registry-entries-for-silent-process-exit) は Windows 7 以降の WER / SilentProcessExit 設定を前提にする。Vista に同じサービス・プロトコルはなく、本家の native 転送のコピーでは成立しない。**終了監視・レポートの実機能は移植完了と扱わない。** 成功するだけの stub を作らず、任意のプロセスを終了させたり、監視設定を書き換えたりもしない。

- KexDll に KexRtlReportSilentProcessExit（ordinal312）の入口を追加。KxNt の既存 ordinal1136 / 1155 を保持したまま転送先を変更した。
- native の API がある OS は、その API に処理を委譲する。native 委譲前の関数探索で変化する LastError / LastStatus を復元し、native 自体の結果を変更しない。
- NT 6.0 では NULL は INVALID_PARAMETER、無効・閉じたハンドルは INVALID_HANDLE、Thread / Event 型は INVALID_PARAMETER。NtQueryObject の型情報で Process であることを確認するため、権限ゼロのプロセスハンドルも分類できる。
- 有効な Process ハンドルには STATUS_NOT_SUPPORTED を返す。**報告成功の代用ではなく、呼出し元が明示的な未対応結果を扱えるようにする ABI の入口。** レジストリ・WER 報告・プロセス状態は変更しない。NT 6.0 の入口は LastError / LastStatus を保持する。

今後、終了監視を実際に要求するアプリに対応する場合は、WER への通知または独立した監視基盤の設計と検証が必要。今回のバインド成功だけを機能対応数に加えない。

### 検証

`tests/kxnt_silent_exit_probe.c` でホスト native と Server 2008 x86 WOW64 / x64 の入口を比較した。NULL、不正、self、thread、event、実 self、closed、権限ゼロ self、QUERY_LIMITED self の9ケース。不正ハンドルの NTSTATUS / 例外なし / LastError は native に一致し、有効なプロセス4ケースは NT 6.0 の方針として SUCCESS ではなく NOT_SUPPORTED を要求した。自プロセスが STILL_ACTIVE のままであることも確認。

NT 6.0 の有効な Process の未対応返却を1,000回反復、計測ハンドル増加0。cold 起動時の全資源の作成スタックまで分類したものではない。native の正常プロセスに対する SUCCESS は参照値として取得したが、ホストでの WER レポート実作成の検証は行っていない。LastStatus の専用照合はこのプローブにはまだない（実装で保存・復元する）。

Zig の全インポートも再検査し、両アーキテクチャで未解決0になった。KxNt / KexDll は VM 専用フォルダーの新しい DLL、kernel32 は system の DLL であることをログのパスで確認した。これだけではプログラムの動作成功にならない。

証跡: `docs/validation/kxnt-silent-exit.json`（native / VM 生ログ、インポート解決、DLL / プローブ / Zig EXE SHA256）。再実行は `tests/build_kxnt_parity.ps1` → `tests/run_kxnt_silent_exit_vm.ps1 -VMX <VMX> -GuestPassword <パスワード>`。Zig のテスト EXE は事前に `tests/run_kxnt_zig_native_reference.ps1` でビルドする。性能カウンタの x86 / x64 回帰試験を再実行し成功、全4配布 DLL の既存 ordinal 変更0も再確認した。

### ConDrv の実際の失敗へ到達

`tests/prepare_kxnt_private_image.py` で自作の Zig 診断 EXE の別コピーだけを用意し、ntdll の import descriptor を KxNt.dll へ変更した。元 EXE、ユーザーのアプリ、IFEO、システム配備は変更していない。この私設ロード経路は通常の VxKex 有効化経路全体の回帰試験の代わりではない。

VM の native launch probe から x86 / x64 のコピーを実際に起動すると、元の ENTRYPOINT_NOT_FOUND ではなく子の終了1になった。さらに x64 の CDB 6.12 で診断コピーを起動し、最初の互換性関連ブレークを捕捉した。

- 新しい専用 KxNt / KexDll がロード済み。
- NtWriteFile の入口で RCX=7（標準出力）、RDX=0（イベントなし）、Length=0x28（40バイト）。Zig の標準 I/O 呼出しスタックを記録した。
- `gu` で関数から戻るまで実行し、RAX=c0000024（STATUS_OBJECT_TYPE_MISMATCH）を取得。先の Vista stdout が NT 側で Key と見える測定と整合する。
- RtlReportSilentProcessExit にも breakpoint を設定し、最初に止まったのは NtWriteFile。戻り値取得後は全 breakpoint を解除して終了させたので、終了後まで report 関数が一切呼ばれないことをこのログだけで主張しない。

証跡: `docs/validation/kxnt-zig-console-trace.json`（元と私設 EXE のハッシュ、CDB コマンド、CP932で読んだ生ログ、debugger 終了ログ）。拡張 DLL と PDB が不足する警告はあるが、native export の breakpoint・レジスタ・戻り値の取得は成功した。コンソール表示や文字符号の成功はまだ確認していない。次はこの実際の呼出しに対し、適用プロファイルとコンソール判定を実装する。

## 2026-10-03: NT 6.0 の同期コンソール書込み

先の実 Zig 標準 I/O の NtWriteFile → OBJECT_TYPE_MISMATCH を解消した。NtWriteFile 自体がないのではなく、NT 6.0 の独立した console handle を NT の File として扱えない問題への適応である。ConDrv ドライバー全体、非同期 console I/O、NtClose の置換は実装していない。

### 実装・適用条件

- `00-Common-Headers/ZigNtIoProfile.h` で mapped PE の内容を判定する。読み取り可能な `.buildid` と、NTDLL / 書換え後の KxNt の NtWriteFile・NtWaitForAlertByThreadId・RtlReportSilentProcessExit の import signature を要求する。製品名、版、固定 RVA を使わない。これは Zig 系の NT-I/O 経路の保守的なプロファイルであり、コンパイラの真正性の証明ではない。他の compiler が同じ内容を持てば対象となる。
- DOS / NT / optional / section / import headers、各 RVA・長さ・終端を減算による範囲検査で扱い、PE32 / PE32+ 双方に対応。Main EXE の判定結果を atomic に cache する。NT 6.0 以外と DisableAppSpecific 有効時は native に委譲する。cache が有効でもオプションの無効化を優先する。
- `KexDll/ntcondrv.c` に Ext_NtWriteFile（ordinal313）。KxNt の Nt / Zw の既存 ordinal573 / 598、1821 / 1848 を維持して転送先だけ変更した。通常の File / Pipe には Vista にある native NtWriteFile を呼び、Windows 7 向けの固定 syscall 番号は使わない。
- console の判定は NULL 除外、下位タグ、GetFileType=CHAR、VerifyConsoleIoHandle の併用。GetConsoleMode だけでは拒否される書込み専用 console も扱う。さらに NtQueryObject の型を調べ、File と console の両方で同じ数値が有効なら **I/O 前に NOT_SUPPORTED**。どちらかへ試し書きして判定しない。予期しない型照会失敗も書込みを推測しない。
- console の Event / APC / Context / Key 付き要求は NOT_SUPPORTED。通常 File の Event 付き要求はそのまま native に渡す。console offset は NULL、0、現在位置 / EOF の sentinel を扱い、別の seek 位置は INVALID_PARAMETER。
- IOS の全範囲を I/O 前に検査し、入力を heap に捕捉してから文字コードを変換する。例外は NTSTATUS にし、LastError / LastStatus を維持する。ユーザーメモリーを他スレッドが同時に解放する競合を完全に防ぐものではない。
- 現在の console output CP で UTF-16 に変換して WriteConsoleW を呼ぶ。UTF-8 と通常の SBCS / DBCS に対応。UTF-7、MaxCharSize>2（UTF-8以外）、使用できない変換フラグは明示的に未対応。不正な文字列は ILLEGAL_CHARACTER とし、欠落や置換を成功として隠さない。入力が INT_MAX を超える場合と不正 offset は拒否し、確保できない場合は NO_MEMORY。
- IOS.Information は **入力のバイト数**。部分完了では UTF-16 の文字数を UTF-8 / DBCS の消費済みバイト境界へ戻す。途中の surrogate だけが書かれた場合は成功とせず、ILLEGAL_CHARACTER と完全な文字までの消費バイト数を返す。このケースでは console に部分的な副作用が既にあるため、原子的な書込みは保証しない。
- 既に KxBase がロードされていれば、その WriteConsoleW を使って既存の VT 処理と連携する。参照を取得し finally で解放して unload race を防ぐ。plain output のために KxBase を新しくロードする依存は追加しない。

仕様参照: [WriteConsole](https://learn.microsoft.com/en-us/windows/console/writeconsole)、[Nt / ZwWriteFile の bytes 契約](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwwritefile)、[Vista の文字変換・不正入力とコードページの制約](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar)。非公開 VerifyConsoleIoHandle の結果は公開された将来保証とは扱わない。

### Server 2008 での検証

VS2010 / SDK の x86 / x64 ビルド成功。`tests/kxnt_condrv_probe.c` と runner を追加し、両アーキテクチャの KxNt → KexDll の実経路を実行、失敗0。設定はプローブ内の共有データだけで操作し、ユーザーのレジストリを変更しない。

- 実 Zig 0.16.0 std.Io プログラムの私設コピーが、両形式で終了0。専用 screen buffer から本文の **全39文字**、File / Pipe から newline を含む **全40バイト**を読み取り照合した。診断 EXE の import を別コピーで KxNt に向けた検証であり、通常 IFEO / ダブルクリック起動全体の証明ではない。
- CP437 ASCII、CP932 の日本語、CP65001 の日本語・surrogate pair 入力。Nt / Zw の返却 byte count と TLS、native WriteConsoleW と同じ console cells を確認。ただし CP65001 / raster font の native 出力自体が Unicode を正しく描画しない例を観測した。**native 参照への一致を日本語・絵文字の表示成功とは扱わない。** console の描画基盤の改善は残る。
- プロファイルの PE32 / PE32+ fixture 各12ケース。短い image、header / section / thunk の範囲逸脱、欠落 import、別 DLL の同名 import、終端のない descriptor を拒否。実プローブの build-id 名をプロセス内で変更した別起動でも native 委譲を確認。
- 書込み専用 screen buffer（GetConsoleMode 失敗）成功、通常 File / Pipe の内容一致、下位タグ付きの有効な File への書込み、Event 付き File の native 結果・IOS・signal の一致。APC の実 callback / PENDING の非同期パイプは未検証。
- NULL IOS、アドレス1、不正入力、readonly IOS、入力 / 出力の guard page、長さ0、未知の offset、不正 UTF-8、console 非同期要求。書込み前に拒否するケースで console 内容が不変。部分的な IOS probe 自体や他スレッドによる解放の競合は別の制約。
- 同じ VM の native File 書込みで IOS の0～7バイトずらしを測定し、全 offsets が成功。互換 console 書込みも Nt / Zw 各8 offsets で status・byte count・構造体前後の canary を照合した。x64 の union の未使用部分まで native のバイトパターンと等しいという主張はしない。
- 所有する File と console の数値衝突を生成し、Nt / Zw とも NOT_SUPPORTED、File の長さと console 内容は不変。これは曖昧な場合の安全な拒否であり、その console が使用可能になることまで保証しない。
- 所有する診断 KexDll の WriteConsoleW import だけに部分完了を注入。UTF-8 は2 UTF-16 unitsで4 bytes、DBCS は2 unitsで3 bytesを返す。DBCS の実部分出力内容、UTF-8 の変換入力、surrogate 分割時の明示的エラーを照合。**実 OS が自然に部分完了した測定ではない。** raster font の UTF-8 部分描画は参照 API 自体でも不安定で、完全な描画成功の主張はしない。
- 4 worker ×1,000回を2フェーズ、各回の status / IOS / TLS を照合。初回は新しい Event が1個増えた。互換書込みを使わず同じ Win32 / NT 補助 API を呼ぶ対照でも同じ Event の増加1。次の4,000回は増加0、handle値・object・type・access・flags の snapshot も一致した。runner は冷間増加が対照と一致することも要求する。Event の作成 stack は未特定であり、cold 起動資源の完全な原因特定とは扱わない。対照は KexDll を読込み済みだが、並行 worker は互換書込み関数を呼ばない。
- KxBase の SetConsoleMode で VT を有効化し、NtWriteFile 経由の色指定 / reset を含む10 bytes が制御文字として処理され、本文 R を表示することを確認。KxBase の SHA256 も receipt に記録した。
- 性能カウンタの x86 / x64 回帰成功。全4配布 DLL の既存 ordinal 変更0、Installer の4 DLLは検証ビルドと同一。

証跡: `docs/validation/kxnt-condrv.json`（VM 生ログ、対照、source / DLL / EXE SHA256、明示した制約）、`docs/validation/kxnt-export-ordinals.json`。再実行は `tests/build_kxnt_parity.ps1` → Zig 診断 EXE をビルド・私設コピーを用意 → `tests/run_kxnt_condrv_vm.ps1 -VMX <VMX> -GuestPassword <パスワード>`。私設コピーの用意は前節の `tests/prepare_kxnt_private_image.py` を使う。

今回の配備先は作業ブランチの Installer と VM の `C:\KxNtParity`。システム DLL や Releases は変更していない。Vista クライアント、native 32bit OS、標準ユーザー、通常の IFEO 有効化経路、全コードページ、native での真の部分完了、非同期 console I/O、UTF-8 描画基盤の改善は未検証 / 未対応。移植フェーズ全体は継続中。

## 2026-10-03: SRW ロックの試行取得

本家と現行 KxNt は RtlTryAcquireSRWLockExclusive / Shared を native ntdll に転送していたが、Server 2008 の WOW64 / x64 双方で転送先が未公開だった。SRW の通常の取得・解放は Vista に存在する。公開 Win32 の [TryAcquireSRWLockExclusive](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-tryacquiresrwlockexclusive) / [Shared](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-tryacquiresrwlockshared) は Windows 7 / Server 2008 R2 以降。

### 実装

- `KexDll/srwtry.c` に BOOLEAN / NTAPI の2入口を追加（ordinal314 / 315）。KxNt の既存 ordinal1228 / 1229（x64）、1246 / 1247（x86）を維持して転送先だけ変更した。KxBase の既存 Win32 実装は変更していない。
- native API がある OS は native に委譲。遅延探索・atomic cache の初期化で変化する LastError / LastStatus を復元し、native 自体の結果を変えない。native がない場合は実 OS バージョン NT 6.0 のみに限定し、未知の形式を推測して使用しない。
- NT 6.0 の native 共有所有者数（0x11 / 0x21）、排他所有（1）、待機者フラグを実測して実装。排他は空状態への compare-exchange、共有は待機者・排他・未知の状態を拒否して所有者数を compare-exchange で追加する。共有数の桁溢れを拒否する。OS の待機者リストを作成・変更しない。
- 失敗は FALSE を返し、blocking acquire を呼ばない。共有の再試行は他スレッドが word を変更した場合のみ行う。競合下での時間上限を数学的に保証する wait-free 実装ではない。
- NULL / guard / readonly storage のアクセス違反は成功や FALSE に置換しない。初版では共有 word の読取りに通常 load を使ったが、readonly の排他所有 word でも native はアクセス違反となることを両アーキテクチャの参照で確認したため、atomic read に修正した。SRW は書込み可能で自然整列した storage が前提。壊れた lock、所有権違反、再帰取得の対応を保証しない。

### 検証

VS2010 / SDK の x86 / x64 ビルド成功。`tests/kxnt_srw_probe.c` をホスト native ntdll、ホスト KxNt → KexDll の native 委譲、Server 2008 の実 KxNt → KexDll 経路で実行し、失敗0。環境に依存する path を除いた固定ログを runner で比較した。

- 空状態、排他所有中、複数共有所有者、native shared acquire と試行取得の混在。native release 後の空状態を確認。
- shared owner → exclusive waiter、exclusive owner → shared / exclusive waiter の3ケース。待機者が実際に enqueue されてから双方の try が FALSE、word 不変、native release 後に worker が取得・解放して終了することを確認。
- 各関数で NULL、アドレス1、PAGE_NOACCESS、guard に跨がる storage。readonly の空・排他・共有の各状態。すべて参照と同じ ACCESS_VIOLATION、結果 sentinel 不変、LastError / LastStatus 維持。
- 試行取得した exclusive / shared lock を native SleepConditionVariableSRW が解放・再取得できることを確認。別スレッドが排他取得して値を書き換え、WakeConditionVariable で正常復帰。
- 4 worker が native blocking と try を交互に使い計4,000回取得。2,000回の更新、共有読取りの整合性、同時 writer / reader の排除、worker 終了、最終空状態、計測ハンドル増加0を確認。永久競合、強制終了中の回収、全ての scheduler interleaving を証明したものではない。
- VM native は2 export が不在、診断終了5を意図した対照として要求。owned probe には15秒 watchdog があり、停止時は自分のプロセスだけ終了する。ユーザーのアプリを終了させない。
- 同じビルドで実 Zig 標準 I/O / ConDrv と性能カウンタの両アーキテクチャ回帰成功。全4配布 DLL の既存 ordinal 変更0、Installer は検証ビルドと同一。

証跡: `docs/validation/kxnt-srw.json`（native / VM 生ログ、source / DLL / EXE SHA256）、更新した `kxnt-condrv.json` / `kxnt-performance.json` / `kxnt-export-ordinals.json`。再実行: `tests/build_kxnt_parity.ps1` → `tests/run_kxnt_srw_vm.ps1 -VMX <VMX> -GuestPassword <パスワード>`。

今回も VM の `C:\KxNtParity` と作業ブランチの Installer の配備。Vista クライアント、native 32bit OS、標準ユーザー、通常 IFEO 経路全体の統合検証は残る。SRW の追加を KxNt 全体の移植完了とは扱わない。

## 2026-10-03: UTF-8 / UTF-16 変換の native 契約測定

次の既存未解決転送2件に向け、実装前の比較基準を追加した。両関数は本家でも native 転送であり、単純なコピーで Vista 向けにはならない。[UTF8ToUnicodeN](https://learn.microsoft.com/en-us/windows/win32/devnotes/rtlutf8tounicoden) / [UnicodeToUTF8N](https://learn.microsoft.com/en-us/windows/win32/devnotes/rtlunicodetoutf8n) の公開要件は Windows 7 / Server 2008 R2 以降。Server 2008 の x86 / x64 native ntdll で両 export 不在を再測定した（診断終了5を明示的に照合）。**互換実装はまだ追加しておらず、VM での変換成功とは扱わない。**

`tests/kxnt_utf8_probe.c` と `tests/run_kxnt_utf8_reference.ps1` を追加。ホスト native で各754呼出しを実行し、x86 / x64 の status、actual byte count、全32出力バイト、例外、LastError / LastStatus を含む固定行が一致した。出力容量外の canary と TLS の維持をプローブ自身でも確認した。

- UTF-8: ASCII / 埋込みNUL、日本語、2〜4 byte文字、最大 scalar、継続 byte 単独、途中で切れた列、overlong、surrogate符号化、上限超過、不正lead、途中にASCIIがある不正列。UTF-16: 同様の有効文字、単独high / low surrogate、不正pair、最大pair、奇数入力長。
- 全入力に出力容量0〜24 bytes、NULL出力によるサイズ照会（容量0 / 1）、actual count無し、出力とcount両方NULLを測定。NULL source の長さ0 / 非0も含む。
- 有効な UTF-8 の4 byte文字で UTF-16出力に2 bytesしか残らない場合、native は high surrogate まで書き、BUFFER_TOO_SMALL と実出力 bytes を返す。一方 UTF-16 → UTF-8 は出力文字の bytes 全体が入る容量まで書かない。この方向差を検出した。
- 不正文字の置換単位は不正 byte 数と常に同じではない。例として ED A0 80 の surrogate 符号化は2つの U+FFFD、F4 90 80 80 は3つに置換された。切れた有効prefixや途中のASCIIを含む列も別の消費単位になる。
- UTF-16の奇数byte長は出力ありでは INVALID_PARAMETER_5、NULL出力による照会では完全なWCHAR分だけ数える。NULL source は長さ0でも INVALID_PARAMETER_4、検証エラー時は actual count の sentinel が維持される。

これらはホストの特定の native 実装で得た契約測定。Windows 7 の全挙動の証明、全 scalar / 全不正列、guard page / 不正非NULL pointer、overlap、並行変換の検証はまだない。次の実装ではこの基準に加えてそれらを検証する。成功するだけのstubや、変換不能なbyteを黙って削除する処理は追加しない。

証跡: `docs/validation/kxnt-utf8-reference.json`（raw出力とソース / EXE SHA256）。再実行: `tests/build_kxnt_probes.ps1 -Architecture x86` / `x64` → `tests/run_kxnt_utf8_reference.ps1 -VMX <VMX> -GuestPassword <パスワード>`。VMへの追加物は `C:\KxNtParity\x86` / `x64` の診断EXEとログだけ。Installer / システムDLL / Releasesはこの測定では変更していない。

## 2026-10-03: UTF 変換の実装と VM 検証

前節の測定を基に `KexDll/utf8.c` を実装した。KexDll ordinal316 / 317を追加し、KxNtの旧ordinal1231 / 1245（x64）、1249 / 1262（x86）を維持して転送先だけ変更。native API があるOSではnativeに委譲し、関数探索が変える LastError / LastStatusを復元する。native がないOSではheap確保・Win32文字変換・共有mutable文字バッファを使わず、入力を順に変換する。

- UTF-8の1〜4byte文字、UTF-16 surrogate pair、埋込みNULを長さ指定で変換。終端を自動追加しない。無効なUTF-8のprefix消費・U+FFFD置換をnative比較に合わせる。UTF-16の単独surrogateも置換しSOME_NOT_MAPPEDを返す。短い出力を成功にせずBUFFER_TOO_SMALLと実出力bytesを返す。
- decodeはUTF-16のhigh surrogateまで出力する場合があり、encodeは1文字のUTF-8 bytes全体が入らなければその文字を書かない。NULL出力は必要サイズを照会。UTF-16の奇数入力長は実変換ではINVALID_PARAMETER_5、照会では完全なWCHAR分だけ数える。
- NULL sourceは長さ0でもINVALID_PARAMETER_4。非NULLの不正pointerによるアクセス違反を成功statusに置換しない。容量0でも入力を読むnativeの順序をguard試験から修正した。
- encodeのactual countは公開ページのoptional注釈に反し、**測定したnativeではNULLのまま実変換すると出力後にアクセス違反**。decodeはNULLを許容する。この方向差も参照と一致させた。全Windows版の同一挙動を確認した主張ではない。
- 必要サイズがULONGの上限を超える場合はINTEGER_OVERFLOWで拒否する。4GB付近の実バッファ・nativeの桁溢れ動作は検証していない。この追加の境界方針をnativeの完全再現とは扱わない。

### 実行した検証

VS2010 / SDKの両ビルド成功。ホストnative、ホストKxNtからnativeへの委譲、Server 2008専用KxNt → KexDllを比較し、両アーキテクチャで成功。実装DLLのロードパスも専用フォルダーに限定して照合した。

- 前節754呼出しに、seed固定のランダムUTF-8 / UTF-16各1,024入力の変換とサイズ照会を追加し、各4,850呼出しのstatus、byte count、32出力bytes、例外、TLSが一致。容量外canaryも確認した。全不正byte列を網羅したものではない。
- 各方向12pointerケース。アドレス1、PAGE_NOACCESS、容量0、入力長0、count不正、guardを跨ぐsource / count、NULL出力による不正sourceの照会。参照と同じ例外・actual sentinel・最初の4出力bytes・TLSを確認。部分faultの全byte書込み順序の証明ではない。
- **全1,112,064 Unicode scalar value** を連結した入力で、UTF-16全4,321,280 bytesとUTF-8全4,382,592 bytesを変換・往復・サイズ照会。独立したWin32変換をUTF-8の全内容の期待値にし、memcmpで全バッファを照合。nativeでも同じ試験を成功させた。
- sourceとdestinationの位置差-4〜+4の18overlapケース。奇数アドレス出力も含め、nativeとstatus / count / 更新範囲40bytesが一致。重なる領域の内容は入力の破壊に依存するため、任意overlapを正しい文字列への変換として推奨しない。
- 初回と次回の各4 worker ×1,000反復、変換2方向と照会2方向で各16,000呼出し。全workerの内容・count・status成功、次回のハンドル増加0。coldでは一度handleが増えた。変換関数を呼ばずnative GetModuleHandle / GetProcAddressだけを並行実行する別プロセス対照で、同じcold増加を確認しrunnerで両countの一致を要求する。
- リソース観測は終了直後と100ms後を両方記録した。最新x86測定では互換変換と対照のcoldが直後+1、100ms後+6、次回はどちらも0。初期の対照では次回直後に+9も観測しており、直後countだけを安定した最終状態と扱わない。100ms後の比較成功は資源作成stackの帰属や、完全なlifecycle検証の代わりではない。待機による補助APIの初期化も観測対象に含む。**cold資源の作成元と全handle identityは未確認**。
- 同じ配布DLLでSRW・実Zigコンソール書込み・性能カウンタを両アーキテクチャ回帰。全4配布DLLの旧ordinal変更0。Installerは検証DLLと同一。

証跡: `docs/validation/kxnt-utf8.json` と更新した `kxnt-utf8-reference.json`（native / VM / lookup対照 / source・DLL・EXE SHA256）、回帰receipt。再実行: `tests/build_kxnt_parity.ps1` → `tests/run_kxnt_utf8_vm.ps1 -VMX <VMX> -GuestPassword <パスワード>`。参照採取もrunner内で順番に行い、native不在の終了5以外の失敗を無視しない。

作業ブランチInstallerとVM専用フォルダーへの配備。Vistaクライアント、native32bit OS、標準ユーザー、通常IFEO起動、全scheduler interleaving、4GB入力、全overlap、不正pointerの全配置は未検証。システムDLL / Releasesは変更していない。KxNt移植全体は継続中。

## 2026-10-03: backup / restore のトランザクション代替を実測

旧NtCreateKeyをNtOpenKeyExの代わりに使うと、欠落キーを作ってしまう。Vistaのtransaction付きcreateで「既存ならcommit、新規ならrollback」を行い、通常のkey handleを返せるか実測した。[ZwCreateKeyTransactedの仕様](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwcreatekeytransacted) はbackup / restore指定を受理し、[Win32の仕様](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regcreatekeytransactedw) でもcommit後の操作に制約がある。

**transaction-bound handleを直接返す方式は採用しない。** Server 2008のWOW64 / x64双方で、native NtCreateKeyTransactedが返した既存key handleもcommit後に読み書きできなくなった。Win32 transaction HKEY wrapperだけの制約ではない。

### 条件と結果

- tests/kxnt_registry_transaction_probe.cを追加。HKCUのSoftware\VxKexProbe\TxnOpen-PID-tickに自分専用の親、Target、Markerを作る。既存fixtureと衝突したら変更せず中止。Missingという子はtransaction内で作られても必ずrollbackし、既存Targetを開いたときだけcommitする。
- 自プロセスのtokenを複製し、thread-privateなimpersonation tokenのprivilegeなし / Backupのみ / Restoreのみ / 両方を測定。元のprocess tokenや別ユーザーのtokenを変更しない。終了時に元のthread tokenを復元した。
- VMは4状態すべて実行。GrantedAccessはBackup=01020019、Restore=010f0006、両方=010f001f。DesiredAccess=KEY_READがbackup / restore指定で上書きされることを確認した。これを全Windows版の規範的access maskと扱わない。
- privilegeなしのtransaction createは既存・欠落ともACCESS_DENIED。native NtOpenKeyExが欠落キーにOBJECT_NAME_NOT_FOUNDを返した前節の結果と異なり、検証順序の適応も必要になる。
- Backup / 両方では、既存Targetのcommit前のNtQueryValueKeyはSUCCESS、Marker=69133742。commit後はc0190003（TRANSACTION_NOT_ACTIVE）。Restore / 両方のcommit後NtSetValueKeyもc0190003。Restoreのみのreadは権限がなくACCESS_DENIEDなので、writeでtransaction失効を別に確認した。
- rollback後のMissingは通常のRegOpenKeyExでERROR_FILE_NOT_FOUND。全8transactionで確認し、既存Targetの更新日時も不変だった。新規キーのrollback済みhandleはKEY_DELETED等を返し、そのまま通常handleとして使えない。
- 最後にTarget、もし残っていればMissing、専用親fixtureを削除。key / transaction / token handleを閉じ、削除失敗、token復元失敗、timestamp変更は試験を失敗させる。VxKexProbeの共通親やユーザーの既存キーを削除しない。
- ホストfiltered tokenにはBackup / Restore privilegeがないため、privilegeなしだけ実行し、3状態はPrivilegesUnavailableを明示した。ホストnativeのprivilege有効状態との完全な比較ではない。

Native ObjectBasicInformationの固定長56bytesでGrantedAccessを測定した。初期のWin32 transaction HKEYや長さ128bytesでの照会は不適切だったため、native handleと固定長に修正して最終証跡を取得した。試験のPASSはfixture不変性・cleanupとこの失効挙動の確認であり、NtOpenKeyEx代替の成功ではない。

再実行: tests/build_kxnt_probes.ps1を両形式で実行 → tests/run_kxnt_registry_transaction_probe.ps1にVMXとGuestPasswordを指定。証跡: docs/validation/kxnt-registry-transaction.json（native前後結果、privilege対照、fixtureとcleanup、source / EXE SHA256）。

配布API・転送先・システムDLLは変更していない。通常open / OBJ_OPENLINKは引き続き移植候補。backup / restoreを無視した成功や失効したhandleの公開はしない。保護ACL、任意のroot / flags、削除競合、registry virtualization、Vistaクライアントは未検証。他の代替方式すべてが不可能だという結論ではない。

## 2026-10-03: 残る native 転送の再集計とレジストリopenの測定

VMのSystem32 / SysWOW64からnative ntdllを改めて取得し、machine種別を照合して現在のInstallerを検査した。`tests/audit_kxnt_native_forwarders.py` は明示したDLL snapshotと基準コミット24a03aeに対し、公開名、ordinal転送、native named転送先を解析して再現可能なJSONを出力する。対象DLLのSHA256も保存した。

| 形式 | 基準のnamed export数 | 現在 | native未解決: 基準 → 現在 |
|---|---:|---:|---:|
| x86 WOW64 | 2,055 | 2,067 | 175 → 168 |
| x64 | 2,015 | 2,027 | 191 → 184 |

基準の公開名 / ordinal名の削除は0、新たな未解決native named転送も0。減少した7件はPerformanceCounter / Frequency、SilentProcessExit、SRW試行取得2件、UTF変換2件。これは**リンク先が存在することの検査**であり、SilentProcessExitの終了監視機能や、全アプリ・全呼出しの成功を意味しない。残り168 / 184件の全てが必要な移植対象だと判断したものでもない。資料: `docs/validation/kxnt-native-forwarders.json`。元の移植可能性調査は基準時点の記録として維持した。

### NtOpenKeyEx の読み取り専用プローブ

[Microsoft の ZwOpenKeyEx 仕様](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwopenkeyex) でWindows 7以降の追加とOpenOptionsの契約を確認した。`tests/kxnt_open_key_reference_probe.c` / runnerを追加し、ホストnativeのOpenOptionsと旧NtOpenKeyの挙動、およびServer 2008の旧NtOpenKeyを比較した。HKLM SYSTEM、既存のCurrentControlSetリンク、存在しないキー名を読取りで開く試験であり、キーや値を作成・削除・変更していない。privilegeも変更していない。

- Server 2008のWOW64 / x64でNtOpenKeyExは不在（終了5を明示照合）。旧NtOpenKeyでは通常指定でCurrentControlSetのリンク先ControlSet001を開き、OBJECT_ATTRIBUTESのOBJ_OPENLINK（0x100）を付けるとリンク自身CurrentControlSetを開く。実handleをNtQueryKeyで照会して区別し、全handleを閉じた。
- ホストnativeのNtOpenKeyEx options0 / 8と旧NtOpenKeyの属性0x40 / 0x140で、それぞれ通常open / リンク自身openを確認。この基盤ならREG_OPTION_OPEN_LINKを扱える見込みがある。まだ互換関数・転送先変更は実装していない。
- ホストの通常キーにbackup / restore（4 / 12）を指定するとACCESS_DENIED、通常openは成功。欠落キーはどちらもOBJECT_NAME_NOT_FOUND。権限の差と検証順序を無視して旧APIへそのまま転送してはならない。
- ホストの不正options1 / 2 / 0xffffffffはINVALID_PARAMETER_4で出力handleがsentinelのまま。既存キー・欠落キーの通常エラーはhandleを変更した。追加測定では現在のnativeはoptions0x10を受理したが、意味とVistaでの代替は未確認。Windows 7の全options契約を確定したものではない。
- 全試験でLastError / LastStatus維持、取得成功handleの型をキー名照会で確認。通常キー3種類の読み取り・TLSの測定であり、standard user、backup権限を有効化したtoken、保護キー、アクセス拒否ACL、削除競合、不正pointer、相対RootDirectory、WOW64 view指定は未検証。

証跡: `docs/validation/kxnt-open-key-reference.json`（native / legacy / VM rawログ、source・EXE SHA256）。ビルドは両アーキテクチャの `tests/build_kxnt_probes.ps1`、実行は `tests/run_kxnt_open_key_reference.ps1 -VMX <VMX> -GuestPassword <パスワード>`。静的再集計は `tests/audit_kxnt_native_forwarders.py --native-x86 <SysWOW64のsnapshot> --native-x64 <System32のsnapshot>`。今回の追加は診断EXE / 検査ツール / 記録のみで、配布DLLやシステムDLL、Releasesは変更していない。

## 2026-10-03: 空の相対名による backup / restore open と ACL 境界

NtOpenKeyEx の追加検討として、既存キーを native NtOpenKey で先に参照し、NtCreateKey にその handle と空の相対名を渡す方式を測定した。**通常キーでは transaction に結び付かない、読み書き可能な backup / restore handle を取得できた。ただし、最初の参照取得を ACL が拒否するキーには対応できない。全般的な置換としては未採用。**

- Server 2008 の WOW64 / x64 で各96ケース。4 privilege状態 × 通常ACL / protected空DACL / OWNER RIGHTS拒否ACE × 参照時のアクセス0 / KEY_QUERY_VALUE / READ_CONTROL / MAXIMUM_ALLOWED × 既存 / 欠落キー。
- 通常キーと空DACLでは、READ_CONTROLで参照し、空名でbackup / restore createするとdisposition=2。Backupだけは読取り成功・書込み拒否、Restoreだけは逆、両方は読書き成功。成功した読取りのMarker内容も照合。transactionをfinishした後の失効と異なり、その場で有効なnative handleとなった。
- KEY_QUERY_VALUEは空DACLで拒否されるが、所有者には暗黙のREAD_CONTROLがある。単に空DACLの成功だけで任意の保護キー対応とは判断できない。[MicrosoftのOWNER RIGHTS説明](https://github.com/MicrosoftDocs/windowsserverdocs/blob/main/WindowsServerDocs/identity/ad-ds/manage/understand-security-identifiers.md) を確認し、S-1-3-4へのKEY_ALL_ACCESS拒否ACEも追加した。実際に設定されたDACLのACE数0 / 1をnative照会した。
- OWNER RIGHTS拒否では、Backup / Restoreを有効にしても全4参照アクセスがACCESS_DENIED。空名createまで到達しない。この方式は「先に何らかの参照を取得できるキー」に依存する。ゼロアクセスの参照も通常キーでACCESS_DENIEDとなった。
- 欠落キー48ケースはOBJECT_NAME_NOT_FOUNDでcreate未実行。名前付きcreateへフォールバックしていない。既存キーの成功createはdisposition=2を要求した。Markerを書き戻すのは専用fixtureだけであり、その操作後のtimestamp不変を主張していない。transaction側の8 timestamp / 欠落キーrollback検査も再実行した。
- ホストfiltered tokenはprivilegeなしだけを測定し、有効化できない3状態を明示。ホストnativeのprivilege有効状態との同等性は未確認。

### 診断用キーの後始末

初期版ではOWNER RIGHTS拒否後のDACL復元・名前からのRegDeleteKeyが拒否され、ホストに専用fixtureが6個残った。最終版は作成時のDELETE権限付きhandleを保持し、native NtDeleteKeyで専用Targetを削除してから親を削除する。最終実行はホスト / VMの両形式でTarget削除STATUS_SUCCESS、親削除成功、token復元、Failures=0を確認した。DACL復元そのものが拒否された場合も記録し、復元成功と誤記しない。キー全体の削除をcleanupの条件とした。

残った6個は、今回の固定キー名、子がTargetだけ、Marker=69133742を照合する一回限りの管理者清掃helperで削除し、ホストの専用親に子が残らないことを確認した。既存アプリの設定キーは対象にしていない。清掃の要約は docs/validation/kxnt-registry-fixture-cleanup.json。

診断プローブとrunnerを更新。runnerは96件の網羅、48件の欠落拒否、16件のOWNER RIGHTS参照拒否、有効なprivilege別read/write、effective ACL、削除を明示gateにした。証跡は更新した docs/validation/kxnt-registry-transaction.json（source / EXE / runner SHA256とhost / VMのraw結果）。再実行方法は前節と同じ。

VM再接続では専用native-launchからcmd.exe /d /c exit 0を起動し、CreateProcess成功・子終了0を確認した。配布DLL・システムDLL・Releasesは変更していない。任意root、symlink、view、削除競合、非所有者token、通常IFEO、Vistaクライアントの包括検証と、保護キーを扱える代替方式の検討は継続する。
## 2026-10-03: transaction root から通常createで開き直す方式の検証

保護キーの最初の参照をtransaction付きbackup / restore createで得た場合、通常のNtCreateKey（TransactionHandle引数なし）に空の相対名とそのrootを渡して、transactionから独立したhandleを得られるか検証した。**この方式もそのままでは採用できない。** Server 2008のWOW64 / x64とも、rootのtransactionへの結び付きを引き継いだ。

- 通常ACLの専用Targetをtransactionで開いた後、commit前に通常NtCreateKeyで空名を開くとSTATUS_SUCCESS・disposition=2。Backup / 両privilegeではMarkerの読取りも成功し、内容を照合した。
- 元のtransactionをcommitすると、通常createが返したhandleの読取りもc0190003（TRANSACTION_NOT_ACTIVE）となった。通常createを呼んだという事実だけでは独立性を保証しない。
- commit後にtransaction-bound rootから空名createを行うと、Backup / Restore / 両方の3状態すべてでc0190003。戻るhandleの有効性を得られなかった。
- Restoreだけの再openはQUERY_VALUE権限がないため前後ともACCESS_DENIED。これをtransaction終了後の有効性の証明とは扱わない。新しいhandleから書き込みは行っていない。
- transaction内で新規作成されたMissingはreopenのrootとして使わず必ずrollback。marker内容、元Target timestamp、Missing不在、96件のACL/pin matrix、native DELETEによるfixture清掃、token復元を両形式で再確認。ホストfiltered tokenではprivilege有効状態は引き続き未確認。

runnerはcommit前の再open成功・commit後の読取り失敗、commit後create拒否、Missing再open未実行をgateに追加。更新証跡: docs/validation/kxnt-registry-transaction.json。source / EXE / runner SHA256を保存した。配布DLL・native転送先は変更していない。別方式すべての不可能性を証明したものではない。保護キーを誤って作る方式、失効handleを成功として公開する方式へ置き換えない。
## 2026-10-03: Vista クライアントで主要11項目を再検証

停止中だったVista x64 VMを再開し、Vistaユーザーの専用フォルダーC:\Users\Vista\KxNtParityから既存プローブを実行した。device-familyの両形式の記録はReportedVersion=6.0.6002 ProductType=1。Server 2008のProductType=3の記録をクライアントの証明として流用していない。

**主要11項目のWOW64 / x64、合計22の主実行が成功。** CPU機能、domain、device-family、persisted-state、Package SID、Capability SID、membership、object comparison、拡張file information、thread alert / wait、性能counter / frequency。追加実装や新しい配布バイナリの変更ではなく、既存機能のVista側の検証範囲を広げた。

- tests/run_kxnt_core_suite.ps1を追加。既存の意味・native参照照合のgateを持つrunnerを順に実行し、各receiptを固有RunNameのフォルダーに保存する。旧アーカイブは上書きしない。途中失敗ならCompletedとCurrentProbe、Failedを保存して中止する。Passwordを証跡に記録しない。
- 実行前にReleaseのKexDll / KxNtとInstaller（x86はKex32、x64はルート）のSHA256一致を4本とも要求した。rootやシステムディレクトリに配備せず、ユーザー専用scratchを使用した。
- Device familyではclient / serverの区別、NULL出力8通り、PEBで偽装した3バージョンを再確認。家族情報を固定server値にしたテストではない。
- domain / persisted-state / SID分類 / membership / comparisonは既存のnative参照との出力比較も再実行。比較では生きたhandle identity / typeの前後照合が成功。
- File informationは各形式96ケース、2回の1,000反復でhandle増加0。Alertは通常試験に加え、Nt / Zw各aliasの待機thread終了ケースをVMware Toolsから別起動し、終了後timeoutとhandle増加0も両形式で成功。通常起動の子へのKexDLL伝播を証明したテストではない。
- 性能counter / frequencyは出力のalignment16ケース、不正出力10ケース、4threadで各4,000呼出し、TLS維持、native RTLとの出力比較が成功。

証跡: docs/validation/kxnt-vista-client-core/ の11 receiptとmanifest（VMX、GuestUser、GuestDirectory、client種別、EXE / DLL / runner SHA256、raw結果）。再実行: tests/run_kxnt_core_suite.ps1 -VMX <Vista VMX> -GuestUser <ユーザー> -GuestPassword <パスワード> -GuestDirectory <専用フォルダー> -RunName <新しい名前> -ExpectedProductType 1。Server用はExpectedProductType 3を指定する。

この検証はscratchの明示DLL読込みであり、システムDLL配備、通常IFEO起動、native32bit OS、任意token、全競合・全不正入力の検証ではない。Vistaユーザーの名前から標準ユーザー / token elevation状態を推定していない。SRW、UTF変換、ConDrv、SilentProcessExitのVista側追加検証は継続する。既存の機能制約や未対応部分をこのPASSで解消したと扱わない。
## 2026-10-03: SRW try取得の Vista クライアント検証

Vista x64 VM（前節でnative 6.0.6002 / workstationを確認）のWOW64 / x64で、tests/run_kxnt_srw_vm.ps1のnative interoperation試験を実行し、両形式で成功した。新しい配布DLL変更ではなく、979cfa9で実装した機能のクライアント側検証である。

- Native RtlTryAcquireSRWLockの不在を終了5とNATIVE_TRY_ABSENTで確認し、native acquire / releaseで作ったVistaのロック状態も検査した。
- Nativeホストのtry取得結果と比較。保持中 / 空き / shared count、待機者のいる状態、無効・guard・read-only出力の例外、TLSとlock word不変性の固定結果が一致。ホスト上のnative委譲経路も比較した。
- Native condition variableとexclusive / shared lockの組合せ、4workerのnativeと互換tryの混在、保護データ、終了、handle増加0のgateを通過した。
- 実際のProviderPathとKexDllPathがVistaユーザーの専用C:\Users\Vista\KxNtParity\x86またはx64であることをrunnerが要求した。コード・DLL・EXEのSHA256、native / adapter / VM raw結果を保存した。

証跡: docs/validation/kxnt-srw-vista-client.json。再実行はtests/run_kxnt_srw_vm.ps1にVista VMX、GuestUser、GuestPassword、GuestDirectoryを指定する。終了5はAPI不在の明示確認であり、try機能の試験失敗を無視するための扱いではない。

通常IFEO起動、native32bit OS、任意スケジューラの全interleavingは依然として未検証。システムDLL・Installer・Releasesは変更していない。続いてUTF変換を測定したところ、x64のcold resource比較gateに差分があり停止した。これをSRWの成功やUTF変換の全面的な成功へ読み替えず、別途診断する。
## 2026-10-03: UTF Vista試験のcold資源gate失敗とコンソールIMEの対照

VistaのWOW64では既存UTF runnerのnative比較を通過したが、x64はcold resource差分のgateで停止した。対照の初回100ms後のhandle増加が8、互換変換は7。両方のwarm測定は0、個々のprobeの内容検査はFailures=0だった。**UTFの両形式に対する既存runner全体の成功とは記録しない。** 失敗した原データを上書き前に保存した。

x64のfixed contract比較は、失敗gateより後にあるためその初回runnerでは実行されなかった。後で原データを使って同じ固定行の比較を別途行い、ホストnative / native委譲 / VMが一致することを確認した。4,850 conversion / size-queryケース、24 pointerケース、全1,112,064 scalarの全内容と往復、18 overlap、4worker ×16,000呼出しの2phaseが対象。cold handle数はこの比較から環境値として分離するが、別のresource gateは失敗のまま維持した。証跡: docs/validation/kxnt-utf8-vista-client-initial-failure.json（native参照、原VM / lookup対照、hash、失敗scope）。

### 追加の資源診断

- tests/kxnt_utf8_resource_probe.c / runnerを追加し、WOW64 / x64各5modeを別プロセスで実行。KexDllありのempty worker / native lookupだけ / 実変換、KexDllなしのempty worker / native lookupだけ。これは別の計測を加えた診断であり、初回失敗時の同一実行やstack採取ではない。
- 同じ4workerをcold / warmで作成・join・close。変換modeの内容も検査。前、直後、100ms待機後、さらに900ms待機後、warm後のsystem handle表から、自PIDのhandle identity / kernel object / type / accessと個数を記録した。Key / Event / Directoryの名前、Threadの所有PID / ID / start、Processのimageも記録。型照会が失敗したhandleもstatusを隠さない。File名の照会は行っていない。
- x64の各modeで、増えたProcess / Threadが別PIDのC:\Windows\System32\conime.exeを指すことを確認した。ConsoleIME_StartUp_Event、BaseNamedObjects、Windows NT CurrentVersion / AppCompatFlagsのKeyも観測した。
- KexDllなしのmode3 / 4でも同種の増加が起き、初期10handleから16または17へ増えた。計測回によってempty modeでも6 / 7の差があり、warmでEventが追加される場合もあった。したがって異なるプロセスのcold countだけから変換関数の漏れと断定できない。コンソール初期化の非同期動作が比較を乱している可能性を示す証拠であり、すべての作成元を同定したとの主張ではない。
- runnerは全5観測点、no-KexDll対照の実際のKexDllLoaded=0、互換modeのprovider / implementationの専用フォルダーパス、MEASURED終了を要求した。MEASUREDは診断完了であり、漏れゼロやcold差分解決の判定ではない。元のUTF resource gateを緩めていない。

証跡: docs/validation/kxnt-utf8-vista-client-resources.json（両形式10実行のraw結果、source / EXE / runner / DLL hash）。再実行: tests/build_kxnt_probes.ps1を両形式で実行し、tests/run_kxnt_utf8_resource_probe.ps1にVMX / GuestUser / GuestPassword / GuestDirectoryとArchitectureを指定する。modeごとのrawログはaudit/KxNtParityに残る。

初回差分のhandle作成stack、各Eventの完全なlifecycle、計測介入がない実行との帰属、通常IFEOの統合試験は未完了。配布DLL、Installer、システムDLLは変更していない。SRWのVista成功は別の記録としてコミット済みであり、今回のUTF資源比較の失敗を覆い隠さない。
## 2026-10-03: ConDrv限定互換の Vista クライアント検証

Vista x64 VMのWOW64 / x64でtests/run_kxnt_condrv_vm.ps1を実行し、両形式でconsole and real Zig stdio試験に成功した。66f071fの限定互換をクライアントでも測定した結果であり、ConDrv全体の実装完了ではない。

- コンソールハンドルの判定、無効値、File / consoleの数値alias、file / pipeのnative経路、offset / IOSB / input pointer、同期書込み、文字コード、強制部分完了など、既存probeの全gateを再実行。KexDll / KxNtの実読込みパスを専用フォルダーと照合した。
- 識別情報とimport表を使うprofileの境界・破損入力、DisableAppSpecific、profileなしのnative対照を実行。全アプリを一律にconsole adapterへ転送したテストではない。
- 4threadで4,000書込みをcold / warmで実行し、coldのhandle増加がnative対照と一致、warmの増加0、生きたhandle identityの前後一致を確認した。
- 公式Zig 0.16の専用に変更したprivateコピーを使用。両形式でconsole child終了0・39 UTF-16 unitsの内容を照合。file / pipeでは終了0・40bytesの全内容を照合した。公式配布EXEやユーザーアプリのimport表を変更していない。
- KxBaseのVT modeのロード・export・有効化・出力も成功。raster font上のUTF8表示にあるnativeの制約が解消したとは扱わない。

証跡: docs/validation/kxnt-condrv-vista-client.json（native対照、VM raw結果、source / EXE / DLL hash）。再実行はtests/run_kxnt_condrv_vm.ps1にVista VMX、GuestUser、GuestPassword、専用GuestDirectoryを指定する。

既存の制約は維持する。自然なOS部分完了ではなくowned KexDllのIATだけで強制したcase、曖昧aliasのNOT_SUPPORTED、event / APC等の非対応経路、cold native-control Eventの作成stack未同定、通常IFEO統合・system DLL配備未検証。配布バイナリ、システムDLL、Releasesは変更していない。
## 2026-10-03: SilentProcessExitの明示的未対応とimport解決を Vistaで確認

Vista VMのWOW64 / x64でtests/run_kxnt_silent_exit_vm.ps1を実行し、既存の明示的rejectionとimport bindabilityの試験に成功した。**WER終了監視・報告機能は未実装のまま。** 014c609の機能制約をクライアントで確認した結果である。

- 各形式9handle validation caseをホストnativeと比較し、有効なprocessではSTATUS_NOT_SUPPORTEDを期待値として明示した。無効、閉じたhandle、別object、zero / limited access等の結果も比較し、LastErrorを確認した。
- 各形式1,000回の呼出しでhandle増加0。ReportingSupported=0、Result=PASSを確認した。PASSは報告処理の実施を意味しない。
- 専用Zig smoke EXEのimport resolutionを全件確認し、RtlReportSilentProcessExitもResolved=1、Missing=0。ここでは実際のWERイベントやreport作成の観測は行っていない。
- DLL / EXE hash、native / VM raw結果を保存。runnerはKxNtを専用絶対パスで渡すが、forward先KexDllの実module pathをログ・gateにしていないため、その限定も証跡に明示した。

証跡: docs/validation/kxnt-silent-exit-vista-client.json。再実行はtests/run_kxnt_silent_exit_vm.ps1にVista VMX / GuestUser / GuestPassword / GuestDirectoryを指定。配布DLL、システムDLL、Releasesは変更していない。実際のWER、通常IFEO、任意process tokenの全面検証へ成功範囲を拡張しない。
## 2026-10-03: コンソール初期化を避ける条件で起動停止と実行間の差を検出

UTF cold資源測定のconime影響を切り離すため、[Microsoftのprocess creation flags仕様](https://learn.microsoft.com/en-us/windows/win32/procthread/process-creation-flags) にあるCREATE_NO_WINDOW条件を試した。APIの変換caseを省略する案ではなく、同じ試験を異なるconsole起動条件で測定する診断である。**この環境では安定した測定に到達しておらず、UTFの成功条件として採用していない。** 元のUTF resource gateの失敗は維持する。

- tests/kxnt_native_launch_probe.cに明示的なno-console引数を追加。既存引数ではcreation flags=0を維持し、指定時だけ08000000を使用。実際のcommand lineとflagsを記録した。timeout時には、自分で作成した子だけのDLL一覧、主thread context PC / SP、stackの32pointer分を取得。主threadをresumeしてから、所有する子を終了する。他のアプリやsystem processを停止しない。
- UTF probeをこの条件で起動した初期試行は、CreateProcess成功後10秒でtimeoutし、子の試験logを取得できなかった。この試行だけでUTF処理が原因とは判断しない。
- tests/run_kxnt_launch_creation_flags.ps1を追加し、cmd.exe /d /c exit 0を対照にした。Vista client / Server 2008、WOW64 / x64、flags0 / CREATE_NO_WINDOWの8実行。通常4実行は自然終了0。初期の対照試行ではno-console両形式が両VMで時間切れになったが、context採取を追加して再実行した最新セットでは、Vista WOW64のno-consoleだけは自然終了0、残る3条件はtimeoutだった。この差を隠さず保存した。恒常的なハングやOS非対応を断定しない。
- ParentはKexDllLoaded=0。timeout3件ではcmd.exe / ntdll.dll / kernel32.dllのsnapshot、PC / SPとstackを採取できた。snapshotにKexDllは記録されていないが、子の全初期化・IFEO設定や将来のmodule読込みを証明する観測ではない。現在の証跡だけでOS、VMware Tools、互換レイヤーのどれが原因かを決めない。
- SDK7.1のToolhelp ANSI APIはModule32First / NextとMODULEENTRY32であり、最初に使ったA suffixの宣言はビルドエラーだった。修正し、VS2010で両形式を再ビルドしてから最終8実行を採取した。失敗したbuildを実行成功として使っていない。

証跡: docs/validation/kxnt-launch-creation-flags.json（最新8 raw結果、VMX / user、source / EXE / runner hash、自然終了とtimeoutの区別）。再実行はtests/build_kxnt_probes.ps1を両形式で実行し、tests/run_kxnt_launch_creation_flags.ps1に各VMX / GuestUser / GuestPassword / 専用GuestDirectoryを指定する。10秒は診断上限であり、永続ハングの証明ではない。MEASUREDとtimeoutを動作成功としない。正常終了したno-console例もエラーに置換しない。

次の解析ではPC / return addressのsymbol・module offset、子の初期化と標準handle条件を調べる。conime由来と推測した全handleの作成stack・lifecycle、計測介入の影響、通常console条件のUTF cold差分、通常IFEO統合は未解決。配布DLL、システムDLL、Installerは変更していない。
## 2026-10-04: CREATE_NO_WINDOW の標準入出力条件を切り分け

前回のコンソールなし起動停止について、tests/kxnt_native_launch_probe.c に no-console-nul を追加した。実際の継承可能な NUL read / write handle を作り、STARTF_USESTDHANDLES と bInheritHandles=TRUE で子に渡す。親は CreateProcess 後に自分の handle を閉じる。通常起動と従来の no-console 条件は保持した。配布互換 DLL の変更ではなく、UTF 資源診断を進めるための起動対照である。

- VS2010 / SDK7.1で x86 / x64 probe をビルドし、Vista / Server 2008 の各形式で normal / no-console / no-console-nul、計12実行を完了した。
- 通常起動4実行と WOW64 のコンソールなし4実行は自然終了0。x64の no-console / no-console-nul は両VMで10秒の診断上限に達した。所有する子の module / PC / SP / stack を採取し、当該子だけを終了した。前回はServer WOW64にも時間切れがあり、今回の自然終了は停止が恒常的とは限らないことを裏付ける。
- raw logの ExplicitNulStdio / StartupFlags でNUL指定の実行を確認した。source / runner hash と当時のEXE hash、VMX / user、raw logを保存した。runnerはcreation flagsと親KexDll不在をgateにしているが、NUL指定行の照合はこの証跡整理時に追加で実施した。
- x64では明示的な標準入出力だけでは解消しない。CREATE_NO_WINDOWが利用不能という一般論、OS / VMware / VxKexの原因特定、UTFのcold資源gate合格へは拡張しない。MEASUREDは観測完了であり機能成功ではない。

証跡: docs/validation/kxnt-launch-stdio.json。以前の8実行の証跡は別ファイルに保持した。この12実行は実行セッションの終了コード0と全ログ取得を確認済みであり、後続のVM再起動の測定とは混在させていない。再実行は tests/run_kxnt_launch_creation_flags.ps1（3modeに拡張）を使用する。UTFの通常console cold差分、作成stackとlifecycle、通常IFEO統合は引き続き未解決。Installer / system DLL / Releasesは変更していない。

## 2026-10-04: WindowsサブシステムでUTF全caseを比較、cold資源gateは未解決

コンソールなしフラグでの起動停止を避ける追加条件として、tests/build_kxnt_probes.ps1にutf8-windowless.exeを追加した。同一のkxnt_utf8_probe.cをWindows subsystem 6.0 / mainCRTStartupでビルドし、case数・worker・100ms測定・guard・TLS・内容比較を省略していない。画面を作るコードはなく、CREATE_NO_WINDOWは使わずflags0で所有するlauncherから起動した。これは別起動条件の診断であり、元のconsole resource gateを置き換えない。

- Server 2008 / Vista、WOW64 / x64の4組でlookup対照と変換試験が自然終了0。Windows subsystem=2のPE header、実runtime provider / implementationパス、launcherのflags0・自然終了0をrunnerで要求した。
- 全4組でホストnativeの同じソースによる固定行と完全一致。4,850変換 / size-query、24pointer、全1,112,064 scalarの全内容と往復、18overlap、4worker ×16,000変換のcold / warm各phaseを維持した。資源countは別gateで評価し、固定行のcold数だけを環境値として分離した。
- cold変換 / lookupの100ms後deltaはServer WOW64=1 / 1、Server x64=0 / 2、Vista WOW64=0 / 1、Vista x64=0 / 1。warmはすべて0。Server WOW64だけ資源gateも通過し、残る3組は失敗。全体receiptは両VMともFailed。native semanticsの一致を資源問題の解決と扱わない。
- tests/run_kxnt_utf8_windowless_vm.ps1を追加。RunNameごとの新規専用archiveに全raw logとreceiptを保存し、過去の結果を上書きしない。途中例外でもIncomplete状態を残し、gate失敗をthrowする。DLL / EXE / source / runner hash、VMX / user、範囲を記録した。通常IFEO統合やsystem DLL配備は証明しない。

証跡: docs/validation/kxnt-utf8-windowless-server.json / kxnt-utf8-windowless-vista.json。元のconsole初回失敗の記録は保持した。再実行は両形式をbuild後、runnerにVMX / GuestUser / GuestPassword / GuestDirectory（この診断では空白のないscratch） / 未使用RunNameを指定する。追加起動条件でもcold対照数に差が残るため、コンソールIMEだけを原因と断定できない。次は残るhandleの型・identity・作成元を調べる。

### コンソールなし起動の停止位置

native-launchのtimeout採取でinteger registerとRdx先頭32bytesを追加し、Server x64のno-console-nulを実測した。所有する子は10秒以内に終了せず、module一覧とcontext取得後に終了した。現VMからntdll / kernel32を取得し、PCのRVA45b4aはNtWaitForMultipleObjects export45b40の内部であることを照合した。kernel32のreturn RVA22cdeの直前はwait呼出し、count=2を設定し、handles配列をrsi+28hから渡す命令列だった。実contextのRcx=2、Rdx=Rsi+28h、配列の先頭二値は0x10 / 0x18。これらのobject type / name、signalしない理由、CSR messageの意味は未特定。

証跡: docs/validation/kxnt-launch-wait-registers.json（raw context、module hash、export RVAとdisassembly抜粋）。VCのx64 dumpbinはmsdis170.dllをロードできずdisassemblyを生成しなかったため、その出力を証拠にせず、同梱32bit dumpbinでx64 imageを逆アセンブルした。関数名を推定で補わず、確認できたexportとoffsetのみを記録した。

配布DLL、Installer、system DLL、Releasesは変更していない。UTFの資源gate、待機objectの帰属、通常IFEO統合、監査に残る未実装項目は引き続き未完了。

## 2026-10-04: Windowless資源identityとnative loader-lock Eventの作成を観測

tests/build_kxnt_probes.ps1に同じ資源診断ソースのWindows subsystem版を追加し、tests/run_kxnt_utf8_resource_probe.ps1にWindowless / RunNameを追加した。各VM / architectureでempty、native lookup、変換、KexDllなしempty、KexDllなしnative lookupの5mode、計20実行を完了した。全5phaseのhandle表、型・名前・identityを採取。Windowlessではflags0のowned launcherで自然終了0、PE subsystem=2、実DLLパス、no-KexDll対照の実不在を要求し、Measuredと漏れゼロの判定を区別した。DLLは当該Releaseから専用scratchへコピーし、そのhashを記録した。既存console modeの起動条件は保持した。

- tests/analyze_kxnt_handle_snapshots.pyを追加。各phaseの列挙数と解析できたidentity数を照合し、handle番号とkernel objectの組を使ってbeforeから追加・削除された資源を抽出した。名前だけの比較や番号だけの再利用判定ではない。
- 全20条件でafter100msの追加資源は0〜2個の名前なしEventであり、今回のwindowless実行でProcess / Thread / conimeの追加は観測されなかった。互換DLLを読み込まないmode4でも両VM・両形式でEventが増えた。warmの増加やidentity差は全phaseの証跡を保存し、数だけから漏れ・所有者・作成元を断定しない。
- これは別プロセスでの診断で、前回の全UTF試験で失敗した個々のcold Eventを追跡したものではない。元のcold数一致gateやそのFailed記録を維持する。

証跡: docs/validation/kxnt-utf8-windowless-resources.json（4receipt、20raw）、kxnt-utf8-windowless-identities.json（全phaseの追加・削除と元receipt hash）。再実行はbuild後にresource runnerへWindowless、未使用RunName、VMX / user / password / scratch / architectureを渡す。異なるRunNameで過去の結果を残す。

### Server x64のnative-only mode4で一つのEventをloader lockへ帰属

CDBでNtCreateEventのentry stackを採取した。最初はCDBが終了時breakpointで止まり、owned launcherが10秒でdebuggerを終了したため、debuggerの自然終了とは扱わなかった。[MicrosoftのCDB仕様](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/cdb-command-line-options)で終了時breakpointを無視する-Gを確認し、別logへ再実行。probeのMEASURED終了とdebuggerの自然終了0を確認した。

- NtCreateEventのstackにLdrInitializeThunkがあり、native ntdll内部からの作成を観測した。export-onlyのvsnwprintf等の近傍名を内部関数の正確な名前として採用しない。sourceを省略した最適化EXEであり、debuggerの介入はworker間競合を変える。
- 取得済みServer ntdllのhashとdisassemblyに対し、return RVA39b76でSTATUS_SUCCESS、RVA39b8bでcritical sectionのLockSemaphore（+18h）を観測した。critical sectionはnative ntdll+1122e0、格納済みhandleは0x30。公開LdrLockLoaderLock（RVA4b3d0）が同じcritical sectionをRtlEnterCriticalSectionへ渡す命令も照合し、loader lockであることを裏付けた。
- 同じデバッグ実行のbeforeにはそのEventがなく、after100msではhandle0x30が名前なしEventとして存在し、warm100msにも同じkernel objectが残った。native loader lockの同期資源を変換関数の漏れと混同しない。全Eventの作成・閉鎖、他VM / architecture、前回失敗時の同一processへの帰属は未証明。
- 内部RVAは当該Server ntdllだけの診断用。互換DLLに固定アドレスやアプリ名の分岐を追加していない。再現用の正確なCDB commandsとmodule / probe hashを証跡内に保存した。別imageへこれらのRVAを無検証で適用しない。

証跡: docs/validation/kxnt-native-loader-event-trace.json（成功traceと同じprocessのsnapshot、初回debugger timeoutの区別、commands / hashes / disassembly）。通常consoleの全cold差分、残るEventの帰属・lifecycle、通常IFEO統合は未完了。配布DLL、Installer、system DLL、Releasesは変更していない。

## 2026-10-04: 全UTFケースと同じプロセス内で資源を追跡

kxnt_utf8_probe.cにKXNT_UTF8_RESOURCE_TRACEでのみ有効なsnapshotを加え、buildにutf8-traced-windowless.exeを追加した。通常buildではsnapshotも追加待機も無効。新規kxnt_utf8_trace.hは既存の資源診断と同じhandle表 / 型 / 名前 / identityの採取を使う。traced buildはcold前・直後・100ms後・1000ms後・warm100msの5点を同じプロセスで採取する。4,850case、24pointer、全scalar、18overlap、4worker ×16,000callは維持し、snapshotと待機の介入を明示する。過去の初回失敗プロセスを遡って採取できたとの主張ではない。

- run_kxnt_utf8_windowless_vm.ps1にTraceResourcesを追加。両VM / 両形式でlookupと全変換を実行し、同じnative固定行と全4組が一致した。SourceSHA256に追加headerを含めた。記録は両VMともFailedであり、Server / VistaのWOW64だけcold資源gate通過、x64は差分が残った。従来のgateは緩めない。
- analyze_kxnt_handle_snapshots.pyにfull-utf receipt解析を追加し、全phaseの列挙数とidentity数を照合した。Server / VistaのWOW64ではlookup / 変換とも名前なしEventが一つ追加、x64ではlookupだけ一つ追加・変換では追加なし。warm後も全identityの差分を保存した。
- 別のCDB実行でServer x64の全ケースEXEを追跡。内部RVAを使う前に当該VMのntdllを再取得し、前回のimage hashと一致することを要求した。lookupは自然に完了し、同じ実行でloader critical sectionのLockSemaphore=0x28とafter100msのEvent handle=0x28が一致した。これにより今回の実lookup対照のcold増加をnative loader lockに帰属できた。他VM / architectureや過去の全差分へ一般化しない。
- 最初の変換debug実行ではwarm Eventが増え、その後、pointer試験の意図的なfirst-chance access violationでCDBが停止し、10秒のowned debugger timeoutとなった。この部分logは不完全として保存した。アプリの無処理AVや全試験の完了と扱わない。loader lockへのstoreとwarm Eventの対応はrawに残すが、完了実行の代わりには使わない。
- sxd avを加えてfirst-chance AVを試験側へ渡すCDB commandsで別logへ再実行した。debugger自然終了0、4,850caseを含む全試験のFailures=0 / PASS、cold / warm delta0を確認した。デバッガーの成功終了だけからprobe成功を推測せず、probe最終行も確認した。どの実行でも必ずEventが増えるわけではなく、debug介入が競合を変える。

証跡: docs/validation/kxnt-utf8-fulltrace-server.json / kxnt-utf8-fulltrace-vista.json、kxnt-utf8-fulltrace-identities.json、kxnt-utf8-fullcase-event-traces.json。再実行は両形式build後、windowless runnerにTraceResourcesと未使用RunNameを渡す。CDBの正確なcommands / module hash / 不完全実行との区別はevent-traces証跡に保存した。内部RVAは当該Server ntdll専用の診断であり、配布コードには追加していない。

元のconsole cold差分、残るEventの全面帰属とlifecycle、通常IFEO統合、監査の他の未完了項目は引き続き未完了。配布DLL、Installer、system DLL、Releasesは変更していない。

## 2026-10-04: NtOpenKeyEx のフラグと構造体alignmentを両形式で再測定

監査全体の実装に戻るため、未解決native転送のNtOpenKeyExを再確認した。[Microsoftの仕様](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwopenkeyex)はWindows 7以降のAPIで、OpenOptions=0を旧openと同等とする。一方、既存の実測にはOpenOptions=8とOBJ_OPENLINKの違いがあり、単純なフラグ変換はまだ正当化できない。

- tests/kxnt_open_key_options_probe.cを追加。既存のread-only probeを拡張し、通常SYSTEM、CurrentControlSetリンク、欠落キーの3入力 × attributes0x40 / 0x140 × 12OpenOptionsをnative hostで測定。加えて同じObjectAttributesをbyte offset0〜8へ置いた9入力、合計81ケース / 形式を測定した。旧APIは3入力 ×2attributesとalignment9入力の15ケース / 形式。
- VS2010で両形式をbuild。再現runner tests/run_kxnt_open_key_options.ps1でホストnative / legacy、Server / Vistaのnative absence / legacyを確認した。NT6 VMのexit5はNativeExPresent=0 / NATIVE_EX_ABSENTを実際のlogで確認した期待する非存在診断であり、起動成功やadapter成功と扱わない。legacyは各15行とFailures=0を要求した。LastError / LastStatusを各呼出しで検査し、handle成功時にはNtQueryKeyで実際のキー名を確認した。
- ホストの両形式でCurrentControlSetにOpenOptions=8・attributes0x40を渡すとControlSet001へ解決された。同じ8でもattributes0x140ではCurrentControlSetのリンク自身が返った。旧NtOpenKeyにattributes0x140を渡した結果もリンク自身。したがって、このnative referenceではoptions8を無条件にOBJ_OPENLINKへ変換すると、要求と異なるキーを返す。
- ホストnativeはoptions0x10 / 0x18も受理した。この観測を全Windows版の規範とせず、公開仕様の4 / 8だけを理由に未知bitを一律INVALID_PARAMETER_4へする実装も現時点で採用しない。backup / restoreは権限と検証順序を含めて別問題である。
- alignmentのoffset4はホストWOW64で成功、ホストx64ではSTATUS_DATATYPE_MISALIGNMENT。offset1 / 2などは拒否された。入力構造体を無条件にalignedなローカルcopyへ移すと、このnative parameter validationを失うおそれがある。任意のpointer配置やoutput pointerの検証順序を網羅した試験ではない。

証跡: docs/validation/kxnt-open-key-options-server.json / kxnt-open-key-options-vista.json（それぞれ両形式、native / legacy raw、source / EXE / runner hash、VMX / user）。RunNameごとに別archiveへ保存し、途中例外はIncompleteを残す。キー作成・ACL変更・特権有効化・配布API変更は行っていない。

次工程は、仕様とnativeで保証される通常openの移植を進めつつ、flags4 / 8 / 0x10、保護キー、相対root / view、削除競合、引数検証順序を別途解決する。通常openだけでNtOpenKeyEx全体が完成したとは扱わない。WNFは元監査の実利用確認・設計条件を維持する。UTF資源の残る帰属と通常IFEO統合検証も未完了のまま。Installer / system DLL / Releasesは変更していない。

## 2026-10-04: NtOpenKeyEx の通常openを実装・両VMで検証

未解決native転送の通常openをKexDll/openkeyex.cへ実装し、KxNtのNtOpenKeyEx / ZwOpenKeyExをKexNtOpenKeyExへ転送した。KexDllにordinal318を追加し、宣言とプロジェクト / filtersを追加した。Native NtOpenKeyExが存在するOSでは全引数をそのまま委譲し、NT6.0の非存在時はOpenOptions=0だけをNtOpenKeyへ渡す。これは通常openの移植であり、NtOpenKeyEx全体の完成ではない。

- GetProcAddressのキャッシュ初期化時にLastError / LastStatusを保存・復元。ObjectAttributes、Name、RootDirectory、出力pointerをローカルcopyしたり書き換えたりしない。native probing、alignment、ACL、view指定、output handleの失敗時動作を旧APIに任せる。
- NT6.0の非ゼロoptionsはSTATUS_NOT_SUPPORTEDとし、出力を変更しない。invalid flagsのnative statusを完全再現したとは主張しない。backup / restore、OpenOptionsのlink指定、virtualization等は未対応のまま。保護キーを作成する代替や、flag8をOBJ_OPENLINKへ変換する処理は追加していない。ObjectAttributesに明示されたOBJ_OPENLINKは通常openでそのままnativeへ渡る。
- VS2010でKexDll / KxNt / probeを両形式build。Server 2008 / VistaのWOW64 / x64、計4組で通常openの140case / 組をnative NtOpenKeyと同じプロセスで比較して全件一致。absolute通常 / link / missing、relative root / empty name、NULL / bad length / missing Name / inaccessible Name / inaccessible attributes / unaligned attributes / NULL output / readonly output / invalid root、2attributes ×5access maskを対象にした。状態、output変更の有無、LastError / LastStatus、実キー名、GrantedAccessを比較した。
- 各組1,000回の取得・closeでhandle delta0。未対応options8種類のNOT_SUPPORTED・output未変更を確認した。Nt / Zwが同じ実装addressを解決することと、provider / implementationが専用scratchの実DLLであることをrunnerで要求した。
- ホストの両形式でも140caseをnative旧APIと比較し、native NtOpenKeyExへの委譲を確認。さらに非ゼロoptions8種類をnative NtOpenKeyExと直接比較して一致した。NT6.0の非ゼロ対応が完成したこととは区別した。
- build後に作業ブランチのInstaller4DLLを更新し、Release / Installerのhash一致を確認。24a03ae時点の既存export ordinal変更は4DLLすべて0。新APIの追加が既存ordinalを変えていない。追加probeにはWindowsのfunction addressをGetModuleHandleExのFROM_ADDRESSへ渡すcastに対するVS2010 C4054警告があり、build failureではない。production新ファイルのcompiler errorはない。

証跡: docs/validation/kxnt-open-key-adapter-server.json / kxnt-open-key-adapter-vista.json、kxnt-open-key-export-ordinals.json。再実行はbuild_kxnt_parity.ps1の後にtests/run_kxnt_open_key_adapter.ps1へVMX / user / password / scratch / 未使用RunNameを指定する。receiptがPassedでもscopeは通常openとホストnative委譲に限定する。

今回のVM配備は専用scratchだけで、system DLL / 原作業ツリーのInstaller / Releasesは変更していない。native32bit OS、任意のsecurity descriptor / QOS、registry virtualization、transaction root、削除競合、通常IFEOの統合は未検証。GuestUserは記録したがtokenの標準 / 昇格状態を別測定していない。拡張flags、とくに保護ACLを迂回すべきbackup / restoreの実装・検証は継続する。UTFの資源gateと監査の残りも未完了。

## 2026-10-04: 通常の IFEO / AVRF 起動で NtOpenKeyEx の静的インポートを検証

tests/kxnt_open_key_adapter_probe.c に KXNT_IFEO_IMPORT 時だけ有効な静的インポート診断を追加した。tests/kxnt_ifeo_imports.def は KxNt ではなく ntdll!NtOpenKeyEx を参照する。専用 build script は両形式の PE import 表を dumpbin で検査し、ntdll と対象 API があり、KxNt / KexDll の直接 import がないことを要求する。main では LoadLibrary せず、既に読み込まれた KxNt / KexDll と IAT の実関数アドレスを照合し、その静的 import を全比較ケースに使用する。既存の通常 build は専用 scratch の明示 LoadLibrary のまま。

- 再起動後のユーザー Server VM では MCP / 直接 vmrun の cmd.exe が exit1だったが、既存の owned native-launch 経由の cmd.exe は flags0で自然終了0、転送も成功した。ホストの REST API は VM 一覧取得成功、vmrest は session0、VMware 本体は session1。この差だけをゲスト実行失敗の原因と断定しない。
- グローバル KexDir はプロセス単位で上書きできず、private AVRF DLLを指定するだけでは配布 DLL の検証にならない。ユーザー VM の system DLL と製品設定を変更する代わりに、既存の使い捨て Server 2008 clone で実インストーラーを使った。cloneをこの工程で起動した。元の両VMは変更していない。
- tests/run_kxnt_ifeo_vm.ps1 は clone の正確な VMX だけを受理し、新規 host archive / guest fixture を要求する。58ファイルの候補全体、ソース / EXE の hash を記録する。guest driver は使い捨て marker、実際の管理者 token、新規インストール状態、未所有の IFEO key 不在を確認してから変更する。各 child に60秒の待機上限を設け、timeout時は owned child だけを終了し失敗とする。
- 未適用の同じ両形式 EXE は entrypoint不足 0xc0000139 で自然終了した。候補 VistaSetup で導入後、KexCfg /ADD でその EXE だけを有効化。x64 / WOW64とも早期 KexDll 読込みと static import address一致、native NtOpenKeyとの140比較、8非対応option拒否、1,000回 open / closeの handle delta0、Failures=0を確認した。直接LoadLibraryによる代用ではない。
- 最終実行で system32 / SysWOW64 の KexDll / KxNtと C:\VxKex の両形式 package copy、計8DLLを候補とbyte比較した。実providerはx64が C:\Windows\system32\kxnt.dll、WOW64が C:\VxKex\Kex32\kxnt.dll。implementationは両形式の system32\kexdll.dll（WOW64ではredirectされた32bit DLL）であり、比較した候補に対応する。候補の主要4DLLは現在のReleaseとhash一致。
- /DELETE、実VistaSetupのuninstallを実行し、owned IFEO key不在と製品directory / HKLM marker不在を確認した。最終driver自然終了0 / Failures=0、runner State=Passed、両形式Passed。ユーザーVMのインストールと original Installer / Releases は更新していない。
- 初回は診断側が Kex64\KxNt.dll の存在を誤って要求し、導入後の比較で停止した。profileを有効化せず、uninstall成功。二回目は実driverと両probe成功だったが、runnerがWOW64の実Kex32読込み先を拒否した。両回のIncomplete記録を残した。実配置のbyte比較と期待providerを修正した三回目で最終成功を確認し、過去の失敗記録を書き換えていない。VS2010のfunction pointer診断castに対するC4054警告は残る。

証跡: docs/validation/kxnt-ifeo-open-key-server.json（最終receipt、候補58file hash、両raw probe、driver、PE import表、初回 / 二回目の区別）。再現は両形式 build_kxnt_ifeo_probe.ps1 の後、run_kxnt_ifeo_vm.ps1へ使い捨てcloneのVMX / credentials / 新規RunNameを渡す。固定guest fixtureが残る場合はdriverのteardown結果を確認してから、既存fixtureを別の専用archive名へ保存する。ユーザーVMへこのrunnerを適用しない。

今回証明した通常IFEO統合は NtOpenKeyEx の OpenOptions=0 に限定する。Vista clientの通常IFEO、native32bit OS、監査にある他APIの静的import / loader初期化、backup / restore等の拡張options、UTFの残る資源gateと帰属、WNFの実利用・設計は引き続き未完了。監査全体の完了とは扱わない。

## 2026-10-04: 監査対象の詳細テストを通常IFEO経由で実行

通常ローダー経路の検証を、processor feature、domain正規化、device family、persisted state、package / capability SID、通常token membership、object比較、拡張file操作、thread alert / wait、RTL性能counter、SRW tryの12種類へ広げた。新規 tests/kxnt_ifeo_suite_provider.h / imports.def と build / runner の Suite switch を使い、既存の詳細probe本体を再利用する。アプリ名や製品固有のproduction分岐を追加していない。

- 各suite EXEは26個の ntdll APIを静的importする。helperはWindowsの実IAT slotを読み、既に存在するKxNtに対するGetProcAddressの結果と同じaddressであることを検査する。無関係なAPIに仮のcalling conventionを与えず、実呼出しの型は既存probeが保持する。helperがKxNt / KexDllを新しくLoadLibraryすることはなく、既存moduleのreferenceだけを取得して従来のFreeLibraryと釣り合わせる。別libraryの読込みは従来のnative経路を保つ。
- x86のIAT変数名装飾、PowerShellの単一library引数のarray保持、VS2010 Cの宣言位置をビルド時に修正し、両形式の12suite EXEをbuildした。これらは診断のビルド不具合で、production APIの変更ではない。診断macroのC4127とfunction address castのC4054等の警告は残る。
- 使い捨てcloneは実測NT6.0.6003 / ProductType3 / native architecture9、driverのKexDll未読込みを確認した。ユーザーVMの製品設定を変更していない。26 EXEで未適用の0xc0000139を確認し、58file候補を実VistaSetupで導入。KexCfgで各imageを有効化してから実行した。DLLのsystem / packageコピー8個のbyte一致、実provider / implementationパス、26個のslot一致を検証した。
- 初回のsuiteはState=Failed / driver exit1。WOW64のobject比較は16通常caseを完了したが、800並列callの測定中にhandleが1個増えFailures=1。同じprocessのbefore / after identity差分でhandle f0 / kernel object0526D040のEventが追加された。SRWのWOW64も4,000混合操作と保護データ確認は成功したが、handle delta1で失敗した。SRW初回には型・identity一覧がなく、その1個をEventやloader lockへ帰属させていない。どちらも失敗として保存した。
- 初回のSIDは値の差ではなく、runnerがnative参考ログ末尾のSidClassificationCases / Result行を除外せず比較したため拒否された。全4組の678値行（660構造体、NULL、17guard境界）は元native参考と一致した。runner側の参考行filterを修正し、初回receiptのFalseは書き換えていない。
- tests/kxnt_srw_probe.cにKXNT_IFEO_SRW_RESOURCE_TRACE時だけ有効なbefore / after snapshotを追加。既存のhandle / kernel object / 型の採取コードを再利用する。defaultのprobeには追加観測がなく、IFEO suiteのSRWだけinstrumented buildにする。元のhandle delta0合否条件は維持した。追加の列挙や関数lookupがloader競合や実行タイミングを変えることを明示する。
- 新規archiveへ再実行。24組すべてのsuiteと2組の通常openが成功し、全child自然終了、driver Failures=0 / exit0、runner Passed。processorの64 +5入力、domainの2,000確保解放、familyの8出力 / 3偽装値、persistedの16入力、SIDのnative値比較、membershipの2,000反復、比較の800並列callとlive identity維持、fileの96case / 2回の1,000反復、alertのNt / Zw各4,096 thread churnと強制終了waiter、counterの4thread / 4,000callずつ / 16unaligned / 10invalid出力、SRWの4,000混合操作と条件変数を確認した。
- traced SRWではWOW64のbefore / after 27個、x64の19個の全handle identityと型を採取し、列挙数とGetProcessHandleCountの一致、追加・削除なしを確認した。二回目の成功から初回の資源差分の原因解明・修正完了を主張しない。初回比較のEvent作成元、SRW初回増加のidentity・作成元は未確認。
- 全26 owned IFEO keyを除去し、候補のuninstall後にdirectory / HKLM marker不在を確認した。source / EXE / 全候補file hashと両回のrawを保存。tests/analyze_kxnt_ifeo_suite.ps1は両receiptを別々に検査し、初回SIDの値一致、比較の追加Event、traced SRWの全identityを解析する。過去のFailedをPassedへ変更しない。

証跡: docs/validation/kxnt-ifeo-core-suite-server.json（InitialFailed / TracedPassed / Analysis）。再実行は build_kxnt_ifeo_probe.ps1 -Architecture x86 -Suite と x64 -Suite の後、run_kxnt_ifeo_vm.ps1 -Suiteへ使い捨てclone VMX / credentials / 新規RunNameを指定する。既存guest fixtureはteardown成功を確認して別の専用archiveへ保存する。解析はanalyze_kxnt_ifeo_suite.ps1へInitialReceipt / TracedReceipt / 未使用Outputを渡す。意味・競合試験と単なるbinding検査を区別する。

今回のbinding表にはUTF、NtWriteFile、SilentExitも含むが、その動作をsuiteで実行したとの証拠にはしない。これらの通常起動での詳細検証、Vista clientのIFEO、native32bit OS、初回資源増加とUTFの残る資源gate、拡張registry options、WNFの実利用・設計は未完了。production DLL / Installer / Releasesはこの工程では変更していない。

## 2026-10-04: WOW64 IFEO の資源差分を native loader lock の Event 格納まで追跡

通常IFEOで見つかった比較 / SRWのハンドル増加を調査した。productionコードを変更せず、専用buildのexport rendezvousで同一processの測定前後を停止し、既存CDB6.12とwow64extsでNtCreateEvent、native critical-sectionの作成結果とCAS格納を採取した。診断用のprivate RVAはnative x86 ntdll SHA256 A3D767B53F36E97DFEFCAD305AE050C98D5282D0107B7D51A36825C25F544506と一致する場合だけ使用する。製品への固定address追加ではない。

- 初回 / 二回目はWOW64初期停止、および旧CDBのeffmachコマンド解析エラーにより専用60秒watchdogへ到達した。Incomplete receiptを保存し、teardown成功を確認した。観測待ちをアプリ成功として扱わない。
- 三回目は両probeが自然終了したが、診断PID parserがログのコマンド文字列をPID行と誤認し、さらにSRWのexit1をdebugger自体の失敗としたためreceiptはIncomplete。rawを別途回収して保存し、過去の状態は変更しない。SRWは4000操作 / 2000書込の値検証に成功、handle delta1でFailures=1。phase0のnative loader lock LockSemaphoreは0、測定中にEvent作成status0、CASのprevious値0でhandle80がslot77750070へ格納され、phase1まで残った。
- native imageの公開LdrLockLoaderLockの逆アセンブルはCS image+e0060をRtlEnterCriticalSectionへ渡している。CS+10のslot、NtCreateEvent後のreturn、CAS後のeax / ecx / edx / slotを照合したため、このSRW processの増加にはnative loader lock Eventのlazy作成という具体的な説明がある。nearest exportのstack名だけをprivate関数名として断定していない。
- PIDを行頭markerに限定し、Wait=OBJECT0の自然終了code0 / 1とprobe自身の合否を別保存して四回目を実行。driver / install / uninstall / owned IFEO cleanupは成功し、receiptはMeasured。比較とSRWはPASS。このSRWでは同じloader lock Event handle70がphase0以前に格納されており、両phaseのslotは70、mixed delta0だった。同じ診断fixtureでも生成時期が違い、初回の資源失敗が修正されたことは証明しない。
- tests/analyze_kxnt_ifeo_event.ps1は保存receipt / log / probe / native imageをhash記録し、phase順序、CS+10、CAS previous0、実handle格納、測定区間との位置関係、delta1 / 0を検査する。tests/kxnt_ifeo_event_trace.cdbとbuild / runnerオプションで採取を再現できる。UninstrumentedSrwはsnapshot列挙を追加せず、EventPhaseTraceは診断exportだけを加える。既存handle delta0のgateは維持した。

証跡: docs/validation/kxnt-ifeo-event-server.json。四回目のreceiptとraw、三回目のIncomplete receipt / raw SRW / binding / trace、二つの初期Incomplete receipt hash、native disassemblyと解析結果を保存。解析はtests/analyze_kxnt_ifeo_event.ps1へInitialArchive（三回目） / MeasuredArchive（四回目） / 未使用Outputを指定する。観測対象はServer 2008使い捨てcloneのWOW64二processで、debuggerは実行時期を変える。歴史的な比較Event、全UTF Event、初回SRWの未採取identityを全面帰属させない。資源gateの全面解決、Vista IFEO、UTF / NtWriteFile / SilentExitの通常起動での詳細動作、拡張registry flags、WNFなど元監査の残りは未完了。配布DLL / Installer / Releasesはこの工程で変更していない。

## 2026-10-04: UTF / Silent Process Exit の通常 IFEO 詳細試験

前工程ではbindingだけだった2機能を、実install / KexCfg / AVRFの通常起動で実行した。build_kxnt_ifeo_probe.ps1 -RuntimeSuiteとrun_kxnt_ifeo_vm.ps1 -RuntimeSuiteを追加した。既存core suiteの範囲は変更せず、追加4image（2機能×両形式）を選択して実行する。UTFでは同じ静的import imageによるlookup-only controlを別processで先に実行し、その後変換probeを実行する。自動retryや資源gate緩和は行わない。

- VS2010で両形式build、dumpbinでntdllの26importと直接KxNt / KexDll import不在を確認。使い捨てServer 2008 cloneを起動し、前工程guest directoryを別名へ保存して新規fixtureを作成した。
- 実OSは6.0.6003 / ProductType3 / NativeArchitecture9、driver自身はKexDll未読込み。4imageの非登録起動はすべてc0000139。候補全fileをinstallし、System32 / SysWOW64とpackage-rootのKexDll / KxNt計8copyを候補とbyte比較した。4imageを実KexCfgで登録し、各mainのIAT26slotと実provider exportのaddress一致・KexDll早期読込み・owner pathを確認した。
- Silent Process ExitのWOW64 / x64は両方PASS。不正 / null / closed / event / thread / self / limited / zero-access handleの9caseについて、同じfixtureのhost native参考と状態・例外・LastErrorが一致（有効selfのstatusだけ期待するNOT_SUPPORTEDへ対応）。両形式1000反復でhandle delta0、ReportingSupported=0。WER報告や終了監視が実装された証拠にはしない。
- UTFの両形式で4850call、24pointer case、18overlap case、全1112064scalarの往復がhost native参考の4893値行と完全一致した。状態、出力長、buffer内容、例外、LastError / LastStatusも比較対象。4threadの各phase16000callで値error0だった。
- ただし両形式とも変換phase0のhandle delta0、phase1のdelta1でFailures=1 / Result=FAIL。lookup-onlyはphase0でdelta1、phase1でdelta0 / CONTROL。cold差分一致・warm delta0の既存gateを両方満たさず、UTFの通常IFEO資源検証と全体receiptはFailed。driverも2probeの失敗を記録しexit1。このcountだけで追加objectをEventやloader lockと断定しない。前のSRW traceや変換値一致を根拠にFAILをPASSへ変更しない。
- 全4childは自然終了し、4owned IFEO profileを除去、実uninstall後directory / HKLM marker不在を確認した。cleanup自体は成功。次は、このIFEO変換processのphase0 / 1におけるobject identity・作成stack・native critical-sectionへの格納を採取し、過去の異なるprocessの結果と分けて検証する。

証跡: docs/validation/kxnt-ifeo-runtime-server.json（原Failed receiptを保持、package / sources / EXE hash、main binding / native参考 / lookup control / 変換raw全件）。docs/validation/kxnt-ifeo-runtime-analysis.json。解析用tests/analyze_kxnt_ifeo_runtime.ps1はReceipt / 未使用Outputを指定し、値比較と資源gateとSilentExitの未対応を別々に検査する。NT6 Server cloneの両形式で通常IFEOに到達したことは示すが、Vista IFEO・native32bit OS・UTF資源問題の修正・WER報告機能の完成は示さない。配布DLL / Installer / Releasesは変更していない。拡張registry flags、ConDrv通常IFEO、WNF実利用・設計と監査の残りは引き続き未完了。

## 2026-10-04: 通常 IFEO UTF の cold Event を同一processの native loader lock まで追跡

前工程の通常IFEO UTF資源FAILを保持したまま、WOW64の専用診断buildへKXNT_IFEO_RESOURCE_PHASESを追加した。各parallel phaseのbefore計数直前とafter計数直後の4停止点で、native loader lockのLockSemaphoreを採取する。default UTF buildとproduction DLLにこの観測はない。run_kxnt_ifeo_vm.ps1 -Utf8Traceは実install / KexCfg / AVRFを経由し、同じfixtureのlookup-only controlとCDB下の変換probeを実行する。private native RVAは既存の正確なntdll hash guardを維持した。

- 使い捨てcloneの前工程directoryを保存し、新しいarchiveで実行。native negative control1imageのc0000139、候補8copyのbyte一致、早期KexDllとIAT26slot一致、debugger自然終了、実uninstallとowned IFEO cleanupを確認した。driver exit0、receiptはMeasured（一般の資源gateのPassedへ昇格しない）。
- このdebugged UTF processでは、loader CS base77a90060 / LockSemaphore slot77a90070のphase0値が0。測定中のx86 NtCreateEventからnative CS allocation return status0 / created handle78 / CS77a90060を採取し、CAS previous0 / ecx78 / edx77a90070 / slot78の一致を確認した。phase1 / 2 / 3もslot78で、二回の混合試験を通して保持された。CSがloader lockである根拠は、前工程で逆アセンブルした公開LdrLockLoaderLockがimage+e0060をRtlEnterCriticalSectionへ渡すことと、この測定imageのhash一致。
- 作成stackにはLdrInitializeThunkが含まれる。ただしprivate symbolsはなくunwind警告もあるため、nearest-export名を精密なprivate関数名として断定しない。NtCreateEventの引数、native内部のreturnとCAS、同一processのslotを証拠の中心にする。
- この試行はparallel phase0 delta1 / phase1 delta0、値errors0 / Failures0。lookup-onlyもphase0 delta1 / phase1 delta0。4850call +24pointer +18overlap +全scalar行の計4893行は前工程の保存host native参考と完全一致。元のnondebug実行は両形式ともphase0 delta0 / phase1 delta1でFAILだったため、今回の生成時期をそれらの過去processへ転用しない。
- tests/analyze_kxnt_ifeo_utf8_event.ps1を追加。4phase順序と同一slot、empty初期値、cold区間内の作成・return・成功CAS、全phaseへのhandle保持、実parallel delta、native値比較を要求する。archive / NativeReference / 未使用Outputを指定する。native imageとreceipt / reference / analyzer hashを記録し、元FAILと今回の観測を別資料として保存する。

証跡: docs/validation/kxnt-ifeo-utf8-event-server.json（raw receipt / trace / main binding / lookup control / 全変換raw / sources・candidate・debugger hash）とkxnt-ifeo-utf8-event-analysis.json。再現はbuild_kxnt_ifeo_probe.ps1 -Architecture x86 -RuntimeSuite -EventPhaseTrace、x64 -RuntimeSuiteでdriverをbuildし、runner -Utf8Traceへ使い捨てVMX / credentials / 未使用RunNameを渡す。CDBは実行timingを変える。このWOW64 processのcold増加をloader lockへ帰属できたが、過去nondebug warm増加、x64の作成元、全Eventのprocess終了時cleanup、一般的なUTF資源gateの完成は未証明。production DLL / Installer / Releasesを変更していない。元監査の未完了範囲は継続する。

## 2026-10-04: ConDrv 内容プロファイルの通常 DLL 書換えに対応

通常IFEOのConDrv詳細試験を追加したところ、private import書換えの以前の成功とは異なり、両形式でprofile-selectedが0だった。実install / KexCfg起動のmapped importは拡張子なしのkxnt。KexDll/redirects.hのDLL_REDIRECT("ntdll","kxnt")とdllrewrt.cのin-place変換がこの表記を生成する。一方、ZigNtIoProfile.hはntdll.dll / KxNt.dllだけを許可していた。この内容判定を修正した。

- boundedかつcase-insensitiveな完全名（NULを含む）の照合でntdll / ntdll.dll / kxnt / kxnt.dllを認める。名前・製品・バージョン・固定addressによるアプリ特例は追加していない。.buildid、元import thunkのNT-I/O3signature、PE構造体bounds、descriptor終端の条件を維持した。名称末尾の余分な文字、画像末尾の未終端文字列は拒否する。拡張子なし5byteの名前が画像末尾へ正しく収まるケースも許可する。
- both PE形式のfixtureにbasename / mixed-case / suffix / tail / unterminated caseを追加。通常IFEOで両形式のmapped kxntとprofile-selected1を確認した。IAT26slotのaddress一致と早期KexDllも確認。候補8copyのbyte一致、native negative control（親2 / 実Zig child2）のc0000139、実KexCfgによる4owned profileの登録・除去、install / uninstallとfresh状態回復を測定した。
- 初回はprofile不選択で広範囲に失敗（WOW64 Failures62 / x64 Failures68）。PowerShell7でのbuild script呼出しがquoted source pathでC1083となり、旧DLLを含む二回目の試行も発生した。そのreceiptは名前fixedだが修正buildの証明ではない。旧DLL hash、コンパイル失敗log、試験FAILをすべて保存し、結果を成功扱いしない。Windows PowerShellで再buildし、成功marker / fatal error不在 / 新DLL hashを確認して配備した。
- 通常起動でKxBaseが先に読み込まれるため、部分書込みを強制する旧診断のKexDll!WriteConsoleW IAT hookは実際の選択経路に届かなかった。診断をimport名でslotを探す方式へ変更し、KxBase読込み時はowned KexDllのGetProcAddress IATだけをhookして、そのmoduleのWriteConsoleW取得だけpartialWriteへ返す。それ以外は元resolverへ委譲し、試験後に元slot / protectionを復元する。fallbackでは従来のWriteConsoleW IATを使用する。productionへの注入hookや成功stubは追加していない。
- 最終resolver試行のWOW64は全詳細試験と通常IFEOの実Zig stdioがPASS。console全39UTF16 units、file / pipe全40bytes、各Zig child終了0を確認した。x64は親のprofile・通常console・file / pipe・guard・衝突拒否・部分6call（UTF8 / DBCS / split-surrogate）・VT・4000並列書込2phase・warm handle delta0 / identity維持が成功。ただし実Zig childのconsoleだけ2回ともexit1 / 内容不一致で、Failures4 / FAILのまま。file / pipe childは両方成功。全体receiptはFailed / driver exit1のまま。
- cold handle deltaは両形式ともadapter1 / unprofiled native control1で一致。warm0とidentity維持を確認した。native controlのcold Event作成stackをこの試験で追跡したとは主張しない。
- KexDllを両形式buildし、作業ブランチInstallerを更新。122named exportsの旧ordinal変更0（最高ordinal318、gapsあり）。ReleaseとInstallerのhash一致を確認し、build由来obj / res / lib / pdbの変更を整理した。原作業ツリーのInstaller、ユーザーVMのsystem DLL、Releasesは変更していない。

証跡: docs/validation/kxnt-ifeo-condrv-server.json（4試行の原receipt、Failed build logs、native Zig import表 / hashと解析）、kxnt-condrv-basename-ordinals.json。tests/analyze_kxnt_ifeo_condrv.ps1は初回 / 最終receiptを比較し、判定修正・両PEの境界・部分注入・資源gateを検査し、x64 childの4失敗を明示する。再現はbuild_kexdll.ps1 / build_kexdll_x86.ps1をWindows PowerShellで実行して成功を確認し、Installer2DLLを更新、build_kxnt_ifeo_probe.ps1の両形式-Suite、runner -ConDrvSuiteへ使い捨てVMX / credentials / 未使用RunNameを指定。childは保存されたZig0.16 smokeのNTDLL importを保持したPE6.0版で、private KxNt import差替え版を使用していない。

この工程で修正・検証できたのは通常書換え名への内容判定とWOW64通常IFEOの詳細動作。x64実Zig console childの失敗は未解決。handle数値のFile / console衝突、子のprofile状態、import経路等は実際のchildで次に測定する必要があり、原因としてまだ断定しない。Vista IFEO / native32bit OS、UTF資源gate、拡張registry flags、WNFなど元監査の残りは未完了。

## 2026-10-04: x64 Zig 通常 IFEO 子プロセスの File / console 数値衝突を実測

通常IFEOのx64 Zig console childだけが失敗する問題を、実際の子プロセスで追跡した。productionコード、元作業ツリーInstaller、ユーザーVMのsystem DLLは変更していない。tests/kxnt_condrv_probe.cにdebug-zigを追加し、console試験だけ既存CDB6.12経由で同じZig imageを起動する。file / pipe試験は通常起動のまま。tests/kxnt_ifeo_deployment_probe.c / run_kxnt_ifeo_vm.ps1のConDrvTraceはx64親だけを実行し、両形式のowned Zig profileを登録・解除する。以前の通常試験の合否条件は緩和しない。

- 初回write traceでKexDll!Ext_NtWriteFileにhandle13、Event / APC routine / context NULL、Length40、Key NULLで入ることを確認。実payloadはKxNt Zig standard-library console smokeと改行で、返却はc00000bb。KexDll / KxNt / KxBaseのSystem32コピーが実childでロードされていた。初回receiptはMeasuredだがtrace内容のrunner gateが弱くZigWriteTracesが空だったため、後続runでraw / hashと自然終了を要求した。初回を後から書き換えていない。
- 次のhandle traceではabsolute load後の!ntsdexts.handleがextension読込みに失敗し、native型の証明にはならなかった。両write entry / returnとchild自然終了は採取できた。全rawを保存し、拡張コマンドの失敗を隠さない。
- extensionに依存しない診断へ変更。製品が実際に呼ぶ公開ntdll!NtQueryObjectで、元writeのhandleとObjectTypeInformation(class2)を条件に停止する。bufferをdebugger pseudo-registerへ保存し、guで実callのreturnを観測、その後元Ext_NtWriteFile callerのreturn addressへ停止する。private RVAや製品名・固定addressは使用しない。MicrosoftのNtQueryObject資料にあるTypeName先頭のUNICODE_STRINGを読み取る。
- 最初のobject traceは旧CDBのMASM条件式で&&が構文エラーとなった。実引数・return・File文字列は採取できたが、ENTRY markerを欠くため完全な診断成功として使わない。原Measured receiptを保持し、後続gateを強化した。nested .ifへ直して新規archiveで再実行した。
- 最終2childで、親が渡すconsole handle13とwrite引数13が一致。native queryもhandle13 / class2 / buffer512bytes、status0を返し、TypeName Length8 / MaximumLength10 / Fileを観測した。queryからのreturn先はKexDll内。続いて元write callerへ戻りc00000bbを確認、両child自然終了1。KexConsoleWriteKindの成功queryがFileならKind2を返すコードと一致し、実childでのFile / console数値衝突が拒否に至る経路を確認できた。native Fileのidentity・名前・用途・作成元は未採取。nearest export名をprivate関数名として断定しない。
- 最終runnerはmain IAT26一致、2child自然終了とentry / type-query / return marker、構文エラー不在、実driver cleanupを要求してMeasured。親の詳細結果はFailures4 / FAIL / Passed=Falseをそのまま保持した。CDB出力自体がconsoleを変更するため、content比較を製品成功の証拠にしない。driver exit0は観測とteardownの成功であり、ConDrv実装のPASSではない。owned IFEO3keyの不在と実uninstall後fresh状態を確認した。

証跡: docs/validation/kxnt-ifeo-zig-write-server.jsonに4runの原receiptと最終解析を保存。tests/analyze_kxnt_ifeo_zig_write.ps1はArchive / 未使用Outputを受け取り、receiptと実logのhash / byte一致、parent consoleとwrite/query handle一致、引数、成功File型query、順序、実write拒否、child終了1とcleanupを検査する。PowerShellのread-only PID変数名を避けた診断parser修正も行った。再現は両形式build_kxnt_ifeo_probe.ps1 -Suiteでfixturesをbuildし、x64 driverをbuild、run_kxnt_ifeo_vm.ps1 -ConDrvTraceへ使い捨てVMX / credentials / 未使用RunNameを渡す。既存guest fixtureはcleanup成功を確認して保存後に実行する。

次は同じ実childのnative Fileの用途と割当て元を確認し、安全なhandle変換を設計する。単にFile / console衝突の拒否を削除したり、標準出力の値を無条件にconsole優先にするとnative Fileの書込みを誤配送するため、既存の衝突拒否試験を維持する。x64通常Zig consoleの修正・uninstrumented成功、Vista IFEO、native32bit OS、UTF一般資源gate、拡張registry flags、WNFと元監査の残りは未完了。

## 2026-10-04: 標準コンソール出力と現在ディレクトリ File の衝突を限定対応

前工程のx64通常Zig child失敗に対し、File型の役割を実測して限定対応を実装した。使い捨てServer 2008 cloneで実install / KexCfg / AVRF / unmodified NTDLL-import Zig imageを使用し、最終検証はデバッガーなしで行った。元作業ツリーの配布物・ユーザーVMのsystem DLL・Releasesは変更していない。

- .extpathをowned guest directoryへ設定してntsdextsのabsolute load / .chainを確認できた。!handle13はWin32 console複製扱いでerror87。低2bitを除いたnative値10を調べる最初のexpression引数も旧extensionが区切りを誤解してUnknown typeとなった。pseudo-registerへ計算してから渡すと、native File / Attributes0 / GrantedAccess100020（同期・探索、書込なし）を両childで確認できた。nameは表示されず、正確なNT object名は採取できていない。これらの原Measured receiptはAPI entry/type/return観測を示すだけで、失敗した名前採取まで成功扱いしない。
- 実childのPEB ProcessParametersを追加観測した。x64のCurrentDirectory.Handle offsetを最初50と誤って指定し、DllPath側の値を表示した原logを保存。00-Common-Headers/NtDll.hの構造体配列に合わせて48へ修正した最終runでは両childともStandardOutput13 / CurrentDirectory.Handle10、CurrentDirectory.DosPathはC:\Windows\。native型照会・整列値・付与access・process parameterの役割を照合した。x64 PEB20 / output28 / path-buffer40 / directory-handle48は診断CFだけで用い、製品コードは構造体のfieldを使う。private fixed addressを製品へ追加していない。作成stack・kernel object identity・正確なnative File名は未採取。
- KexDll/ntcondrv.cのKexConsoleWriteKindで、従来の内容プロファイル、NT6.0、legacy console tag、GetFileType CHAR、VerifyConsoleIoHandleの条件を維持した。native TypeName Fileの場合は、値が現在のStandardOutputまたはStandardErrorと完全一致し、整列native値がCurrentDirectory.Handleと一致し、ObjectBasicInformationが成功してFILE_WRITE_DATA / FILE_APPEND_DATA権限がない場合だけconsole経路を選択する。それ以外のFile衝突、query失敗は従来のNOT_SUPPORTED。Event/APC/Key等の非対応引数の拒否も維持した。製品名・ファイル名・バージョンに依存しない。
- これは内容プロファイルで認識した標準コンソールの意図に基づく限定方針であり、NT6の二つのnamespaceを全て統合する実装ではない。任意のtagged native Fileや、標準出力以外の曖昧なconsoleを一般に解決したとは主張しない。現在ディレクトリFileがwrite accessを持つ場合も拒否する。
- collision試験を拡張し、書込可能Fileのtagged値と実consoleが一致する場面を通常 / StandardOutput設定 / StandardError設定でNtとZw各一回（計6call）実行する。全てNOT_SUPPORTED / IOS未変更で、console内容とFile sizeが不変。設定したstandard handlesは復元する。標準指定だけで衝突を許可しないことを検査した。
- Windows PowerShell5でproduction DLL両形式をbuildし成功markerを確認、両形式Suite fixturesをVS2010でbuild。最終通常IFEOでWOW64 / x64の詳細probe両方Failures0 / PASS。実Zig console39units / exit0を各2回、fileとpipe40bytes / exit0、profileと両PE境界、通常console / bytes / Unicode、部分6call、split surrogate明示error、VT、4000並列書込の2phase、cold adapter1とnative control1一致、warm0と全handle identity維持を確認した。デバッガーによるconsole出力汚染の成功判定ではない。
- main IAT26slot一致、early KexDll、実provider ownerを確認。native negative control4imageのc0000139、候補system / package8copyのbyte一致、owned親2 / Zig child2 IFEO解除、実uninstallとfresh状態回復、driver exit0 / runner Passedを確認した。元のx64 console FAILは記録のまま残し、新しいcandidateでの後続成功と区別する。
- 作業ブランチInstallerのKexDll x64 / x86を更新し、Releaseとのhash一致を確認。build由来obj / res / lib / pdb等の差分を整理した。b8c88bf比較でKxNt / KexDllの両形式export ordinal変更0。KexDllは122named export、既存ordinalを維持した。

証跡: docs/validation/kxnt-condrv-directory-server.json（6runの原receipt、production / fixture build logs、最終解析）、kxnt-condrv-directory-ordinals.json。tests/analyze_kxnt_ifeo_zig_write.ps1は旧Failed childのArchiveに加えて-FixedReceiptを受け取り、実handle metadata / current-directory / standard-handle一致と100020access、後続通常IFEO両形式のstdio、6衝突拒否、static binding、warm資源、cleanupを要求する。UInt64 maskをPowerShellで負数literal扱いしないようMaxValue-3へ修正した。解析結果の成功と製品試験の成功を別々に保存する。

再現はproduction2DLLをWindows PowerShellでbuildしInstallerへコピー、build_kxnt_ifeo_probe.ps1 -Architecture x86 / x64 -Suite、run_kxnt_ifeo_vm.ps1 -ConDrvSuiteへ使い捨てVMX / credentials / 新規RunNameを渡す。-ConDrvTraceは旧失敗経路の観測用なので、修正後に同じ失敗を期待する解析の代わりに使わない。現在ディレクトリへの一致判定に使うnative情報の将来変化、他の曖昧なnamespaceケース、Vista IFEO / native32bit OS、UTF一般資源gate、拡張registry flags、WNFと元監査の残りは未完了。

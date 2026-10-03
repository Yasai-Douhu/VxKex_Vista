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
| ZwCompareObjects / NtCompareObjects 精度改善 | 実装済み | Server 2008 x86 / x64 native 比較・並列検証成功。初回初期化の資源増加は継続確認 |
| 拡張 rename / delete | 未着手 | 未実施 |
| スレッド通知・待機 / Zw 別名 | 未着手 | 未実施 |
| ConDrv 向け NtWriteFile | 未着手 | 未実施 |
| WNF / ZwQueryWnfStateData | 調査段階 | 本家にも未実装があるため実機能の対応を判断する必要あり |
| 既存の未解決 native 転送 | 調査段階 | 呼び出すアプリと API ごとに検証予定 |

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

初回の並列処理ではプロセス内ハンドルが増えた（保存した最新ログは x86 6、x64 7）。比較処理を実行しない対照の thread 起動・終了でも増加 1 を観測した。初期化後の計測区間では数・識別情報・型が一致したが、初回増加の発生元の完全な分類は未完了。これを初回から資源増加ゼロと解釈しない。配布前の確認事項として保持する。

Vista クライアント、native 32bit OS、標準ユーザー / 制限トークン、大規模なハンドル表、native ntdll が 4GB 以上にある配置、他スレッドによる入力ハンドルの close / 再利用競合は未検証。複製後のオブジェクトは保持する設計だが、複製前の入力変更まで atomic な native syscall と同じに扱えると主張しない。

参照結果: `docs/validation/kxnt-compare-reference.json`。VM 出力・DLL / プローブ SHA256: `docs/validation/kxnt-compare.json`。再実行: `tests/run_kxnt_processor_feature_vm.ps1 -Probe compare`。diagnostic 行はポインターや環境固有の初期化を記録し、native 参照の固定値比較から除く。計測前後の識別情報・型は別途 runner 内で厳密に比較している。
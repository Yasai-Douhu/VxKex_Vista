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
| RtlIsCapabilitySid | 未着手 | 未実施 |
| RtlCheckTokenMembershipEx | 未着手 | 未実施 |
| ZwCompareObjects | 未着手 | 未実施 |
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
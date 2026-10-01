# KxNt: VxKex NEXT 2463 から Vista への移植可能性

調査日: 2026-10-02。比較基準は VxKex_Vista の HEAD `24a03ae` と、その時点の作業ツリー、およびローカルの `VxKex_NEXT_Source_Code_1_2_3_2463`。既存の Gemini 関連変更は保持した。

## 結論

**多くの差分は移植可能。ただし KxNt のファイルを丸ごと置き換える方法は適切ではない。** KxNt は主にエクスポートの転送を担い、NEXT の追加機能の実体は KexDll にある。転送先の実装、宣言、ビルド、初期化処理を一緒に移植する必要がある。

特に RTL 補助関数は有力な候補。スレッド通知・待機は Vista の keyed event を使って実装できる見込みがある。一方、WNF は NEXT 側にも未実装処理があり、エクスポートを増やすだけでは機能対応にならない。

今回はソース比較、配布 DLL の転送先検査、Server 2008 VM の読み取り専用プローブまで実施した。互換性コードの変更、機能の移植、移植後の動作検証は実施していない。

## 調査方法と数値の読み方

- 対象: 両プロジェクトの KxNt、関連する KexDll、共通ヘッダー、配布済み DLL。
- ソースの公開名: 有効な `/EXPORT` 宣言と `.def` を比較。32bit と 64bit の宣言を合算した名前の集合であり、単一 DLL のエクスポート数ではない。
- VM: Windows Server 2008 x64、NT 6.0.6003。32bit / 64bit のプローブから `GetProcAddress` で native ntdll の存在を確認。
- バイナリ: VM の System32 / SysWOW64 の ntdll と Installer 内の対応する KxNt を PE エクスポート表で照合。
- API の存在確認と、API の意味・動作の検証は別。Vista クライアント VM での再確認も必要。

再現用スクリプトと結果はローカルの `audit/KxNtAudit/` に保存した。このディレクトリは Git の管理対象外。

## 1. 公開 API の差分

ソース上の公開名は NEXT が **2,181**、Vista が **2,174**。NEXT のみにある名前は **14**。内訳は新たに公開が必要な 10 API と、4 つの Zw 別名である。

| NEXT のみにある公開名 | 現状と判断 |
|---|---|
| `NtAlertThreadByThreadId` / `ZwAlertThreadByThreadId` | Vista の KexDll に未実装関数はあるが、公開・実装が不足。通知基盤ごとの移植が必要 |
| `NtWaitForAlertByThreadId` / `ZwWaitForAlertByThreadId` | 同上 |
| `RtlCanonicalizeDomainName` | NEXT の KexDll 実装を移植可能な見込み |
| `RtlCheckTokenMembershipEx` | NEXT は native 転送。Vista 向け実装を別途作る必要がある |
| `RtlGetDeviceFamilyInfoEnum` | 互換性方針を決めれば移植可能 |
| `RtlGetPersistedStateLocation` | NEXT の既定パスへのフォールバックを移植可能 |
| `RtlIsCapabilitySid` | NEXT は native 転送。SID の分類処理を別途実装可能 |
| `RtlIsPackageSid` | 同上 |
| `RtlIsProcessorFeaturePresent` | NEXT の実装を移植可能な見込み |
| `RtlUnsubscribeWnfNotificationWaitForCompletion` | NEXT は成功を返すだけ。実際の通知停止・完了待機は未実装 |
| `ZwCompareObjects` | Vista の既存 `NtCompareObjects` への別名を追加可能。ただし既存実装にも比較精度の制約がある |
| `ZwQueryWnfStateData` | 別名の追加は可能だが、既存の本体が未実装なので WNF 対応にはならない |

同じ公開名でも、次の転送先が異なる。

| 公開名 | Vista | NEXT |
|---|---|---|
| `NtSetInformationFile` / `ZwSetInformationFile` | native ntdll | `KexDll.Ext_NtSetInformationFile` |
| `NtWriteFile` / `ZwWriteFile` | native ntdll | `KexDll.Ext_NtWriteFile` |

Vista のみにある次の 7 名前は、置換時に失わないようにする。

`LdrpResGetRCConfig`、`NtCancelDeviceWakeupRequest`、`NtFlushBuffersFileEx`、`NtRequestDeviceWakeup`、`NtRequestWakeupLatency`、`WerCheckEventEscalation`、`WerReportWatsonEvent`。

## 2. 機能ごとの移植判断

### RTL 補助関数: 優先して実装できる

**RtlIsProcessorFeaturePresent — 難度: 低**

NEXT は共有ユーザーデータの ProcessorFeatures 配列を範囲確認付きで読む。Vista 向けにも実装できる見込みが強い。未知の番号を成功扱いせず、CPU と OS が実際に利用可能とする値を返す必要がある。

**RtlCanonicalizeDomainName — 難度: 低〜中**

NEXT は IDN 変換、IPv4 / IPv6 解析・整形、大小文字の正規化を組み合わせる。依存する以下の 6 関数は Server 2008 VM の両アーキテクチャに存在した。

`RtlIdnToAscii`、`RtlIdnToUnicode`、`RtlIpv4StringToAddressExW`、`RtlIpv6StringToAddressExW`、`RtlIpv4AddressToStringExW`、`RtlIpv6AddressToStringExW`。

日本語などの IDN、IPv4、IPv6、IPv4 mapped IPv6、ポート、スコープ、長い名前、不正入力、確保した文字列の解放を検証する。Vista の IDN テーブルと新しい Windows の Unicode 対応が同一とは限らない。

**RtlGetDeviceFamilyInfoEnum — 難度: 低〜中**

NEXT のデスクトップ向け固定値は互換性用の近似値であり、そのまま Vista / Server 2008 の実情報とは扱えない。クライアント・サーバーの区別、バージョン偽装との関係、任意の NULL 出力を決めて実装する。公式仕様でも Desktop と Server の family は異なる。[Microsoft の仕様](https://learn.microsoft.com/en-us/windows/win32/devnotes/rtlgetdevicefamilyinfoenum)

**RtlGetPersistedStateLocation — 難度: 低〜中**

NEXT は DefaultPath があれば必要サイズを返してコピーし、なければ見つからない旨を返す。Windows の状態保存先リダイレクト機構全体を実装しているわけではない。このフォールバックなら移植可能。サイズのバイト単位、NULL、短いバッファ、パラメーター検証を確認する。

### SID 分類・トークン検査: 本家からの単純コピーでは足りない

**RtlIsPackageSid / RtlIsCapabilitySid — 難度: 低〜中**

NEXT の実装は native ntdll への転送であり、Server 2008 には転送先が存在しない。SID の形式を検査する関数として独立実装は可能だが、AppContainer 自体を Vista に実装することとは別である。不正な SID、長さ、権限境界を正しく扱う必要がある。

**RtlCheckTokenMembershipEx — 難度: 中**

Vista の通常のトークンに対する検査は既存の membership 機能を利用できる見込みがある。NTSTATUS、PBOOLEAN の ABI、未知のフラグ、制限トークン、deny-only SID、偽装トークンを検証する。AppContainer / LPAC 用のフラグを無条件に通常の検査と同じ成功扱いにしない。[公開ヘッダーの宣言・フラグ](https://github.com/winsiderss/phnt/blob/master/ntrtl.h)

### スレッド通知・待機: 実装可能な基盤あり、統合作業は大きい

**NtAlertThreadByThreadId / NtWaitForAlertByThreadId — 難度: 中〜高**

NEXT は `KexDll/ntalrtid.c` の状態機械と keyed event で実装している。通知先のスレッドとプロセスを確認し、通知先が待機前か待機中かを管理し、タイムアウトとの競合も処理する。

Server 2008 VM では `NtCreateKeyedEvent`、`NtWaitForKeyedEvent`、`NtReleaseKeyedEvent`、`NtOpenThread`、`NtQueryInformationThread` が両アーキテクチャに存在する。したがってカーネル更新なしで実装できる見込みがある。

ただし現在の Vista 版には、NEXT の `kexrtlp.c` の共有 keyed event、`KEX_TEB_EXTENSION`、DllMain のスレッド初期化がない。NEXT が利用する TEB 末尾の領域を Vista / WOW64 にそのまま当てはめてはいけない。Vista の領域を確認するか、プロセス内で管理するスレッド状態を採用する。

必須検証: 待機前の通知、複数通知、タイムアウトと通知の競合、スレッド終了、ID 再利用、別プロセスの ID、不正 ID、32bit / 64bit の負荷試験。単発の成功だけではハングや状態破壊を否定できない。

### 拡張 rename / delete: 基本操作は可能、フラグの意味を守る

**Ext_NtSetInformationFile — 難度: 中**

NEXT は FileDispositionInformationEx / FileRenameInformationEx を旧形式へ変換する。ただし一部の新しいフラグは記録するだけで無視している。通常の削除・置換は移植候補だが、POSIX 動作などを無視して成功と返すと、アプリが要求した操作と実際のファイル状態が食い違う。

対応できないフラグは適切なエラーとし、通常の情報クラスは native に委譲する。可変長ファイル名、RootDirectory、32bit / 64bit の構造体、入力長、不正ポインターを確認する。NEXT の入力長に比例するスタック確保も見直す。[拡張削除フラグの Microsoft 仕様](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/2e860264-018a-47b3-8555-565a13b35a45)

### ConDrv / コンソール書き込み: Vista 用の適応が必要

**Ext_NtWriteFile — 難度: 中〜高**

NEXT は限定された対象でコンソールハンドルへの書き込みを WriteConsoleA に変換する。コンソール判定に NT オブジェクト型名 Console を用いているが、Vista の古いコンソールハンドルでそのまま機能するかは未確認。Vista でのハンドル判定とプロファイルの有効化を作り直す必要がある。

イベント / APC を伴う書き込みなどは native に委譲する構成であり、ConDrv ドライバー全体の実装ではない。同じソースの Ext_NtClose も、今回の NEXT の公開・転送設定では有効化を確認できないため、関数が存在するだけで閉じる処理まで対応済みと判断しない。

標準出力、ファイル、パイプ、無効なハンドル、空データ、部分書き込み、文字コード、非同期経路を分けて確認する。VistaPty は別の用途のアダプターであり、この低レベルの互換処理と同一ではない。

### WNF: NEXT のコード追加だけでは解決しない

**RtlUnsubscribeWnfNotificationWaitForCompletion — 実機能としての移植は保留**

NEXT の本体は STATUS_SUCCESS を返すだけで、購読停止もコールバックの完了待機もしない。他の WNF API にも未実装があり、Vista の NtQueryWnfStateData も未実装。API の存在を増やすことと、WNF を動作させることを区別する。

実際に必要とするアプリがある場合は、状態管理、購読、コールバック、アクセス制御、プロセス間のスコープ、完了待機を含む設計から始める。優先度は、呼び出しと要求される意味を確認して決める。

## 3. 現行 KxNt の native 転送にも未解決箇所がある

今回の VM の ntdll に対し、現行 Installer の KxNt にある named export の native 転送先を照合した。

| アーキテクチャ | KxNt の named export 数 | native に存在しない転送先を持つ公開名 |
|---|---:|---:|
| x64 | 2,015 | 191 |
| x86 | 2,055 | 175 |

これは静的な転送先検査の結果であり、191 / 175 件すべてが現在のアプリで呼ばれて失敗したという意味ではない。オプション機能や別の書き換え経路の影響は、この検査だけでは判断できない。

例: `RtlUTF8ToUnicodeN`、`RtlUnicodeToUTF8N`、`RtlQueryPerformanceCounter`、`RtlQueryPerformanceFrequency`、`RtlTryAcquireSRWLockExclusive`、`RtlTryAcquireSRWLockShared`、`NtOpenKeyEx`、`NtQuerySystemInformationEx`。

本家との差分追加に並行して、実際に対象アプリがインポートする名前から、この既存の転送も整理すべきである。Vista の既存機能で置換できる補助処理と、UMS など OS 側に深く依存する機能は別に評価する。成功を返すスタブで一律に埋めない。

## 4. 推奨する実装順序

1. RTL 補助関数、SID 分類、適切な制約を持つ通常トークンの membership、Zw の別名。小さな API から x86 / x64 の意味を検証する。
2. 拡張ファイル操作。未対応フラグを明示し、通常の native 操作を壊さない。
3. スレッド通知・待機。保存領域・DllMain・keyed event を統合し、競合を含む負荷試験を行う。
4. ConDrv の限定互換。具体的な利用アプリを再現し、Vista のコンソール判定を検証する。
5. WNF は実利用の必要性を確認して別途設計する。

同時に、既存の未解決 native 転送を対象アプリのインポート・実行ログから優先づける。

移植時は KxNt の `.def` / 転送宣言に加え、KexDll の `.def`、共通宣言、プロジェクト・ビルドスクリプト、必要な DLL 書き換えテーブル、初期化・終了処理を確認する。既存の未実装関数と重複させず、x86 の引数と名前修飾、既存の ordinal、Zw 別名を維持する。

## 5. 保存した検証資料

- `audit/KxNtAudit/exports.json`: ソース公開名と転送先の差分。
- `audit/KxNtAudit/native-x86.txt` / `native-x64.txt`: native ntdll の 27 名前の存在検査。
- `audit/KxNtAudit/binary-forwarders.json`: 配布 KxNt の native 転送先検査の全結果。
- `audit/KxNtAudit/compare.py` / `forwarders.py`: 比較スクリプト。
- `audit/KxNtAudit/native-probe.c`: VM で実行した読み取り専用プローブ。

API の存在を確認しただけでは移植後の正しさを保証できない。実装段階では、各 API のエラー、境界条件、競合、既存アプリの回帰を追加検証する。

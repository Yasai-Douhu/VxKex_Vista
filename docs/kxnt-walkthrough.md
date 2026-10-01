# KxNt 移植・検証の進捗

基準: [移植可能性調査](kxnt-next-2463-portability-audit.md)。作業ブランチ: `codex/next-parity-high`。
このファイルを機能ごとに更新し、検証が完了した機能をそれぞれコミット・プッシュする。

## 状態一覧

| 機能 | 実装 | 検証 |
|---|---|---|
| RtlIsProcessorFeaturePresent | 完了 | Server 2008 x86 / x64 成功 |
| RtlCanonicalizeDomainName | 未着手 | 未実施 |
| RtlGetDeviceFamilyInfoEnum | 未着手 | 未実施 |
| RtlGetPersistedStateLocation | 未着手 | 未実施 |
| RtlIsPackageSid / RtlIsCapabilitySid | 未着手 | 未実施 |
| RtlCheckTokenMembershipEx | 未着手 | 未実施 |
| ZwCompareObjects | 未着手 | 未実施 |
| 拡張 rename / delete | 未着手 | 未実施 |
| スレッド通知・待機 / Zw 別名 | 未着手 | 未実施 |
| ConDrv 向け NtWriteFile | 未着手 | 未実施 |
| WNF / ZwQueryWnfStateData | 調査段階 | 本家にも未実装があるため実機能の対応を判断する必要あり |
| 既存の未解決 native 転送 | 調査段階 | 呼び出すアプリと API ごとに検証予定 |

## 2026-10-02: RtlIsProcessorFeaturePresent

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

## 次の作業

RtlCanonicalizeDomainName を実装し、IDN / IPv4 / IPv6 / 不正入力 / 長さ / 解放処理を x86 / x64 で検証する。スレッド通知・待機などの状態管理を伴う機能も、調査の対象範囲として引き続き進める。

# VSCode 1.70.2 / Vista 互換検証

## 確認結果（2026-09-17）

Windows Server 2008 SP2 x64（6.0.6003）の VMware VM で、VSCode **1.70.2 / Electron 18.3.5** のエディター表示とテキストファイルの読込みを確認した。デバッガーなしでも起動する。

対象は `C:\VSCodeUserSetup-x64-1.70.2.exe` から innoextract 1.9 で取り出した x64 本体。**元の x86 Inno Setup インストーラーが Vista 上で完走することは未対応**。別バージョンへの置換は行っていない。

## VM での起動

修正済み DLL と Code.exe の互換設定は検証 VM に反映済み。

```bat
C:\VxKexProbe\Installer\launch-vscode-vista.cmd
```

本体の配置先は `C:\VSCode1702\code$GetDestDir\Code.exe`。起動スクリプトの第 1 引数に別の Code.exe の絶対パスを指定できる。

他の環境へ反映する場合は、アプリを終了してから `Installer\install.bat` のインストールを実行し、管理者として `Installer\configure-vscode-vista.cmd` を実行する。設定は実行ファイル名 Code.exe に適用されるため、同名の別バージョンにも影響する。

検証済みの起動オプション:

```text
--disable-gpu --disable-gpu-sandbox --disable-software-rasterizer --use-gl=disabled
```

GPU の sandbox を無効化するため通常の対応 OS より隔離が弱い。WebGL と GPU 描画は使用できない。`--disable-gpu` だけでは SwiftShader 初期化が残り、今回の VM では通常起動が止まる。WebGL を使用しないエディター画面は上記設定で描画できた。

## 修正の要点

| 箇所 | 原因と修正 |
| --- | --- |
| KexDll / DLL import 書換え | kernel32 の 2 番目の import が ntdll という前提を廃止。Vista では終端要素を参照して PE ヘッダーへ書き込んでいた。名前で検索する。 |
| KxNt | Vista の kernel32 が参照する 6 個の ntdll export を転送する。 |
| KxBase / SRW lock | 共有所有者数のビット配置を修正。ネイティブの Acquire/Release と併用できるようにした。 |
| KxBase / 名前付きオブジェクト | Vista のセッション名前空間を開き、既存の section DACL 保持処理を有効化。共有メモリの読取り専用ハンドルから書込み権限へ昇格できないことを検証。 |
| KxBase / プロセス属性 | 既存の対応ポリシーへの絞込み後、値がゼロの場合だけ Vista で属性登録を省略する。非ゼロのポリシーをこの修正で成功扱いにはしない。 |
| KxBase / API | スレッド affinity、locale 解決、fail-fast 等の互換 export と PSAPI 転送先を追加。power request は未対応エラーを返す。 |
| KxUser / shell32 | AppUserModelID のプロセス内保持と取得、DisplayConfig 未対応エラーを実装し、shell32 を転送対象に追加。Vista のタスクバーに新しいグループ化機能を追加するものではない。 |
| Installer | 子プロセス用 verifier を custom provider のみに設定。DWrite 依存 DLL を Kex64 に同梱。 |

`dwrw10.dll` は既存の VxKex-NEXT 同梱バイナリーを使用する。依存 import の互換書換えが必要なので System32 ではなく `C:\VxKex\Kex64` に配置する。今回の VM にある UCRT 等の依存関係を前提とし、未更新の Vista への新規導入は未検証。

## 検証

- `tests/vista_compat_probe.c`: 21 項目 PASS。ネイティブ共有ロックとの相互運用、4 スレッド各 10,000 回の競合、affinity、locale、AppUserModelID、空の mitigation 属性、section DACL を確認。
- `C:\VerCheck.exe`: VerifyVersionInfo / GetVersionEx / RtlGetVersion / Registry の全 4 項目 PASS。
- VSCode: Get Started、テキストの表示、通常起動を確認。拡張機能全般、統合ターミナル、デバッグ実行、長時間の安定性は保証していない。
- 詳細ログ・ダンプ・画面はローカルの `audit\vscode`。`screen_standalone2.png` に読込み結果と 1.70.2 の About 表示を保存。

## ビルド

VS2010 x64 compiler と Windows SDK 7.1 を使用する。PowerShell 7 の引数引用規則との差を避けるため、Windows PowerShell (`powershell.exe`) で実行する。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build_kexdll.ps1 -OutputDirectory audit\release\KexDll
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build_kxbase.ps1 -OutputDirectory audit\release\KxBase
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build_kxnt.ps1 -OutputDirectory audit\release\KxNt
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build_kxuser.ps1 -OutputDirectory audit\release\KxUser
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\build_vista_compat_probe.ps1
```

試験 EXE は VxKex を適用した VM で実行する。ホスト OS では実行しない。

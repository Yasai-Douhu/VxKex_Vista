更新内容

- Windows Vista の強いバージョン偽装時に `GetTickCount` / `GetTickCount64` が停止し、Inno Setup のUAC起動が待機し続ける問題を修正しました。QPCを利用した互換タイマーを追加しています。
- `VistaRun.exe` が対象プロセス終了前に終了し、Inno Setup のSpawnServer通信が切れる問題を修正しました。
- x86/x64 Installer バイナリを再ビルド・更新しました。

検証

- Vista VMでGit 2.55.0.5を通常起動し、UAC承認後にセットアップ画面が表示されることを確認しました。
- x86/x64の強いバージョン偽装タイマープローブが成功しています。

SHA-256は同梱の `VxKex_Vista-1.2.0.2230-SHA256.txt` を参照してください。

# VxKex Vista 2.0.0.2233

VxKex NEXT 1.2.3.2463 の KxNt 差分から、Vista / Server 2008 で実現できる API と互換処理を移植しました。

## 主な変更

- スレッド ID による通知・待機、ドメイン名の正規化、デバイス情報、状態保存先の既定パス、SID 分類、通常トークンの membership 判定、processor feature 判定、Zw 別名を追加。
- 拡張 rename / delete を旧情報クラスへ変換。対応できない flags を明示的に拒否します。
- UTF-8 / UTF-16 変換、performance counter、SRW try-lock、NtOpenKeyEx の未解決 native 転送を修正。
- Zig の legacy console 書き込みに対する限定 ConDrv 互換を追加。ファイル・パイプの通常動作を維持します。
- Vista 固有の既存エクスポート7名を維持。両アーキテクチャのビルドとバージョンリソースを修正しました。
- 設定 GUI、ログビューア、トランザクション対応インストーラー、VistaPty などの既存機能を同梱します。

## 検証範囲

機能ごとの代表正常系、主要な異常系・境界条件、x64 / WOW64、Vista / Server 2008 の差、通常 IFEO 経路と実 Zig の確認を実施しました。機能別の実装方法・証跡・回帰試験結果は [kxnt-walkthrough.md](https://github.com/Yasai-Douhu/VxKex_Vista/blob/codex/next-parity-high/docs/kxnt-walkthrough.md) に記録しています。

## 既知の制限

- WNF、AppContainer / LPAC、UMS、ConDrv 全体、実 WER reporting は今回の移植範囲ではありません。成功だけを返す stub を機能対応として追加していません。
- registry の backup / restore など未対応 options、POSIX ファイル操作などは明示的な未対応エラーになります。
- native OS の遅延確保や診断による有限の handle 差は記録しています。反復変換での継続増加は確認されていません。
- native 32bit OS は未検証。x86 の動作確認は x64 OS 上の WOW64 です。
- 任意のアプリケーションやすべての新しい native API の互換性を保証するものではありません。

## インストール

ZIP を展開し、同梱の `install.bat` を実行してください。UAC の確認を承認してインストールします。既存設定の保持・復元は同梱のインストーラーで扱います。

タグと配布版番号は `v2.0.0.2233` / `2.0.0.2233` です。ZIP の SHA256 は同梱するチェックサムファイルで確認できます。

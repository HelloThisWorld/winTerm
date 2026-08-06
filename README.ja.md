[English](README.md) | 日本語

# winTerm

[![Validation](https://github.com/HelloThisWorld/winTerm/actions/workflows/winterm-validation.yml/badge.svg)](https://github.com/HelloThisWorld/winTerm/actions/workflows/winterm-validation.yml)
[![Windows CI](https://img.shields.io/github/actions/workflow/status/HelloThisWorld/winTerm/winterm-validation.yml?branch=main&label=Windows%20CI)](https://github.com/HelloThisWorld/winTerm/actions/workflows/winterm-validation.yml)
[![Latest release](https://img.shields.io/github/v/release/HelloThisWorld/winTerm?display_name=tag&label=release)](https://github.com/HelloThisWorld/winTerm/releases/latest)

日本語の製品ページ: <https://winterm.dev/ja/>

## winTermをダウンロード

[**最新の安定版リリースページを開いて、x64版のwinTermをダウンロードする**](https://github.com/HelloThisWorld/winTerm/releases/latest)

最新の安定版リリースページには、インストーラー（Setup EXE）、ポータブルZIP、
リリースノート、チェックサムがまとめて掲載されています。

Command Timelineを先行して試せる**ベータ版**のチャンネルもあります。
[v1.3.0-beta3](https://github.com/HelloThisWorld/winTerm/releases/tag/v1.3.0-beta3)
はGitHubのプレリリースとして公開されており、資産の構成は安定版と同じです。
ベータ版は動作確認を目的とした配布のため、通常の利用には上記の安定版を推奨します。

インストーラーは署名されていないため、Windowsに「不明な発行元」または
SmartScreenの警告が表示される場合があります。必ず上記の公式リリースから
ダウンロードし、同梱の `SHA256SUMS.txt` を確認してから実行してください。

winTermは、`helloThisWorld` が公開している独立したオープンソースの
Windows 11向けターミナルです。Microsoft Terminalのソースコードを基にしていますが、
Microsoftの製品ではなく、Microsoftによる提供・承認・支援を受けたものでもありません。
Microsoft、Windows、Windows Terminalのロゴも使用していません。

## 配布形式

主な配布形式は、アンパッケージ形式で自己完結したInno Setup製のEXEです。
ポータブルZIPも提供しています。バージョンを `<version>` とすると、
推奨されるアプリケーションのダウンロードは次の2つだけです。

- `winTerm-<version>-setup-x64.exe` — 現在のユーザー、または全ユーザーへのインストール用;
- `winTerm-<version>-portable-x64.zip` — 展開してそのまま実行する用。

現在のソースバージョンは `1.3.0-beta3` で、
最新の安定版リリースは `1.2.0` です。公開されている資産の一覧とチェックサムの全体は、
[最新の公式リリース](https://github.com/HelloThisWorld/winTerm/releases/latest)
を参照してください。

リリースが署名されていない場合は、そのリリースノートに明記されています。
Windowsに「不明な発行元」またはSmartScreenの警告が表示される場合があるため、
同じリリースに含まれる `SHA256SUMS.txt` で検証してください。
リリースEXEのインストールに、証明書のインポート、開発者モード、Visual Studio、
Windows SDK、`Add-AppxPackage` は一切必要ありません。

[インストール手順（英語）](docs/user/installation.md)と
[1.2.0のリリースノート（英語）](docs/releases/1.2.0.md)もあわせて参照してください。

## 主な機能

- ペインごとの**Command Timeline**（`Ctrl+Tab`、またはターミナル左端の細い
  ハンドル）: そのペインで実行したコマンドをメモリ上に保持する一覧です。
  OSC 133のシェル統合の情報だけから構築され、実行せずに入力欄へ読み込む操作、
  大文字と小文字を区別しない文字列での絞り込み、コマンドや出力のコピー、
  出力位置への移動に対応し、推測を行わない ✓／✕／Running の状態表示を備えます;
- PowerShellの自動的なシェル統合: 素の `powershell.exe` または `pwsh.exe`
  プロファイルは、起動時に同梱の `winTerm.Shell` モジュールを読み込みます
  （プロファイルごとの設定 `"shellIntegration.autoInject"`、既定は有効）。
  そのためコマンドの区切りが最初から機能します。独自に指定された
  コマンドラインが書き換えられることはなく、`cmd.exe` を推測することもありません;
- フォーカス中のペインを基準にした上・下・左・右への分割。プロファイルの選択を
  引き継ぎ、失敗時はトランザクションとしてロールバックします;
- 境界のドラッグによるペインのサイズ変更。連続的な更新、最小サイズの制約、
  4分の1・3分の1・50%のスナップ位置に対応します;
- Altキーを併用した自由なサイズ変更、安定したスナップのヒステリシス、
  正確なサイズ変更の取り消しとやり直し、そして1コマンドで実行できる
  **Balance Panes**（ペインのバランス調整）;
- ペインのアイコン、タイトル、実際の状態テキスト、統一されたオーバーフロー
  メニューを備えたコンパクトなペインヘッダー。ペインの位置を入れ替えるための
  操作要素は持ちません;
- ウェブサイトと調和したダークなネイティブウィンドウ、タブストリップ、
  ペイン表面、細い青灰色の分割線、ミントカラーのフォーカスとサイズ変更の表示;
- ペインのサイズ変更とアプリケーションUIの設定、キーボードによるサイズ変更
  コマンド、ハイコントラストに対応した分割線の表示、スクリーンリーダー向けラベル;
- ペインごとのVisual Progress。標準のOSC進捗、シェルのライフサイクル状態、
  一般的なCLIの進捗表示の範囲を限定したローカルな認識に対して、
  Rainbow Arc Weldのオーバーレイを表示します。描画は環境に応じて調整され、
  状態はアクセシビリティに配慮して読み上げられます;
- `%LOCALAPPDATA%\winTerm` を使う独立したデータ領域と `winterm.exe` の
  コマンド登録。Windows Terminalや `wt.exe` を置き換えることはありません;
- PowerShell、CMD、WSLプロファイルの動的な検出、テーマ、アプリ内蔵フォント、
  ワークスペース、スナップショット、診断、複数行の貼り付け保護。

Windowsのファイル名は大文字と小文字を区別しないため、製品名の表記に合わせた
実行ファイル `winTerm.exe` は `winterm.exe` としてもそのまま起動できます。
インストーラーは `PATH` を全体的に書き換えるのではなく、App Pathsのエントリを使用します。

## ポータブルモード

ポータブルZIPの中身をすべて書き込み可能なディレクトリへ展開し、
`winTerm.exe` を実行してください。同梱の `portable.marker` があると、winTermは設定、
テーマ、ワークスペース、スナップショット、ログ、更新データを、隣接する `data`
ディレクトリ以下に保存します。このマーカーを削除すると、データの保存先は
`%LOCALAPPDATA%\winTerm` に切り替わります。

ポータブルモードは、レジストリを変更せず、ショートカットを作成せず、
アンインストーラーを登録せず、管理者権限も必要としません。

## ビルドとテスト

PowerShell 7と、[ビルド手順（英語）](docs/build.md)に記載されたMicrosoft Terminalの
ツールチェーンを使用します。

```powershell
.\scripts\winterm\build.ps1 -Configuration Release -Platform x64 -IncludeTests
.\scripts\winterm\test.ps1 -Suite Relevant -Configuration Release -Platform x64
.\scripts\winterm\build-unpackaged.ps1 -Configuration Release -Platform x64
.\scripts\winterm\build-installer.ps1 -Version 1.3.0-beta3 -Platform x64
.\scripts\winterm\build-portable.ps1 -Version 1.3.0-beta3 -Platform x64
```

アンパッケージ形式の生成処理では、統合されたリソースインデックスを作るための
アップストリーム由来のビルド中間物として、署名なしのMSIXのみを使用します。
MSIXは、ステージ、リリースの許可リスト、利用者向けの主要なインストール経路には
一切含まれません。

## プライバシーとセキュリティ

winTermは、コマンド内容、ターミナル出力、クリップボード内容、ワークスペース内容、
作業ディレクトリ、利用状況を収集しません。クラッシュレポートの送信は既定で無効の
オプトインです。Visual Progressの認識処理はローカルで完結し、範囲を限定した
メモリ上の処理として行われます。ターミナルの内容を送信したり保存したりすることは
ありません。詳細は [PRIVACY.md（英語）](PRIVACY.md)、
[SECURITY.md（英語）](SECURITY.md)、[SUPPORT.md（英語）](SUPPORT.md)を参照してください。

## コード署名ポリシー

Free code signing provided by SignPath.io, certificate by SignPath Foundation.

正式なポリシーは [CODE_SIGNING_POLICY.md（英語）](CODE_SIGNING_POLICY.md)です。

### 役割

- 作成者、コミッター、レビュアー:
  [HelloThisWorld](https://github.com/HelloThisWorld)
- 承認者:
  [HelloThisWorld](https://github.com/HelloThisWorld)

公式のリリース資産は、この公開リポジトリから、変更されないリリースタグを起点として、
GitHub Actionsのマネージドランナー上でビルドされます。

リリースの署名要求には、毎回手動での承認が必要です。

**現在の状態:** 最新の公開リリースはAuthenticode署名されていません。SignPathの証明書は
まだ発行されていません。正式な状態は
[CODE_SIGNING_POLICY.md（英語）](CODE_SIGNING_POLICY.md)を参照してください。

winTermのプライバシーポリシーは [PRIVACY.md（英語）](PRIVACY.md)を参照してください。

## ライセンスとアップストリーム

winTermは、Microsoft TerminalのMITライセンス、著作権表示、サードパーティ通知を
そのまま維持しています。固定しているアップストリームの基準は
`release-1.25@1cea42d433253d95c4487a3037db48197b5e72f4` です。

[LICENSE](LICENSE)、[NOTICE.md（英語）](NOTICE.md)、
[THIRD_PARTY_NOTICES.md（英語）](THIRD_PARTY_NOTICES.md)、
[アップストリームの同期（英語）](docs/upstream-sync.md)を参照してください。

## サポートと関連ドキュメント

不具合の報告や機能の要望は
[GitHubのIssue](https://github.com/HelloThisWorld/winTerm/issues)へお寄せください。
サポート方針は [SUPPORT.md（英語）](SUPPORT.md)、
セキュリティ上の問題の報告手順は [SECURITY.md（英語）](SECURITY.md)に記載しています。

利用者向けのドキュメントは `docs/user/` にあります。
[はじめに（英語）](docs/user/getting-started.md)、
[Command Timeline（英語）](docs/user/command-timeline.md)、
[キーボードショートカット（英語）](docs/user/keyboard-shortcuts.md)、
[設定（英語）](docs/user/settings.md)、
[アンインストール（英語）](docs/user/uninstall.md)などが含まれます。

リポジトリのドキュメントとアプリケーションのUIは、現時点ではすべてが日本語で
提供されているわけではありません。アプリケーションの表示言語はWindowsの言語設定に
従い、日本語も選択できますが、winTerm固有の新機能では一部の表示が英語のままです。
このREADMEの日本語訳は、英語版 [README.md](README.md) を正本として同期しています。

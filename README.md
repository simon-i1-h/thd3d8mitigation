# thd3d8mitigation

Direct3D 8の非互換性緩和レイヤーです。Windows 10で行われたDirect3D 8の破壊的変更を、Direct3D 8を用いて作られた東方Projectのゲームを遊ぶのに必要な分だけ緩和します。

**注意: そうは言っても通常プレイの範疇外なので、節度を守って使ったり、本プログラムの使用を明記したりすることをおすすめします。また、本プログラムを適用した上で発生した問題を元のゲームの作者に報告するのは避けた方がよいでしょう。**

現在は主に、フルスクリーンモードの東方紅魔郷がWindows 10で高速化する問題を緩和する効能があります。

本プログラムは特定のゲームに依存しない汎用的なものです。特定のゲームの中身に関するコードは含まれていません。

## ライセンスおよび免責事項

MIT License。詳細はLICENSE.txtを参照のこと。

## インストール方法

1. d3d8.dllとthd3d8mitigationcustom.exeをコピーして、ゲームの実行ファイルがあるフォルダーに貼り付けます。
2. ゲームを起動します。
3. お楽しみください。

環境によっては必要なプログラムが足りず起動に失敗することがあります。その場合はVisual Studio 2019 用 Microsoft Visual C++ 再頒布可能パッケージ (x86)をインストールしてみてください。 https://support.microsoft.com/ja-jp/help/2977003/the-latest-supported-visual-c-downloads

## アンインストール方法

1. インストール時にゲームの実行ファイルがあるフォルダーに貼り付けたd3d8.dllとthd3d8mitigationcustom.exeを削除します。
2. 該当ゲームの設定ファイルと同じフォルダーに生成されたthd3d8mitigationcfg.iniとthd3d8mitigationlog.txtを削除します。
3. おつかれさまでした。

## 設定

thd3d8mitigationcustom.exeで設定できます。設定は独自の設定ファイルに保存されます。また、thd3d8mitigationをインストールした状態でゲームを起動すると、設定ファイルが無い場合は自動で設定ファイルが生成されます。

設定ファイルの場所は該当ゲームの設定ファイルと同じフォルダーにあるthd3d8mitigationcfg.iniです。

(正確には、該当ゲームと同様に実行ファイルと同じフォルダーにある設定ファイルの使用を試みます。しかし、Windows Vista以降かつ該当ゲームがC:\\Program Files (x86)\\配下やC:\\Program Files\\配下などにインストールされている場合、UACの仮想化という機能によりVirtualStoreフォルダー(大抵はC:\\Users\\「ユーザー名」\\AppData\\Local\\VirtualStore\\)配下のファイルを使用します。)

メモ帳などで設定ファイルを直接開いて、設定を確認および変更することもできます。

### presentationセクション

#### wait_forオプション

主に、東方紅魔郷がWindows 10で高速化する問題を緩和するためのオプションです。初期設定はautoです。フルスクリーンモード下での表示処理について、元来は待機処理を行う箇所で行う追加の待機処理を設定します。

- auto: とりあえず動けばいいという場合に推奨されます。環境に合わせて最適な設定を自動で設定します。この設定を設定すると起動時間が少し長くなります。
- vsync: Windows 10でプレイする際に推奨されます。通常の処理に加え、独自の方法で垂直同期信号の待機を試みます。
- normal: Windows 10以外の環境(Windows 10より前のWindowsや、WineあるいはProton等の互換レイヤーなど)でプレイする際に推奨されます。特に何もせず通常通りの処理のみを行います。

垂直同期信号が60Hzより高い環境では、最初に東方紅魔郷の「強制的に６０フレームにする」設定を有効にすることをおすすめします。

## バージョンの確認

thd3d8mitigationcustom.exeのウインドウタイトルに表示されます。また、ゲームの起動時に、該当ゲームの設定ファイルと同じフォルダーに生成されたthd3d8mitigationlog.txtに記録されます。

## 動作確認

- Windows 10 Version 2004 東方紅魔郷 v1.02h Normal 霊夢A: 動作する
- Windows 10 (Build 20197) 東方紅魔郷 v1.02h Easy、Normal 霊夢A、霊夢B: 動作する
- Windows 10 (Build 20257) 東方紅魔郷 v1.02h Extra 霊夢B: 動作する
- Windows 7 Service Pack 1 東方紅魔郷 v1.02f Normal 霊夢A: 動作する
- Windows XP Service Pack 3 東方紅魔郷 v1.02f Normal 霊夢A: 動作する
- Ubuntu 20.04 Wine 5.0 東方紅魔郷 v1.02f Normal 霊夢A: 動作する
  - winecfgでd3d8.dllについて内蔵版よりネイティブ版を優先する設定にすることでthd3d8mitigationを適用することが可能。
  - 設定の自動検出がうまく働かないため、手動での設定をおすすめします。

作者のシューティング力が不足しているため、HardとLunaticでの動作確認は全く取れていません。

## その他資料

- https://postfixnotation.org/blog/1

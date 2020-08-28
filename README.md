# thd3d8mitigation

Direct3D 8を用いて作られた東方Projectのゲームを元来の動作に近づけることを目指しているDirect3D 8の互換レイヤーです。Direct3D 8のラッパーとして動作します。

**注意: そうは言っても通常プレイの範疇外なので、節度を守って使ったり、本プログラムの使用を明記したりすることをおすすめします。また、本プログラムを適用した上で発生した問題を元のゲームの作者に報告するのは避けた方がよいでしょう。**

現在は主に、フルスクリーンモードの東方紅魔郷がWindows 10で高速化する問題を緩和する効能があります。

## ライセンスおよび免責事項

MIT License。詳細はLICENSE.txtを参照のこと。

## ビルド方法

必要なもの:

- Windows 10
- Visual Studio 2019 (CommunityエディションでOK)
- DirectX SDK August 2007 (https://www.microsoft.com/en-us/download/details.aspx?id=13287)
  - DirectX 8(Direct3D 8を含む)をサポートする最後のSDK
  - SDKのインストールディレクトリはデフォルト(C:\\Program Files (x86)\\Microsoft DirectX SDK (August 2007))で問題ないはずです。詳しくはプロジェクトファイルをご覧ください。

Visual Studioを起動してソリューションファイルを開きます。開いたら構成をDebugではなくReleaseにします。そして、\[ビルド\]→\[ソリューションのビルド\]でビルドします。ビルドの成果物はReleaseフォルダーに出力されます。

## インストール方法

1. d3d8.dllをコピーして、ゲームの実行ファイルがあるフォルダーに貼り付けます。
2. ゲームを起動します。
3. お楽しみください。

## アンインストール方法

1. インストール時にゲームの実行ファイルがあるフォルダーに貼り付けたd3d8.dllを削除します。
2. 該当ゲームの設定ファイルと同じフォルダーに生成されたthd3d8mitigationcfg.iniとthd3d8mitigationlog.txtを削除します。
3. おつかれさまでした。

## 設定

thd3d8mitigationをインストールした状態でゲームを起動すると、設定ファイルが無い場合は設定ファイルが生成されます。該当ゲームの設定ファイルと同じフォルダーにthd3d8mitigationcfg.iniという設定ファイルがあるはずです。メモ帳などで設定を確認および変更できます。

(正確には、実行ファイルと同じフォルダーに設定ファイルの作成を試みます。そのため、通常はゲームの設定ファイル等を含めそのフォルダーに作成されるのですが、Windows Vista以降はC:\\Program Files (x86)\\配下やC:\\Program Files\\配下などにインストールされている場合、UACの仮想化という機能によりVirtualStoreフォルダー(大抵はC:\\Users\\「ユーザー名」\\AppData\\Local\\VirtualStore\\)配下に作成されます。)

### presentationセクション

#### wait_forオプション

主に、東方紅魔郷がWindows 10で高速化する問題を緩和するためのオプションです。初期設定はautoです。フルスクリーンモード下での表示処理時に行う追加の待機処理の方法を設定します。

- auto: とりあえず動けばいいという場合に推奨されます。環境に合わせて最適な設定を自動で設定します。この設定を設定すると起動時間が少し長くなります。
- vsync: Windows 10でプレイする際に推奨されます。通常の処理に加え、独自の方法で垂直同期信号の待機を試みます。
- normal: Windows 10以外の環境(Windows 10より前のWindowsや、WineあるいはProton等の互換レイヤーなど)でプレイする際に推奨されます。特に何もせず通常通りの処理のみを行います。

垂直同期信号が60Hzより高い環境では、最初に東方紅魔郷の「強制的に６０フレームにする」設定を有効にすることをおすすめします。

## バージョンの確認

ゲームの起動時に、該当ゲームの設定ファイルと同じフォルダーに生成されたthd3d8mitigationlog.txtに記録されます。

## 動作確認

- Windows 10 (Build 20197) 東方紅魔郷 v1.02h Easy、Normal 霊夢A: 動作する
- Windows 10 Version 2004 東方紅魔郷 v1.02h Normal 霊夢A: 動作する
- Windows 7 Service Pack 1 東方紅魔郷 v1.02f Normal 霊夢A: 動作する
- Windows XP Service Pack 3 東方紅魔郷 v1.02f Normal 霊夢A: 動作する
- Ubuntu 20.04 Wine 5.0 東方紅魔郷 v1.02f Normal 霊夢A: 動作する
  - winecfgでd3d8.dllについて内蔵版よりネイティブ版を優先する設定にすることでthd3d8mitigationを適用することが可能。
  - 設定の自動検出がうまく働かないため、手動での設定をおすすめします。

作者のシューティング力が不足しているため、Hard、Lunatic、Extraでの動作確認は取れていません。

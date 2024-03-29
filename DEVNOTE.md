# ビルド方法

必要なもの:

- Windows 10
- Visual Studio 2019 (CommunityエディションでOK)
- DirectX SDK August 2007 (https://www.microsoft.com/en-us/download/details.aspx?id=13287)
  - DirectX 8(Direct3D 8を含む)をサポートする最後のSDK
  - SDKのインストールディレクトリはデフォルト(C:\\Program Files (x86)\\Microsoft DirectX SDK (August 2007))で問題ないはずです。詳しくはプロジェクトファイルをご覧ください。

Visual Studioを起動してソリューションファイルを開きます。開いたら構成をDebugではなくReleaseにします。そして、\[ビルド\]→\[ソリューションのビルド\]でビルドします。ビルドの成果物はReleaseフォルダーに出力されます。

# thd3d8mitigation

## 設計

ゲームが利用するd3d8.dllよりも優先度が高いDLL検索パスに同名のDLLを置くことで、ゲームのDirect3D 8 API呼び出しに割り込む。

IDirect3D8およびIDirect3DDevice8オブジェクトを作成するAPI呼び出しに割り込んで、各オブジェクトのメソッドの処理を適宜差し替える。

IDirect3D8およびIDirect3DDevice8オブジェクトに1対1で紐付く拡張プロパティをエクストラデータと呼び、各オブジェクトの作成と解放に合わせて作成と解放を行う。エクストラデータはエクストラデータテーブルという領域に格納されている。エクストラデータは主に紐付くオブジェクトのメソッド内で適宜便利に利用する。

## エラー処理

データファイルの破壊を防ぐため、大半のエラーは即座にクラッシュさせず、呼び出し元に報告するか、ログを出力したあと呼び出し元にNULLやFALSEなどの否定的な値を返す。ただし、ヒープメモリーの確保エラーとC++コードでの想定外の例外は(スタックオーバーフローなどと同様に)回復不能と見做して即座にクラッシュさせる。

## 排他制御

クリティカルセクション(cs_接頭辞が付く関数)やクリティカルセクションオブジェクトを除くグローバル変数(ほとんどの場合g_接頭辞が付く変数)を呼び出したり参照したりする場合、呼び出し側はクリティカルセクションであるかクリティカルセクションオブジェクトを通してEnterCriticalSectionおよびLeaveCriticalSectionで排他制御を行う必要がある。現状、クリティカルセクションオブジェクトはg_CSのみである。

IDirect3D8およびIDirect3DDevice8オブジェクトのメソッド内について、エクストラデータの排他制御の責任は紐付くオブジェクトと同様呼び出し元にあると考えられるので、紐付くオブジェクトのメソッド内ではエクストラデータのデータ競合や競合状態について考えなくてよい。

```
HRESULT __stdcall ModIDirect3DDevice8MethodFoo(IDirect3DDevice8* me)
{
	/*
	 * me_exdataはmeに1対1で紐づく拡張プロパティであり、meの排他制御の責任はmeのメソッドの呼び出し元にあるので、
	 * meのメソッド内ではme_exdataのデータ競合や競合状態について考えなくてよい。
	 */
	struct IDirect3DDevice8ExtraData* me_exdata;

	EnterCriticalSection(&g_CS);
	me_exdata = IDirect3DDevice8ExtraDataTableGet(g_D3DDev8ExDataTable, me);
	LeaveCriticalSection(&g_CS);

	/* これ以降、この関数内ではme_exdataのメンバーを排他制御なしで読み書きしてよい。 */
```

## 時刻およびカウンター

timeGetTimeを使っている箇所はオーバーフローを考慮していないので、Windowsを起動してからだいたい49日を超えるとおかしくなる。少なくとも紅魔郷は似たような仕様があるはずなのでおそらく問題ないが、他の作品での影響は不明。

## 文字コード

文字列はANSI文字列。

thd3d8mitigationのソースコードはBOM付きUTF-8で書かれているが、実行環境のコードページがUTF-8のものとは限らないので、文字列リテラルに非ASCII文字を含むべきではない。

ビルドに使用するファイル(ソースファイルやプロジェクトファイルなど)の文字コードはASCIIもしくはBOM付きUTF-8とする。.clang-formatや.gitignoreなどのその他の設定ファイルやドキュメントは(BOM無し)UTF-8とする。

改行コードはCRLFとする。

## 命名規則

- cs_\*: クリティカルセクション。呼び出し側はクリティカルセクションであるかEnterCriticalSectionおよびLeaveCriticalSectionで排他制御を行う必要がある。
- g_\*: 代入可能なグローバル変数。CRITICAL_SECTION型を除いて、呼び出し側はクリティカルセクションであるかEnterCriticalSectionおよびLeaveCriticalSectionで排他制御を行う必要がある。
- \*_t: 主にtypedefした型識別子。
- Mod\*: この接頭辞に続く名前の処理に割り込んで追加の処理を行う関数。
- V\*: va_listを引数に取る関数。va_listではなく可変長引数を取る版の関数から呼び出されることが多い。
- tm_\*: 呼び出し側はtimeBeginPeriodおよびtimeEndPeriodで一時的にタイマーの精度を上げるのが望ましい。

## その他

- 特にutil.cの各関数は相互再帰に陥らないように注意すること。
- ログはデバッグ出力(DebugViewで見れる)とファイルの両方に出力する。
- Windows 10で起きたDirect3D 8の破壊的変更の緩和が主な目的なので、Direct3D 8のAPIとしてはできるだけ互換性を保つ。
- 設定ファイルでの設定は、原則としてデフォルト設定をautoにして、autoでうまくいかない場合はユーザーに手動で他の設定をしてもらう方針。

# thd3d8mitigationcustom

## 設計

thd3d8mitigationcfg.iniを編集する独立したGUIアプリケーション。thd3d8mitigationとはコードを一切共有しない。設定ファイルの仕様に基づいて別途作られている体だが、実際はthd3d8mitigationからのコードのコピペなどがままある。

## 文字コード

文字列はシステム部分がANSI文字列、UI部分がUnicode(UTF-16LE)文字列。

thd3d8mitigationのソースコードはBOM付きUTF-8で書かれているが、実行環境のコードページがUTF-8のものとは限らないので、ANSI文字列リテラルに非ASCII文字を含むべきではない。Unicode文字列リテラルは非ASCII文字を含んでもよい。

ビルドに使用するファイル(ソースファイルやプロジェクトファイルなど)の文字コードはASCIIもしくはBOM付きUTF-8とする。リソーススクリプトはUTF-16LEとする。

複数文字コードの混在は明らかに設計ミスな部分もあるが、使う場所が分かれているため不便はいまのところない。

改行コードはCRLFとする。

# TODO

- 回復不能なエラー時にutil.c:FatalなどはExitProcessではなくTerminateProcessなどを呼び出すべきかもしれない。
- フルスクリーンモード時に起動画面で端が欠ける問題を緩和したい。少なくとも紅魔郷と妖々夢で確認済み。
- 同様の仕組みで動作するツールと競合するため、競合を回避するような仕組みがあるといいかもしれない。例えば、thd3d8mitigationがd3d8.dllをロードするときに、システムディレクトリよりも指定の特定のディレクトリを優先するとか。
- もし実際のd3d8.dllにDirect3DCreate8以外のエクスポートされたシンボルがあったら、そのシンボルをハンドルしていないため、そのシンボルが参照されたときにthd3d8mitigationがクラッシュする潜在的な可能性がある。実際のd3d8.dllのエクスポートされたシンボルを調べて他のシンボルもハンドルすることは技術的には可能だと思うが、Windowsのリバースエンジニアリング禁止条項に抵触するのでしてはいけないと考えられる。どうするといいだろうか。
  - Direct3DCreate8は足りないのが自明なので、ドキュメントを参考にフックしている。
  - [Wineプロジェクト](https://www.winehq.org/)の取り組み方が参考になるかもしれない。Wineプロジェクトはこのような問題にどうやって取り組んでいるのだろうか。
    - ちなみに、WineはDirect3DCreate8以外に更にいくつかのシンボルをエクスポートしている。 https://github.com/wine-mirror/wine/blob/9d7a710fc0d1a0ecea17a68675d3899aff63ae0c/dlls/d3d8/d3d8.spec
    - また、Wineのクリーンルームガイドラインも参考になる。 https://wiki.winehq.org/Clean_Room_Guidelines
- 独立したCランタイムへの依存は無くした方がいいかもしれない。静的リンクするといいのだろうか？

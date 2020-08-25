# エラー処理

データファイルの破壊を防ぐため、大半のエラーは即座にクラッシュさせず、呼び出し元に報告するかログを出力したあと呼び出し元にNULLなどの無効な値を返す。ただし、ヒープメモリーの確保エラーとC++コードでの想定外の例外は(暗黙で引き起こされるスタックメモリーの確保エラーと同様に)回復不能と見做して即座にクラッシュさせる。

# 排他制御

me_exdataはmeに1対1で紐づく拡張プロパティと考えられ、meの排他制御の責任は呼び出し先ではなく呼び出し元にあるので、meのメソッド内ではme_exdataのデータ競合や競合状態について考えなくてよい。

# 時刻およびカウンター

timeGetTimeを使っている箇所はオーバーフローを考慮していないので、Windowsを起動してからだいたい49日を超えるとおかしくなる。

# 文字コード

thd3d8mitigationはBOM付きUTF-8で書かれているが、東方紅魔郷はShiftJISを前提としているので、文字列リテラルに非ASCII文字を含むべきではない。

ビルドに使用するファイル(ソースファイルやプロジェクトファイルなど)の文字コードはASCIIもしくはBOM付きUTF-8とする。.clang-formatや.gitignoreなどのその他の設定ファイルやドキュメントは(BOM無し)UTF-8とする。

改行コードはCRLFとする。

# 命名規則

- cs_\*: Critical section
- g_\*: Global variable
- \*_t: Type identifier
- Mod\*: Modified
- V\*: Function with va_list
- tm_\*: timeBeginPeriod and timeEndPeriod required

# TODO

- 回復不能なエラー時にutil.c::FatalなどはExitProcessではなくTerminateProcessなどを呼び出すべきかもしれない。
- 設定編集ソフト(custom.exeみたいなやつ)があった方がいいかもしれない。
- フルスクリーンモード時に起動画面で端が欠ける問題を緩和したい。少なくとも紅魔郷と妖々夢で確認済み。

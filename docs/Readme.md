<!-- Global site tag (gtag.js) - Google Analytics -->
<script async src="https://www.googletagmanager.com/gtag/js?id=UA-163314878-1"></script>
<script>
  window.dataLayer = window.dataLayer || [];
  function gtag(){dataLayer.push(arguments);}
  gtag('js', new Date());

  gtag('config', 'UA-163314878-1');
</script>

# OBS 同時配信プラグイン

本プラグインは複数のサイトで同時配信を行なうために作ってみた物です。

# スクリーンショット

![screenshot](./screenshot.jpg)


# ダウンロード

[リリースページ](https://github.com/sorayuki/obs-multi-rtmp/releases/)

現在、更新の自動チェック機能は搭載していません。


# Windows版インストール手順

## インストーラーを使用する場合

そのままインストールを実行してください。

インストール先のフォルダーは変更をしないでください。

## ポータブル版の場合

圧縮ファイルを展開後に「C:\Program Files\obs-studio」へファイルを配置してください。

## アンインストール

インストーラーを使用してアンインストールが行えます。

インストーラーでこのプラグインが正しく削除できなかった場合は、「C:\ProgramData\obs-studio\plugins\obs-multi-rtmp」のフォルダーを削除で消す事ができます。ファイル名を指定して実行で「%programdata%」と入力すると対象のフォルダーを開く事ができます。

「ProgramData」は通常だと非表示になっているのでご注意ください。


# インストール要件 (Windows版)

 OBS-Studioのバージョンに合わせてください。


# よくある質問 (FAQ)

**Q: 同時配信のドックウィンドウが表示されなくなりました。ドックをリセットしても変りません。**

A: たまに発生するバグのようです。原因は不明ですが、以下の手順でドックをリセットする事が可能です。

スタジオモードに変更をするとドックウィンドウが表示されるかもしれません。

もし、それでも表示がされない場合は

1. 「ヘルプ　→　ログファイル　→　ログファイルを表示」を行なう
2. OBSを終了する
3. 「AppData\Roaming\obs-studio」の格納先を開く(ファイル名を指定して実行で「%appdata%」と入力すると対象のフォルダーを開く事ができます)
4. 「global.ini」をテキストエディタで開く
5. DockState=XXXXXXXX(長い文字列になっています)を探す
6. その行を削除をし、保存をする

の手順で直ると思います。


# ビルド方法

断続的インテグレーションのスクリプトをご参照ください。


# 寄付 / Donate

如果你觉得这个工具很有用想要捐赠，这里是链接。注意：这不是提需求的渠道。

このツールの開発に支援もとい投げ銭をしたいと思った方は以下のリンクからお願いします。(機能のリクエストは受け付けていません)

If you regard this tool useful and want to doante for some, here is the link. (It's not for feature request.)

## PayPal / 贝宝
[PayPal / 贝宝](https://paypal.me/sorayuki0)

## AliPay または WeChat / 支付宝或微信

[AliPay](./zhi.png) 

[WeChat](./wechat.jpg)

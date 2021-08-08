<!-- Global site tag (gtag.js) - Google Analytics -->
<script async src="https://www.googletagmanager.com/gtag/js?id=UA-163314878-1"></script>
<script>
  window.dataLayer = window.dataLayer || [];
  function gtag(){dataLayer.push(arguments);}
  gtag('js', new Date());

  gtag('config', 'UA-163314878-1');
</script>

一生懸命に日本語で書いてみます＞＜

# OBS 同時配信プラグイン

本プラグインは、複数サイトに同時配信のため作ったものです。


# スクリーンショット

![screenshot](./screenshot.jpg)


# ダウンロード

[Windows版リリースページへ](https://github.com/sorayuki/obs-multi-rtmp/releases/)

[macOS版リリースページへ](https://github.com/kilinbox/obs-multi-rtmp/releases) (非公式)


# Windows版インストール手順

インストーラーを利用してください。目標フォルダを変更しないで。

## アンインストール

インストーラーを利用してアンインストールもできます。

インストーラーはこのプラグインを正しくリムーブしなかった場合、```C:\ProgramData\obs-studio\plugins\obs-multi-rtmp```フォルダを削除して済みます。

ProgramDataは普段に非表示ので注意してください。

# macOS版インストール手順

[kilinboxさんのページ](https://www.kilinbox.net/2021/01/obs-multi-rtmp.html)


# 要求環境(Windows版)

OBS-Studio バージョン 26.1.1 以降
また、OBS-Studio 本体は QT 5.15.2 と共にビルドしたバージョン

確認済：
obs-studio 26.1.1


# よくあるご質問

**Q: どうして OBS 25.0 以降じゃなくてダメなの？**

A: 古いバージョンの RTMP 配信モジュールはスレッドセーフじゃないため、複数配信の時クラッシュする可能性があります。

詳しいはこのコミットに参照してください: 

https://github.com/obsproject/obs-studio/commit/2b131d212fc0e5a6cd095b502072ddbedc54ab52 


**Q: どうして OBS 本体が配信したことない時このプラグインは使えないでしょうか？**

A: このプラグインは OBS 本体のエンコーダーと共有している。しかし、OBS は初めての配信を開始した前エンコーダーの作成しない。


# How to Build (Windows)

1. Prepare environment
   1. Put official release OBS 26.1.2 into obs-bin directory. 
   2. Extract OBS source code of same version as binary to obs-src
   3. Download Qt that obs-bin uses. Which can be found in CI\install-qt-win.cmd

2. Configure
   Use cmake to configure this project. must use VS2019 or higher. 
   cmake's QTDIR variable is set to the path of QT in the same version as obs uses. 
   
   Set CMAKE_BUILD_TYPE to Release. 

   > Notice: debug version of this plugin only works with debug version of OBS, which means you must build OBS from source.

3. Compile


# Donate

如果你觉得这个工具很有用想要捐赠，这里是链接。注意：这不是提需求的渠道。

このツールに投げ銭したいならここはリンクです。（機能要求ではありません）

If you regard this tool useful and want to doante for some, here is the link. (It's not for feature request.)

## paypal / 贝宝
[paypal / 贝宝](https://paypal.me/sorayuki0)

## alipay or wechat / 支付宝或微信

[alipay](./zhi.png) 

[wechat](./wechat.jpg)

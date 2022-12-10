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

## インストーラーを利用する場合

インストーラーを利用してください。目標フォルダを変更しないで。

## Portableの場合

アーカイブを　C:\Program Files\obs-studio　に解凍してください。

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

**Q: 同時配信のドックウインドウはなくなった。ドックをリセットしても効かない**

A: たまに発生するバッグかどうかわかりませんが、以下の手順でドックを一回完全にリセットする。

1. ヘルプメニュー　→　ログファイル　→　ログファイルを表示
2. OBSを終了する
3. 呼び出すウインドウで、AppData\Roaming\obs-studio\logsとかが表示している。obs-studio に移行
4. global.ini を開く
5. DockState=XXXXXXXX（←長い文字列） を探す
6. そのラインを削除して、セーブする

そうしたら治ると思います。



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

# How to Build (Ubuntu)

1. Install obs-studio via APT.  
Sorry but I didn't make it installed successfully via flatpak in my computer and network.
3. Install build tools.  
```apt install build-essential cmake mesa-common-dev qt6-base-dev```
1. Clone this repository or download the source archive and extract.
2. In the source Code folder, use cmake to do the stuff.  
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

# Donate

如果你觉得这个工具很有用想要捐赠，这里是链接。注意：这不是提需求的渠道。

このツールに投げ銭したいならここはリンクです。（機能要求ではありません）

If you regard this tool useful and want to doante for some, here is the link. (It's not for feature request.)

## paypal / 贝宝
[paypal / 贝宝](https://paypal.me/sorayuki0)

## alipay or wechat / 支付宝或微信

[alipay](./zhi.png) 

[wechat](./wechat.jpg)

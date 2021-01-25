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


# ダウンロード

[リリースページへ](https://github.com/sorayuki/obs-multi-rtmp/releases/)


# インストール手順

zipファイルを OBS のフォルダーに解凍してインストール完了。

![install](./install.jpg)


# スクリーンショット

![screenshot](./screenshot.jpg)


# 要求環境

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


# How to Build

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


# 捐赠 / 投げ銭 / donate

如果你觉得这个工具很有用想要捐赠，这里是链接。注意：这不是提需求的渠道。

このツールに投げ銭したいならここはリンクです。（機能要求ではありません）

If you regard this tool useful and want to doante for some, here is the link. (It's not for feature request.)

## paypal / 贝宝
<div id="smart-button-container">
    <div style="text-align: center"><label for="description">super chat </label><input type="text" name="descriptionInput" id="description" maxlength="127" value=""></div>
      <p id="descriptionError" style="visibility: hidden; color:red; text-align: center;">Please enter a description</p>
    <div style="text-align: center"><label for="amount">amount </label><input name="amountInput" type="number" id="amount" value="" ><span> USD</span></div>
      <p id="priceLabelError" style="visibility: hidden; color:red; text-align: center;">Please enter a price</p>
    <div id="invoiceidDiv" style="text-align: center; display: none;"><label for="invoiceid"> </label><input name="invoiceid" maxlength="127" type="text" id="invoiceid" value="" ></div>
      <p id="invoiceidError" style="visibility: hidden; color:red; text-align: center;">Please enter an Invoice ID</p>
    <div style="text-align: center; margin-top: 0.625rem;" id="paypal-button-container"></div>
  </div>
  <script src="https://www.paypal.com/sdk/js?client-id=sb&currency=USD" data-sdk-integration-source="button-factory"></script>
  <script>
  function initPayPalButton() {
    var description = document.querySelector('#smart-button-container #description');
    var amount = document.querySelector('#smart-button-container #amount');
    var descriptionError = document.querySelector('#smart-button-container #descriptionError');
    var priceError = document.querySelector('#smart-button-container #priceLabelError');
    var invoiceid = document.querySelector('#smart-button-container #invoiceid');
    var invoiceidError = document.querySelector('#smart-button-container #invoiceidError');
    var invoiceidDiv = document.querySelector('#smart-button-container #invoiceidDiv');

    var elArr = [description, amount];

    if (invoiceidDiv.firstChild.innerHTML.length > 1) {
      invoiceidDiv.style.display = "block";
    }

    var purchase_units = [];
    purchase_units[0] = {};
    purchase_units[0].amount = {};

    function validate(event) {
      return event.value.length > 0;
    }

    paypal.Buttons({
      style: {
        color: 'gold',
        shape: 'rect',
        label: 'pay',
        layout: 'horizontal',
        
      },

      onInit: function (data, actions) {
        actions.disable();

        if(invoiceidDiv.style.display === "block") {
          elArr.push(invoiceid);
        }

        elArr.forEach(function (item) {
          item.addEventListener('keyup', function (event) {
            var result = elArr.every(validate);
            if (result) {
              actions.enable();
            } else {
              actions.disable();
            }
          });
        });
      },

      onClick: function () {
        if (description.value.length < 1) {
          descriptionError.style.visibility = "visible";
        } else {
          descriptionError.style.visibility = "hidden";
        }

        if (amount.value.length < 1) {
          priceError.style.visibility = "visible";
        } else {
          priceError.style.visibility = "hidden";
        }

        if (invoiceid.value.length < 1 && invoiceidDiv.style.display === "block") {
          invoiceidError.style.visibility = "visible";
        } else {
          invoiceidError.style.visibility = "hidden";
        }

        purchase_units[0].description = description.value;
        purchase_units[0].amount.value = amount.value;

        if(invoiceid.value !== '') {
          purchase_units[0].invoice_id = invoiceid.value;
        }
      },

      createOrder: function (data, actions) {
        return actions.order.create({
          purchase_units: purchase_units,
        });
      },

      onApprove: function (data, actions) {
        return actions.order.capture().then(function (details) {
          alert('Transaction completed by ' + details.payer.name.given_name + '!');
        });
      },

      onError: function (err) {
        console.log(err);
      }
    }).render('#paypal-button-container');
  }
  initPayPalButton();
  </script>


## alipay or wechat / 支付宝或微信

![alipay](./zhi.png) 

![wechat](./wechat.jpg)

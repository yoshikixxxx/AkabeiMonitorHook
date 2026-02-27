# AkabeiMonitorHookDLL
Unofficial modifications of AkabeiMonitor<br>
[AkabeiMonitor(x64)]を機能改善する非公式DLLです。

## 改善点
- 以下のネットワークデバイスが検出されます
  - Microsoft Network Adapter Multiplexor Driver（Windowsチーミングドライバー）
  - VPN
  - TAP
  - WWLAN（SIMネットワークデバイス）
- 右クリックメニューのインターフェース項目を昇順ソートします

## 動作原理
AkabeiMonitorが使用しているIPHLPAPI.DLLの代わりにIPHLPMOD.DLLを呼び出すようにパッチを当て、DLLをEXEと同一フォルダに配置することで関数がフックされます。

## 使用方法
バイナリをダウンロードするか以下の手順でビルドしてください。
1. akamoni.exeにパッチを適用します。適用前と適用後のSHA256ハッシュ値が下表のハッシュ値と一致していることを確認してください。
```sh
patch.bat akamoni.exe IPHLPAPI.DLL IPHLPMOD.DLL
```
2. DLLをビルドするためにLLVMをインストールします。
3. build.batを実行してDLLをビルドし、生成されたIPHLPMOD.DLLをakamoni.exeと同じフォルダにコピーします。
```sh
build.bat
```

## SHA256
| EXE | Version | SHA256 |
| ------ | ------ | ------ |
| akamoni.exe (original) | 2.01b | 352D41E2C6E53D07D6EBA01CFA298303852CBC0AE6E54477FCFBED2607217165 |
| akamoni_201b_h1.exe | 2.01b hook 1 | 3CB35A87E21258B3D1CB39E4BEE751E39F41731AE83C7A06E2F7120B52F3E44F |

## サンプル
![sample1](https://raw.githubusercontent.com/yoshikixxxx/AkabeiMonitorHook/refs/heads/main/img/sample1.png)
![sample2](https://raw.githubusercontent.com/yoshikixxxx/AkabeiMonitorHook/refs/heads/main/img/sample2.png)
![sample3](https://raw.githubusercontent.com/yoshikixxxx/AkabeiMonitorHook/refs/heads/main/img/sample3.png)

## ライセンス
AkabeiMonitorおよびAkabeiMonitorHookは無料ソフトウェアです。<br>
AkabeiMonitorの著作権は作成者であるAkabei様が所有します。<br>
AkabeiMonitorおよびAkabeiMonitorHookの使用によって生じたいかなる損害についても作成者は一切責任を負いません。<br>
作成者はAkabeiMonitorおよびAkabeiMonitorHookの欠陥に対してサポートを提供したり修正したりする義務を負いません。


[AkabeiMonitor(x64)]: <http://park8.wakwak.com/~akabei/>

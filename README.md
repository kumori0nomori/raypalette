# raypalette

raypalette は、制御した照明下で球の色が、どのように変化するかをGUIで対話的に
調整し確認するための軽量な CUDA レンダラーです。
イラスト作成時の影色選択に役立てることを目標としています。

- <img src="./assets/screenshots/20260813_gui.png" alt="実行画面画像" title="実行画面" width="70%">

## 1. 使い方

### 1.1 動作環境

配布版はWindows 11を対象とし、NVIDIA GPUがない環境でもCPUだけで動作します。

- 通常はCPUを使用して動作します。
- RTX 50シリーズに対応したNVIDIA GPUとNVIDIAドライバがある場合は、自動的にGPUを使用して高速に動作します。

### 1.2 ダウンロード

GitHubの[Releases](../../releases)から`raypalette.exe`をダウンロードしてください。

ライセンス確認用に、`THIRD_PARTY_NOTICES.txt`も同じRelease Assetとして公開しています。

### 1.3 起動

ダウンロードしたexeをダブルクリックして起動します。
コマンドプロンプトから起動する場合は、次のように実行します。

```powershell
.\raypalette.exe
```

### 1.4 SHA256の確認

Release Assetの`SHA256SUMS.txt`に記載された値と、ダウンロードしたexeのSHA256値を
比較してください。PowerShellでは次のコマンドで確認できます。

```powershell
Get-FileHash .\raypalette.exe -Algorithm SHA256
```

表示された`Hash`の値が`SHA256SUMS.txt`の値と一致すれば、同じファイルであることを確認できます。

## 2. 基本操作
### 2.1 Controls

`Raypalette Controls`では、シーンとレンダリングの設定を変更できます。
設定を変更するとPreviewが再描画されます。

- `Sphere & Materials`: 球の材質を以下のモデルから選択できます。

    | 材質 | 概要 | 例 | 設定項目 |
    |---|---|---|---|
    | `Diffuse` | 光を拡散反射するマットな材質 | チョークや粘土、石膏、コピー用紙、布製品 | `Color` |
    | `Metal` | 金属のように光を反射する材質 | 鏡、金属、車のボディ | `Color`, `Roughness` |
    | `Glass` | 光の反射・屈折と内部吸収を表現する透明な材質 | 窓ガラス、レンズ、すりガラス、氷 | `Glass tint`, `Glass absorption density`, `Glass IOR` |
    | `Emissive` | 自ら光を放つ発光材質 | 液晶画面、ネオン管、蛍光灯 | `Sphere emission`, `Emission` |

    各設定値(`Color`、`Roughness`など)の詳細を以下に記載します。

    <details>

    | 設定 | 説明 |
    |---|---|
    | `Color` | 材質の基本色です。Diffuseでは拡散反射色、Metalでは鏡面反射色として使用されます。 |
    | `Roughness` | 金属表面の粗さです。0に近いほど鏡面反射が鋭く、1に近いほどぼやけます。 |
    | `Glass tint` | ガラスを通過する光の色です。 |
    | `Glass absorption density` | ガラス内部で光が吸収される強さです。 |
    | `Glass IOR` | 屈折率です。入射角とともに反射・屈折の割合を決定します。 |
    | `Emission color` | 発光する光の色です。 |
    | `Emission strength` | 発光の強さです。値が大きいほど明るくなり、周囲への照明も強くなります。 |

    </details>

- `Floor`: 床の設定です。

    | 設定 | 説明 |
    |---|---|
    | `Floor color` | 床の基本色です。Diffuseモデルとして使用されます。 |

- `Light`: 光源は以下から選択できます。その位置は、球の中心を原点とする「極座標系([wiki](https://ja.wikipedia.org/wiki/%E6%A5%B5%E5%BA%A7%E6%A8%99%E7%B3%BB))」で指定できます。(radius、theta、phi)

    | 設定 | 説明 | 例 |
    |---|---|---|
    | `Point` | 小さな1点から周囲360°の全方向に向かって放射状に光を放つ光源。 | 電球、ろうそくの光、松明 |
    | `Rectangular area` | 面から一方向に向かって光を放つ光源。 | 蛍光灯、窓から差し込む部屋の明かり |
    | `Directional (sun)` | 平行光源。空間全体に対して完全に平行な光を均一に降らせる。 | 太陽光 |

    `Rectangular area`を選択した場合は、面光源のサンプリング回数も設定できます。

    | 設定 | 説明 |
    |---|---|
    | `Area light samples` | 面光源をサンプリングする回数です。`4 (2x2)`、`9 (3x3)`、`16 (4x4)`から選択できます。 |

- `Environment color`と`Environment intensity`: シーン全体を包む環境光の設定です。これは光源が直接届かない面への間接的な明るさを示します。

    | 設定 | 説明 |
    |---|---|
    | `Environment color` | 環境光の色です。 |
    | `Environment intensity` | 環境光の強さです。 |

- `Rendering`: レンダリングの設定です。

    | 設定 | 説明 |
    |---|---|
    | `CPU/GPU` | NVIDIA GPUが利用可能ならGPUを、それ以外ではCPUを使用します。 |
    | `samples per pixel` | 1フレームで各ピクセルをサンプリングする回数です。 |
    | `target samples` | 累積レンダリングで目標とする総サンプル数です。 |
    | `max bounces` | 光線が反射・屈折できる最大回数です。 |

- `Camera / Display`: カメラ側の設定です。

    | 設定 | 説明 |
    |---|---|
    | `CPU/GPU` | 表示画面の明るさを調整します、値を大きくすると明るく、小さくすると暗くなります。 |
    | `Reinhard tone mapping` | 明るすぎる部分を圧縮して、ハイライトの白とびを抑えます。|
    | `Camera distance` | 球を撮影するカメラと球の中心との距離を設定します。 |


`Reset scene`を押すと、シーン・カメラ・光源が初期状態に戻り、Paletteも消去されます。
露出またはトーンマッピングを変更した場合も、表示結果とPaletteが更新されます。

### 2.2 Preview

`Preview`には、現在の設定でレンダリングした球と床の画像が表示されます。画像上の色を
調べるには、対象の位置を左クリックしてください。クリックした画素の色がPaletteに追加
され、画像上に番号付きのマーカーが表示されます。
また、右側の`HSV Space`にもクリックした画素の色が描画されます。

### 2.3 Palette

Preview下部の`Palette`タブには、Previewから抽出した色が一覧表示されます。各項目には
色見本とHEX表記が表示されます。

- 色の項目をクリックすると選択できます。
- 選択した色のHEX表記を`Ctrl+C`でクリップボードへコピーできます。
- `Clear palette`を押すと、登録した色をすべて削除できます。

シーン設定の変更や表示設定の変更によってPaletteが消去されます。
必要な色は設定を変更する前にコピーしてください。

### 2.4 HSV Space

`HSV Space`には、レンダリング結果の色の分布をHSV空間([wiki](https://ja.wikipedia.org/wiki/HSV%E8%89%B2%E7%A9%BA%E9%96%93))で表示します。
HSV空間は、色相を円周方向、彩度を半径方向、明度を高さ方向として表します。イラスト作成時によく使われる表現です。
球の色、光源の色、Paletteの色はマーカーで表示され、レンダリングで得られた色の分布は点群で表示されます。

| マウス操作 | 説明 |
|---|---|
| 左ドラッグ | HSV空間円柱を回転します。 |
| マウスホイール | 表示を拡大・縮小します。 |
| 右クリック | HSV空間の色相断面を選択します。 |
| `Hue section`の色相バーを左ドラッグ | 表示する色相を選択します。 |
| `Reset HSV view` | HSV空間の視点を初期状態に戻します。 |


色相断面の下に表示される平面では、横方向が彩度、縦方向が明度です。選択した色相の
範囲に含まれるレンダリング結果と、球・光源・Paletteの色を確認できます。

## 3. 開発者向け

開発環境の構築、ビルド、テスト、コード整形、リリース作成については
[開発者向けガイド](docs/development.md)を参照してください。

## 4. ライセンス

raypalette本体のソースコードとドキュメントは、[MIT License](LICENSE)の下で
提供します。Windows配布exeに組み込まれるDear ImGuiとGLFWのライセンス通知は
[THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt)に収録しています。

GoogleTestはテスト専用の依存ライブラリです。配布用ビルドでは`BUILD_TESTING=OFF`
として取得・構成しません。開発用ビルドでは、CMakeの`FetchContent`を通じて取得します。

GPU版のビルドと実行にはNVIDIA CUDA Toolkitおよび対応するNVIDIAドライバが必要です。
CUDA ToolkitとNVIDIAドライバはraypaletteのMIT Licenseの対象外で、それぞれの利用条件が
適用されます。

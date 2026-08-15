# 材質モデルとプリセットの実装計画

## 1. 目的

現在の `MaterialType` は `Diffuse`、`Metal`、`Dielectric`、`Emissive` を直接の散乱処理分岐として使用している。
この構成を、イラスト制作で扱いやすい汎用Surfaceモデルへ移行する。

ユーザーは `Matte`、`Glossy`、`Metal`、`Cloth`、`Skin`、`Hair` からプリセットを選択し、backendはプリセットを共通パラメータへ解決する。
材質ごとの個別BSDFやGUIを増やし続けないことを主な設計目標とする。

## 2. 設計方針

### 2.1 MaterialTypeの責務

`MaterialType` は材質名ではなく、光輸送の大分類を表す。

```cpp
enum class MaterialType : std::uint32_t {
  Surface,
  Dielectric,
  Emissive,
};
```

- `Surface`: 不透明表面。非金属と金属を同じPrincipled系モデルで評価する
- `Dielectric`: 反射、屈折、内部吸収を扱う透過モデル
- `Emissive`: 発光して経路を終了するモデル

`Metal` は `MaterialType` から削除する。ただし金属性は削除せず、Surfaceの `metallic = 1.0` として表現する。

Hairは最初から独立した `MaterialType` にしない。異方性をSurfaceローブとして実装できる範囲では `Surface + Hair` として扱い、専用Hair BSDFが必要になった時点で独立モデル化を判断する。

### 2.2 MaterialPresetの責務

`MaterialPreset` はUI上の見た目の選択肢であり、backendの散乱分岐ではない。

```cpp
enum class MaterialPreset : std::uint32_t {
  Custom,
  Matte,
  Glossy,
  Metal,
  Cloth,
  Skin,
  Hair,
};
```

プリセット選択時に、Surfaceの共通パラメータへ決定的な初期値を設定する。`Glossy` はPlastic、Lacquer、光沢のあるCeramicをまとめた汎用的な非金属反射面である。
プリセットは色を必ず上書きせず、既存のユーザー色を維持する。roughnessなどの外観に直結する値はプリセットの標準値へ更新する。

## 3. データモデル

### 3.1 Material

`Material` はシーンに保存されるユーザー向けの値を持つ。既存のGlassとEmissionの値は移行期間中も保持する。

```cpp
struct Material {
  MaterialType type = MaterialType::Surface;
  MaterialPreset preset = MaterialPreset::Custom;

  Vec3 base_color{0.8f, 0.8f, 0.8f};
  float metallic = 0.0f;
  float roughness = 0.5f;
  float specular = 0.5f;

  float index_of_refraction = 1.5f;
  Vec3 transmission_color{1.0f, 1.0f, 1.0f};
  float absorption_density = 0.0f;

  float coat = 0.0f;
  float coat_roughness = 0.03f;
  float sheen = 0.0f;
  float subsurface = 0.0f;
  float anisotropy = 0.0f;

  Vec3 emission_color{};
  float emission_strength = 0.0f;
};
```

既存のaggregate initializationを壊す影響が大きい場合は、段階移行として新フィールド追加後にすべての初期化箇所を名前付きの補助関数へ置き換える。

### 3.2 PrincipledParameters

renderer内部では、`Material` を直接使わず、解決済みの内部パラメータを使う。

```cpp
struct PrincipledParameters {
  Vec3 base_color;
  float metallic;
  float roughness;
  float specular;
  float transmission;
  float index_of_refraction;
  Vec3 transmission_color;
  float absorption_density;
  float coat;
  float coat_roughness;
  float sheen;
  float subsurface;
  float anisotropy;
  Vec3 emission_color;
  float emission_strength;
};
```

```cpp
RAYPALETTE_HOST_DEVICE
PrincipledParameters resolve_principled_parameters(const Material& material);
```

resolverはプリセットの標準値と、`Material` に保存されたユーザー値を統合する。CPUとCUDAの両方から呼べるよう、既存のhost/device制約に合わせる。

## 4. プリセットの初期値

初期値は物理測定値ではなく、イラスト用プレビューで扱いやすい開始点とする。最終的な数値は標準シーンの比較で調整する。

| Preset | metallic | roughness | coat | sheen | subsurface | anisotropy |
|---|---:|---:|---:|---:|---:|---:|
| Custom | 維持 | 維持 | 維持 | 維持 | 維持 | 維持 |
| Matte | 0.0 | 0.85 | 0.0 | 0.0 | 0.0 | 0.0 |
| Metal | 1.0 | 0.25 | 0.0 | 0.0 | 0.0 | 0.0 |
| Glossy | 0.0 | 0.28 | 0.0 | 0.0 | 0.0 | 0.0 |
| Cloth | 0.0 | 0.80 | 0.0 | 0.6 | 0.0 | 0.0 |
| Skin | 0.0 | 0.45 | 0.0 | 0.0 | 0.35 | 0.0 |
| Hair | 0.0 | 0.35 | 0.0 | 0.0 | 0.0 | 0.8 |

`Cloth`、`Skin`、`Hair` のローブが未実装の段階では、対応するパラメータを保持してもレンダリングへ影響させない。未対応機能を通常のGGXで偽装する場合は、GUIとドキュメントで近似であることを明示する。

## 5. 散乱処理

### 5.1 Surface

Surfaceは、少なくとも次のローブを共通評価する。

```text
diffuse lobe       : metallicが小さい場合に有効
GGX specular lobe  : 非金属、金属ともに有効
coat lobe          : coatが有効な場合
sheen lobe         : Clothなどで有効
subsurface近似     : Skinなどで有効
```

金属では `metallic = 1.0` とし、diffuseを無効化する。`base_color` は色付き反射のF0として利用する。

非金属ではdiffuseを残し、specularのF0はIORまたはspecularパラメータから生成する。

### 5.2 Dielectric

GlassはSurfaceへ統合せず、既存の反射、屈折、Beer-Lambert吸収を維持する。`transmission_color`、`index_of_refraction`、`absorption_density` の回帰テストを優先する。

### 5.3 Emissive

Emissionの評価と経路終了、emissive sphereの直接サンプリングは既存挙動を維持する。共通の発光パラメータから取得できるよう整理する。

### 5.4 Hair / Anisotropic

異方性には接線方向が必要である。現在の `HitRecord` は法線しか持たないため、Hairを完全に実装する段階で次を追加する。

- `tangent`、必要に応じて `bitangent`
- SphereのUVまたは安定した接線生成
- 異方性GGXの評価、サンプリング、PDF
- CPU/CUDA共通の異方性テスト

接線情報なしで `anisotropy` だけを追加する実装は行わない。

## 6. GUI

`Sphere & Materials` に次の順序で表示する。

```text
Material type
  [ Surface | Dielectric | Emissive ]

Surface preset
  [ Custom | Matte | Glossy | Metal | Cloth | Skin | Hair ]

Color
Roughness

[ Material details ]
```

- `Surface` 選択時だけSurface presetを表示する
- `Metal` はSurface presetとして表示する
- `Glossy` はPlastic、Lacquer、光沢のあるCeramicに共通する非金属反射面として表示する
- GlassではGlass tint、IOR、吸収密度を表示する
- EmissiveではEmission color、Emission strengthを表示する
- 内部用のmetallic、specular、coat、sheenなどは初期GUIで直接公開しない
- プリセット選択時は色を保持し、材質特性の標準値を適用する
- プリセット適用後にroughnessを変更しても、選択中のプリセット名は維持する
- `Reset preset values` は必要になった時点で追加する

Hair、Skin、Clothをbackend実装前に表示する場合は、名前だけで本格的な効果を保証しない。初期リリースでは未実装プリセットを非表示にするか、Experimental表記を付ける。

## 7. 実装フェーズ

### Phase 1: 型とresolver

- `MaterialPreset` を追加する
- `MaterialType::Surface` を追加する
- `PrincipledParameters` と `resolve_principled_parameters()` を追加する
- 既存の `Diffuse` と `Metal` をSurfaceへ変換する互換処理を追加する
- Materialの妥当性検証を新しい範囲へ対応させる
- resolverのunit testを追加する

### Phase 2: 共通Surface BSDF

- diffuseとspecularの評価を共通化する
- metallicによりdiffuseを制御する
- GGXの評価、PDF、throughputを共通化する
- direct lightingとpath samplingで同じローブ選択/PDFを使う
- `trace_color()` のDiffuse/Metal個別分岐を削除する

### Phase 3: 基本プリセットとGUI

- Matte、Glossy、Metalを追加する
- Surface typeとSurface presetの2段Comboへ変更する
- 共通Color/Roughness UIへ整理する
- 既存Glass/Emissionの操作を維持する

### Phase 4: Cloth、Skin

- sheenを追加する
- 簡易subsurface近似を追加する
- Cloth、Skinのプリセットを有効化する
- direct lighting、path sampling、有限値のテストを追加する

### Phase 5: Hair / Anisotropic

- ジオメトリに接線情報を追加する
- 異方性GGXまたはHair専用BSDFの方針を決定する
- 異方性用のsampling/PDF/MISを追加する
- Hairプリセットを有効化する

## 8. 互換性と移行

既存シーンや既存テストを壊さないため、移行途中では旧 `Diffuse`、`Metal` の値を読み込める変換関数を用意する。

```text
旧 Diffuse -> Surface + metallic 0.0
旧 Metal   -> Surface + metallic 1.0
旧 Glass   -> Dielectric
旧 Emissive -> Emissive
```

旧enumを残す期間は、rendererの中心ロジックで直接分岐せず、入口の変換処理だけで扱う。移行完了後に旧enumと旧UI分岐を削除する。

## 9. テスト計画

### Unit test

- 各プリセットが期待したパラメータへ解決される
- Metal presetが `metallic = 1.0` になり、diffuseが無効になる
- 非金属presetが `metallic = 0.0` になる
- roughness、coat、sheen、subsurface、anisotropyの範囲を検証する
- 既存GlassのIOR、透過色、吸収を維持する
- すべてのBSDF評価値とPDFが有限かつ非負になる

### Renderer test

- 全プリセットで有限画像を生成できる
- 標準シーンのMetal、Glossy、Glass、Emissiveを回帰する
- area lightとemissive sphereのdirect lightingを回帰する
- CPUとCUDAの共有ヘッダーがコンパイルできる
- Hair有効化後は接線方向を変えた異方性結果を検証する

### GUI確認

- Surface、Dielectric、Emissiveの切り替え
- Surface presetの切り替え
- Metal presetのroughnessと色
- GlassとEmissionの既存操作
- プリセット適用後の色保持と再描画
- CPU/CUDA切り替え後の材質表示

## 10. 完了条件

- rendererの主要分岐が `MaterialType` の光輸送モデルだけに限定されている
- MetalがSurface presetとして機能する
- MatteとGlossyで、Plastic、Lacquer、Ceramicを共通パラメータだけで表現できる
- Cloth、Skin、Hairは対応するローブの実装状態と一致して表示される
- CPU/CUDAで同じresolverとBSDF定義を利用する
- 既存のGlass、Emission、標準シーンの回帰テストが通る
- Windows CPU Debug/Releaseのビルド、CTest、`format-check` が成功する
- CUDA有効環境ではcompile checkと材質テストが成功する
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <cstdint>

// 起動画面とロードSceneで共有し、ブランド別JSONから軽量な2D演出を構成する。
class LoadingPresentation
{
public:
    // ロゴと表示設定だけを主スレッドで準備し、プレイヤーモデルは既存Sceneに任せる。
    void Initialize(bool engine_);
    // 実進捗を超えない表示補間と、実時間によるヒント・装飾を更新する。
    void Update(float deltaTime_, float progress_, bool completed_);
    // 解像度に対する比率で配置し、既存のシルエットを覆わずUIを重ねる。
    void Draw(bool failed_, bool standalone_ = false);
    // Scene終了時に表示用Textureを解放する。
    void Finalize();
private:
    // 同じヒントの連続表示を避けて次の文章を選択する。
    void SelectHint();
    nlohmann::json settings = nlohmann::json::object();
    std::vector<std::string> hints;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> logo;
    float logoAspect = 1;
    float elapsedTime = 0;
    float displayProgress = 0;
    float actualProgress = 0;
    float hintTime = 0;
    float completeTime = -1;
    int hintIndex = -1;
    std::uint32_t randomState = 0x414e4154;
    bool engine = false;
};

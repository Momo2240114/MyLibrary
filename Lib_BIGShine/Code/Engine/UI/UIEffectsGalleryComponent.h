#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

// UI効果の作例をページ単位で表示し、切り替え時に演出を再生する。
class UIEffectsGalleryComponent : public Component
{
public:
    // JSON復元用のコンポーネント名を返す。
    const std::string& GetName() const override;
    // 保存された開始ページを表示する。
    void Start() override;
    // キーボードと画面内の操作ラベルからページを切り替える。
    void Update(float deltaTime_) override;
    // 編集中の試写でも、生時間で拡大と復帰の補間を進める。
    void UpdateUnscaled(float deltaTime_) override;
    // コンポーネントを外す際に、一時的な拡大と前面化を解除する。
    void OnDestroy() override;
    // 編集中にもページ選択と演出の試写を行う。
    void Gui() override;
    // ページ構成と開始ページを保存する。
    void Serialize(nlohmann::json& json_) const override;
    // ページ構成を復元し、表示の再適用を予約する。
    void Deserialize(const nlohmann::json& json_) override;

private:
    // 背景のクリック領域と、まとめて拡大する表示要素を保持する。
    struct GalleryWindow
    {
        std::string hitObject;
        std::vector<std::string> objects;
    };

    // 作例を構成する背景、文字、画像の名前を同じページにまとめる。
    struct GalleryPage
    {
        std::string title;
        std::vector<std::string> objects;
        std::vector<GalleryWindow> windows;
    };

    // ページ番号を循環させ、演出を先頭から再生する。
    void SelectPage(int index_);
    // Start順に依存せず、全描画先が準備できるまで表示設定を再適用する。
    void ApplyPage();
    // UIレイアウトと同じ画面座標で操作ラベルのクリックを判定する。
    bool IsControlClicked(const std::string& name_);
    // 選択したWindowを中央へ拡大する準備を行う。
    void OpenWindow(int index_);
    // 背景のフェードと、Window内の全要素の移動・拡縮を補間する。
    void UpdateFocus(float deltaTime_);
    // 保存したレイアウトを変更せず、一覧表示の補正値へ戻す。
    void ResetFocus();

    std::vector<GalleryPage> pages;
    std::string pageLabelObject;
    std::string previousObject;
    std::string nextObject;
    std::string replayObject;
    std::string focusBackdropObject;
    std::string focusHintObject;
    int initialPage = 0;
    int currentPage = 0;
    bool displayPending = true;
    bool replayPending = false;
    int focusedWindow = -1;
    float focusProgress = 0.0f;
    bool focusClosing = false;
};

#pragma once
#include "SceneBase.h"

// 表示用SceneのJSONを読み込み、動作はScene内の汎用Componentへ委ねる。
class SceneTitle : public SceneBase
{
public:
    // 戦闘更新を行わない表示用Sceneとして初期化する。
    SceneTitle();
    // ObjectとComponentを既存の非同期Sceneロードで復元する。
    void Initialize() override;
};

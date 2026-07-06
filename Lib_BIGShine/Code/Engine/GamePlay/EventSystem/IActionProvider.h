#pragma once

// アクションを提供するコンポーネント用の基底クラス
class IActionProvider {
public:
    virtual ~IActionProvider() = default;

    // アクションの登録を強制する関数（Startとは別に呼べるようにする）
    virtual void RegisterActions() = 0;
};
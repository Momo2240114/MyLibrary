#pragma once
#include "TransitionParameters.h"
#include <functional>
#include <memory>
#include <string>
// 再生状態から独立し、視覚表現と固有リソースだけを扱うStrategy。
class ITransitionEffect
{
public:
    virtual ~ITransitionEffect() = default;
    virtual bool Initialize() = 0;
    virtual void LoadResources() = 0;
    virtual void SetParameters(const TransitionParameters& parameters_) = 0;
    virtual void SetProgress(float progress_) = 0;
    virtual void Update(float elapsedTime_, bool revealing_) = 0;
    virtual bool Render() = 0;
    virtual void Gui(TransitionParameters& parameters_) = 0;
    virtual bool IsReady() const = 0;
};
// 種類名と生成関数を登録し、毎フレームの種類判定を不要にする。
class TransitionEffectFactory
{
public:
    using Creator = std::function<std::unique_ptr<ITransitionEffect>()>;
    static bool Register(const std::string& name_, Creator creator_);
    static std::unique_ptr<ITransitionEffect> Create(const std::string& name_);
};

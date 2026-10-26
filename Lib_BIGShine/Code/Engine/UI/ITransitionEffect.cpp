#include "PrecompiledHeader.h"
#include "ITransitionEffect.h"
#include <map>
namespace {
// 関数内staticにより、Effectの登録順と静的初期化順に依存しない。
std::map<std::string,TransitionEffectFactory::Creator>& GetCreators()
{
    static std::map<std::string,TransitionEffectFactory::Creator> creators_;
    return creators_;
}
}
bool TransitionEffectFactory::Register(const std::string& name_,Creator creator_)
{
    return GetCreators().emplace(name_,std::move(creator_)).second;
}
std::unique_ptr<ITransitionEffect> TransitionEffectFactory::Create(const std::string& name_)
{
    const auto found_ = GetCreators().find(name_);
    return found_ != GetCreators().end() ? found_->second() : nullptr;
}

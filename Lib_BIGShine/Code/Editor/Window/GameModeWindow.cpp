#include "PrecompiledHeader.h"

#include "GameModeWindow.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Game/GameMode/GameModeManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"

#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>

namespace
{
    std::string ToDisplayString(const BlackBoardValue& value_)
    {
        // variantで保持されるBlackboard値を、表へそのまま出せる文字列に変換する。
        return std::visit([](auto&& arg_) -> std::string
            {
                using T = std::decay_t<decltype(arg_)>;
                std::ostringstream stream_;

                if constexpr (std::is_same_v<T, float>)
                {
                    stream_ << std::fixed << std::setprecision(3) << arg_;
                }
                else if constexpr (std::is_same_v<T, int>)
                {
                    stream_ << arg_;
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    stream_ << (arg_ ? "true" : "false");
                }
                else if constexpr (std::is_same_v<T, DirectX::SimpleMath::Vector2>)
                {
                    stream_ << std::fixed << std::setprecision(3)
                        << arg_.x << ", " << arg_.y;
                }
                else if constexpr (std::is_same_v<T, DirectX::SimpleMath::Vector3>)
                {
                    stream_ << std::fixed << std::setprecision(3)
                        << arg_.x << ", " << arg_.y << ", " << arg_.z;
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    stream_ << arg_;
                }

                return stream_.str();
            },
            value_);
    }

    const char* ToTypeName(const BlackBoardValue& value_)
    {
        // Blackboard値の型名を表示し、同じキー名でもデータ種別を判別できるようにする。
        return std::visit([](auto&& arg_) -> const char*
            {
                using T = std::decay_t<decltype(arg_)>;

                if constexpr (std::is_same_v<T, float>) return "Float";
                if constexpr (std::is_same_v<T, int>) return "Int";
                if constexpr (std::is_same_v<T, bool>) return "Bool";
                if constexpr (std::is_same_v<T, DirectX::SimpleMath::Vector2>) return "Vector2";
                if constexpr (std::is_same_v<T, DirectX::SimpleMath::Vector3>) return "Vector3";
                if constexpr (std::is_same_v<T, std::string>) return "String";
                return "Unknown";
            },
            value_);
    }
}

void GameModeWindow::Draw()
{
    if (!isVisible) return;

    // GameModeManagerの現在状態とGlobalBlackboardをまとめて確認するデバッグWindow。
    if (BeginEditorWindow())
    {
        DrawStylePopup();

        GameModeManager* gameModeManager_ =
            gameObjectManager ? gameObjectManager->GetGameModeManager() : nullptr;
        if (!gameModeManager_)
        {
            ImGui::TextDisabled("GameModeManager is not available.");
            EndEditorWindow();
            return;
        }

        DrawState(*gameModeManager_);
        // 単体Sequenceを直接再生せず、GameModeのロックと復元を伴う入口Actionを使う。
        ImGui::SeparatorText("Phase Transition Preview");
        const bool canPreview_ = gameModeManager_->GetCurrentGameState() == GameProgressState::PHASE_1_BATTLE ||
            gameModeManager_->IsPhaseTransitionPreview();
        const char* PREVIEW_PHASES[] = { "DragonDeath", "DragonDissolve", "StageCorruption", "MudAttack",
            "PlayerCounter", "HumanFormation", "SwordClash", "PlayerKnockdown" };
        for (const char* phase_ : PREVIEW_PHASES)
        {
            const std::string sequence_ = std::string("Preview") + phase_;
            ImGui::BeginDisabled(!canPreview_ || !ActionRegistry::Instance().HasAction(sequence_));
            if (ImGui::Button(phase_)) ActionRegistry::Instance().Call(std::string("GameMode.") + sequence_);
            ImGui::EndDisabled();
        }
        ImGui::BeginDisabled(!gameModeManager_->IsPhaseTransitionPreview());
        if (ImGui::Button("Restore First Form")) ActionRegistry::Instance().Call("GameMode.CancelPhaseTransitionPreview");
        ImGui::EndDisabled();
        // 中断時も生成物と予約を復旧Sequenceで整理してから戦闘へ戻す。
        const bool canSkip_ = gameModeManager_->IsPhaseTransitionPreview() ||
            gameModeManager_->GetCurrentGameState() == GameProgressState::PHASE_1_DEFEAT ||
            gameModeManager_->GetCurrentGameState() == GameProgressState::PHASE_2_INTRO;
        ImGui::BeginDisabled(!canSkip_);
        if (ImGui::Button("Skip Phase Transition")) ActionRegistry::Instance().Call("GameMode.SkipPhaseTransition");
        ImGui::EndDisabled();
        ImGui::SeparatorText("Global Blackboard");
        DrawGlobalBlackboard(gameModeManager_->GetGlobalBlackboard());
    }
    EndEditorWindow();
}

void GameModeWindow::DrawState(GameModeManager& gameModeManager_)
{
    // 戦闘ロック、AIディレクター傾向、HP率など、GameModeが持つ主要状態を表で表示する。
    if (ImGui::Button("Reset Battle State"))
    {
        gameModeManager_.ResetBattleState();
    }

    if (ImGui::BeginTable("GameModeStateTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("State");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        auto drawBool_ = [](const char* name_, bool value_)
            {
                // bool/float/intの行追加処理を分け、GameMode状態表の記述を短く保つ。
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(name_);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(value_ ? "true" : "false");
            };

        auto drawFloat_ = [](const char* name_, float value_)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(name_);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.3f", value_);
            };

        auto drawInt_ = [](const char* name_, int value_)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(name_);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", value_);
            };

        drawBool_("Gameplay Locked", gameModeManager_.IsGameplayLocked());
        drawBool_("Player Control Locked", gameModeManager_.IsPlayerControlLocked());
        drawBool_("Enemy AI Locked", gameModeManager_.IsEnemyAILocked());
        drawBool_("Presentation Playing", gameModeManager_.IsPresentationPlaying());
        drawBool_("Battle Paused", gameModeManager_.IsBattlePaused());
        drawInt_("Battle Phase", gameModeManager_.GetCurrentBattlePhase());
        drawFloat_("Player HP Rate", gameModeManager_.GetPlayerHpRate());
        drawFloat_("Boss HP Rate", gameModeManager_.GetBossHpRate());
        drawBool_("Prefer Delayed Attack", gameModeManager_.GetAIDirectorPreferDelayedAttack());
        drawBool_("Prefer Feint Attack", gameModeManager_.GetAIDirectorPreferFeintAttack());
        drawBool_("Prefer Guard Break", gameModeManager_.GetAIDirectorPreferGuardBreak());
        drawBool_("Reduce Aggression", gameModeManager_.GetAIDirectorReduceAggression());
        drawFloat_("Attack Delay Rate", gameModeManager_.GetAIDirectorAttackDelayRate());
        drawFloat_("Feint Rate", gameModeManager_.GetAIDirectorFeintRate());
        drawFloat_("Guard Break Rate", gameModeManager_.GetAIDirectorGuardBreakRate());
        drawFloat_("Aggression Rate", gameModeManager_.GetAIDirectorAggressionRate());

        ImGui::EndTable();
    }
}

void GameModeWindow::DrawGlobalBlackboard(BlackboardComponent& blackboard_)
{
    // GlobalBlackboardをカテゴリごとにTree表示し、各キーの型と値を一覧で確認する。
    const auto& board_ = blackboard_.GetBoard();
    if (board_.empty())
    {
        ImGui::TextDisabled("GlobalBlackboard is empty.");
        return;
    }

    for (const auto& categoryPair_ : board_)
    {
        const std::string& categoryName_ = categoryPair_.first;
        const BlackboardCategory& category_ = categoryPair_.second;

        if (!ImGui::TreeNode(categoryName_.c_str()))
        {
            continue;
        }

        if (ImGui::BeginTable("GlobalBlackboardTable", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
        {
            // カテゴリ内の全キーをKey/Type/Valueで表示し、AIや演出の共有状態を追えるようにする。
            ImGui::TableSetupColumn("Key");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            for (const auto& valuePair_ : category_)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(valuePair_.first.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(ToTypeName(valuePair_.second));
                ImGui::TableSetColumnIndex(2);
                const std::string valueText_ = ToDisplayString(valuePair_.second);
                ImGui::TextUnformatted(valueText_.c_str());
            }

            ImGui::EndTable();
        }

        ImGui::TreePop();
    }
}

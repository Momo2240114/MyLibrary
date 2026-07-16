#include "PrecompiledHeader.h"

#include "GameModeWindow.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Game/GameMode/GameModeManager.h"

#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>

namespace
{
    std::string ToDisplayString(const BlackBoardValue& value_)
    {
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
        ImGui::SeparatorText("Global Blackboard");
        DrawGlobalBlackboard(gameModeManager_->GetGlobalBlackboard());
    }
    EndEditorWindow();
}

void GameModeWindow::DrawState(GameModeManager& gameModeManager_)
{
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

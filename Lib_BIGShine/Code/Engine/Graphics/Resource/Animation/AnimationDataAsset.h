#pragma once

#pragma once
#include "AnimationState.h"
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

struct AnimEvent;
struct AnimState;
void from_json(const nlohmann::json& j, AnimEvent& e);
void to_json(nlohmann::json& j, const AnimEvent& e);
void from_json(const nlohmann::json& j, AnimState& s);
void to_json(nlohmann::json& j, const AnimState& s);

struct AnimationDataAsset 
{
    std::vector<AnimState> states;
    std::string defaultStateName;

    // JSON 変換用
    void Serialize(nlohmann::json& j) const {
        j["DefaultState"] = defaultStateName;
        nlohmann::json statesArray = nlohmann::json::array();
        for (const auto& s : states) {
            statesArray.push_back(s); // AnimState の to_json が呼ばれる
        }
        j["States"] = statesArray;
    }

    void Deserialize(const nlohmann::json& j) {
        defaultStateName = j.value("DefaultState", "");
        states.clear();
        if (j.contains("States") && j["States"].is_array()) {
            for (const auto& sJ : j["States"]) {
                AnimState s;
                from_json(sJ, s);
                states.push_back(s);
            }
        }
    }
};
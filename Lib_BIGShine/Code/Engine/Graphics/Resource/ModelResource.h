#pragma once
#include <string>
#include <vector>
#include <memory>
#include "GlTFModel.h" // 既存のNodeやMeshの定義を参照するため

// モデルの「静的な情報（骨組みや肉体）」のみを記憶する箱
class ModelResource
{
public:
    std::string modelName;

    // GltfModelCommponennt 内で定義されている構造体のリストをここに保持する
    std::vector<GltfModelCommponennt::Node> nodes;
    std::vector<GltfModelCommponennt::Mesh> meshes;
    std::vector<GltfModelCommponennt::Material> materials;
    std::vector<GltfModelCommponennt::Animation> animations;

    // アタッチメントなどが「特定の部位（ソケット）」の位置を探るための、便利な探索
    int FindNodeIndex(const std::string& name) const {
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (nodes[i].name == name) return static_cast<int>(i);
        }
        return -1;
    }

    // 既存のデータをすべて解放する
    void Clear() {
        materials.clear();
        meshes.clear();
        nodes.clear();
        animations.clear();
        modelName = "";
    }
};
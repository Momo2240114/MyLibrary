#pragma once
#include <DirectXMath.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <d3d11.h>
#include <wrl/client.h>
// UI演出のJSONとプリセット選択を共有し、欠損値や不正値を既定値へ戻す。
class UIPresentationPresets
{
public:
    // MeshVFXと同じAssetSlotを使い、画像プレビューとBrowse/Clear/DragDropを提供する。
    static bool DrawTexture(nlohmann::json& settings_,const char* key_,const char* label_);
    static bool LoadTexture(const std::string& path_,Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& output_);
    static void ClearTexturePreviews();
    static nlohmann::json Load(const std::string& path_);
    static bool Save(const std::string& path_,const nlohmann::json& catalog_);
    static float Number(const nlohmann::json& json_,const char* key_,float fallback_,float minimum_,float maximum_);
    static bool Boolean(const nlohmann::json& json_,const char* key_,bool fallback_);
    static std::string String(const nlohmann::json& json_,const char* key_,const std::string& fallback_);
    static DirectX::XMFLOAT2 Vector2(const nlohmann::json& json_,const char* key_,DirectX::XMFLOAT2 fallback_,float minimum_,float maximum_);
    static DirectX::XMFLOAT4 Color(const nlohmann::json& json_,const char* key_,DirectX::XMFLOAT4 fallback_);
    // 前後の切替とComboを同じカタログ順で扱う。保存済みの設定は選択時だけ読み込む。
    static bool DrawPicker(const nlohmann::json& catalog_,std::string& name_);
};

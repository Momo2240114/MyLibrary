#pragma once

//#include "ModelResource.h"
//#include <memory>
//#include <unordered_map>

//class ModelResourceManager
//{
//public:
//    static ModelResourceManager& Instance()
//    {
//        static ModelResourceManager inst;
//        return inst;
//    }
//
//    std::shared_ptr<ModelResource> Load(ID3D11Device* dvice_,const std::string& path_)
//    {
//        // キャッシュにある？
//        auto it = cache.find(path_);
//        if (it != cache.end())
//        {
//            return it->second;              // ← これ
//        }
//
//        // 無い → 新規ロード
//        auto res = std::make_shared<ModelResource>();
//        res->Load(dvice_, path_.c_str());
//        cache[path_] = res;
//        return res;
//    }
//
//private:
//    std::unordered_map<std::string, std::shared_ptr<ModelResource>> cache;
//};

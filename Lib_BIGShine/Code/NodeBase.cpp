#include "NodeBase.h"

#include<vector>
#include"JudgmentBase.h"
#include"BehaviorData.h"

NodeBase::~NodeBase()
{
    delete judgment;
    delete action;
}

bool NodeBase::Judgment()
{
    if (judgment != nullptr)
    {
        return judgment->Judgment();
    }
    return true;
}

NodeBase* NodeBase::SelectPriority(std::vector<NodeBase*>* list)
{
    NodeBase* selectNode = nullptr;
    int priority = INT_MAX;

    //一番優先順位が高いノードを探す
    for (NodeBase*List:*list)
    {
        if (priority >= List->GetPriority())
        {
            priority = List->GetPriority();
        }
    }

    //この優先順位が高いノードを保存する
    for (NodeBase* List :*list)
    {
        if (priority == List->GetPriority())
        {
            selectNode = List;
            break;
        }
    }

    return selectNode;
}

NodeBase* NodeBase::SelectRandom(std::vector<NodeBase*>* list)
{
    int selectNo = 0;

    selectNo = rand() & list->size();
    return (*list).at(selectNo);
}

NodeBase* NodeBase::SelectSequence(std::vector<NodeBase*>* list, BehaviorData* data)
{
    int step = 0;

    //指定されている中間ノードのシーケンスがどこまで実行されたか取得する
    step = data->GetSequenceStep(name);

    //中間ノードに登録されているノード数以上の場合
    if (step >= children.size())
    {
        //ルールによって処理を切り替える
        //ルールがBehaviorTree::SelectRule::SequentialLoopingの時は最初から実行するためstepに0を代入
        //ルールがBehaviorTree::SelectRule::Sequenceの時は次に実行できるノードがないため,nullptrをリターン

        if (selectRule == BehaviorTree::SelectRule::SequentialLooping)
        {
            step = 0;
        }
        else if (selectRule == BehaviorTree::SelectRule::Sequence)
        {
            return nullptr;
        }
    }

    //実行可能リストに登録されているデータの数だけループを行う
    for (auto it = list->begin(); it != list->end(); it++)
    {
        //子ノードが実行可能リストに含まれているか
        if (children.at(step)->GetName() == (*it)->GetName())
        {
            //現在の中間ノードを保温する
            data->PushSequenceNode(this);

            //中間ノードのステップを上げる
            data->SetSequenceStep(this->name, step + 1);
            return children.at(step);
        }
    }

    //指定された中間ノードに実行可能ノードがないのでnullptrを返す
    return nullptr;

}

NodeBase* NodeBase::SearchNode(std::string searchName)
{
    //名前が一致
    if (name == searchName)
    {
        return this;
    }
    else
    {
        //子ノードで検索
        for (auto it = children.begin(); it != children.end(); it++)
        {
            NodeBase* ret = (*it)->SearchNode(searchName);

            if (ret != nullptr)
            {
                return ret;
            }
        }
    }

    return nullptr;
}

NodeBase* NodeBase::Inference(BehaviorData* data)
{
    //listとは実行されるノード保存する
    std::vector<NodeBase*> list;
    NodeBase* result = nullptr;

    //childrenの数だけループを行う
    for (int i = 0; i < children.size(); i++)
    {
        //children.at(i)->judgmentがnullptrでなければ
        if (children.at(i)->judgment != nullptr)
        {
            if (children.at(i)->judgment->Judgment())
            {
                list.emplace_back(children.at(i));
            }
        }
        else
        {
            //判定クラスがなければ無条件に追加
            list.emplace_back(children.at(i));
        }
    }


    //選択ルールでノード決め
    switch (selectRule)
    {
        //優先順位
    case BehaviorTree::SelectRule::Priority:
        result = SelectPriority(&list);
        break;
        //ランダム
    case BehaviorTree::SelectRule::Random:
        result = SelectRandom(&list);
        break;
        //シーケンス
    case BehaviorTree::SelectRule::Sequence:
    case BehaviorTree::SelectRule::SequentialLooping:
        result = SelectSequence(&list, data);
        break;
    }

    if (result != nullptr)
    {
        //行動があれば終了
        if (result->HasAction() == true)
        {
            return result;
        }
        else
        {
            //決まったノードで推論開始
            result = result->Inference(data);
        }
    }
    return result;
}

ActionBase::State NodeBase::Run(float et)
{
    //actionがあるか判断あればメンバ関数Run()実行結果を返す
    if (action != nullptr)
    {
        return action->Run(et);
    }
    return ActionBase::State::Failed;
}

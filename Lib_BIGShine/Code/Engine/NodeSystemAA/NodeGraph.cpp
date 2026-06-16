#include "NodeGraph.h"

#include <algorithm>

namespace NodeSystem
{

    void NodeGraph::Update()
    {
        if (needsRebuild) Rebuild();

        // ƒm[ƒh•]‰¿‘O‚ÉƒŠƒ“ƒNæ‚Ö’l‚ð“]‘—‚µAŠeƒm[ƒh‚ªÅV‚Ì“ü—Í‚ðŽQÆ‚Å‚«‚é‚æ‚¤‚É‚·‚éB
        for (const auto& link_ : links) {
            Pin* start_ = FindPin(link_.startPinId);
            Pin* end_ = FindPin(link_.endPinId);
            if (start_ && end_) {
                end_->value = start_->value;
            }
        }

        // Ú‘±ŠÖŒW‚©‚ç\’z‚µ‚½‡˜‚É]‚Á‚Äƒm[ƒh‚ð•]‰¿‚·‚éB
        for (Node* node_ : evaluationOrder)
        {
            node_->Evaluate();
        }
    }

    void NodeGraph::AddLink(uintptr_t id_, uintptr_t startPinId_, uintptr_t endPinId_)
    {
        links.push_back({ id_, startPinId_, endPinId_ });
        needsRebuild = true;
    }

    void NodeGraph::RemoveLink(uintptr_t linkId_)
    {
        links.erase(
            std::remove_if(links.begin(), links.end(),
                [linkId_](const Link& link_) { return link_.id == linkId_; }),
            links.end()
        );

        // Ú‘±ŠÖŒW‚Ì•ÏX‚ðŽŸ‰ñUpdateŽž‚Ì•]‰¿‡˜‚Ö”½‰f‚·‚éB
        needsRebuild = true;
    }

    void NodeGraph::Rebuild()
    {
        // Œ»’iŠK‚Å‚Í“o˜^‡‚É•]‰¿‚·‚éBzŠÂŒŸo‚ðŠÜ‚Þƒgƒ|ƒƒWƒJƒ‹ƒ\[ƒg‚Í¡Œã‚ÌŠg’£“_B
        evaluationOrder.clear();
        for (auto& node_ : nodes) {
            evaluationOrder.push_back(node_.get());
        }
        needsRebuild = false;
    }

    Pin* NodeGraph::FindPin(uintptr_t pinId_)
    {
        for (auto& node_ : nodes) {
            for (auto& pin_ : node_->inputs) if (pin_.id == pinId_) return &pin_;
            for (auto& pin_ : node_->outputs) if (pin_.id == pinId_) return &pin_;
        }
        return nullptr;
    }

    Node* NodeGraph::FindNodeByPin(uintptr_t pinId_)
    {
        for (auto& node_ : nodes)
        {
            for (auto& pin_ : node_->inputs)
            {
                if (pin_.id == pinId_) return node_.get();
            }
            for (auto& pin_ : node_->outputs)
            {
                if (pin_.id == pinId_) return node_.get();
            }
        }
        return nullptr;
    }

} // namespace NodeSystem

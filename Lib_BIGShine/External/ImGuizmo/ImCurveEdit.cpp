#include "ImCurveEdit.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <stdint.h>
#include <algorithm>
#include <cstdio>
#include <cfloat>
#include <cmath>
#include <set>
#include <vector>

#if !defined(_MSC_VER)
#define _malloca(x) alloca(x)
#endif

namespace ImCurveEdit
{
   float Delegate::Interpolate(size_t curveIndex_, size_t pointIndex_, float rate_) const
   {
      if (GetCurveType(curveIndex_) != CurveSmooth) return rate_;
      return rate_ * rate_ * (3.0f - 2.0f * rate_);
   }

   float Delegate::EvaluateSegment(size_t curveIndex_, size_t pointIndex_, float rate_)
   {
      const ImVec2* points_ = GetPoints(curveIndex_);
      const float interpolationRate_ = Interpolate(curveIndex_, pointIndex_, rate_);
      return points_[pointIndex_].y +
         (points_[pointIndex_ + 1].y - points_[pointIndex_].y) * interpolationRate_;
   }


#ifndef IMGUI_DEFINE_MATH_OPERATORS
   static ImVec2 operator+(const ImVec2 &a, const ImVec2 &b) {
      return ImVec2(a.x + b.x, a.y + b.y);
   }

   static ImVec2 operator-(const ImVec2 &a, const ImVec2 &b) {
      return ImVec2(a.x - b.x, a.y - b.y);
   }

   static ImVec2 operator*(const ImVec2 &a, const ImVec2 &b) {
      return ImVec2(a.x * b.x, a.y * b.y);
   }

   static ImVec2 operator/(const ImVec2 &a, const ImVec2 &b) {
      return ImVec2(a.x / b.x, a.y / b.y);
   }

   static ImVec2 operator*(const ImVec2 &a, const float b) {
      return ImVec2(a.x * b, a.y * b);
   }
#endif

   static float smoothstep(float edge0, float edge1, float x)
   {
      x = ImClamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
      return x * x * (3 - 2 * x);
   }

   static float distance(float x, float y, float x1, float y1, float x2, float y2)
   {
      float A = x - x1;
      float B = y - y1;
      float C = x2 - x1;
      float D = y2 - y1;

      float dot = A * C + B * D;
      float len_sq = C * C + D * D;
      float param = -1.f;
      if (len_sq > FLT_EPSILON)
         param = dot / len_sq;

      float xx, yy;

      if (param < 0.f) {
         xx = x1;
         yy = y1;
      }
      else if (param > 1.f) {
         xx = x2;
         yy = y2;
      }
      else {
         xx = x1 + param * C;
         yy = y1 + param * D;
      }

      float dx = x - xx;
      float dy = y - yy;
      return sqrtf(dx * dx + dy * dy);
   }

   static int DrawPoint(ImDrawList* draw_list, ImVec2 pos, const ImVec2 size, const ImVec2 offset, bool edited)
   {
      int ret = 0;
      ImGuiIO& io = ImGui::GetIO();

      static const ImVec2 localOffsets[4] = { ImVec2(1,0), ImVec2(0,1), ImVec2(-1,0), ImVec2(0,-1) };
      ImVec2 offsets[4];
      for (int i = 0; i < 4; i++)
      {
         offsets[i] = pos * size + localOffsets[i]*4.5f + offset;
      }

      const ImVec2 center = pos * size + offset;
      const ImRect anchor(center - ImVec2(5, 5), center + ImVec2(5, 5));
      draw_list->AddConvexPolyFilled(offsets, 4, 0xFF000000);
      if (anchor.Contains(io.MousePos))
      {
         ret = 1;
         if (io.MouseDown[0])
            ret = 2;
      }
      if (edited)
         draw_list->AddPolyline(offsets, 4, 0xFFFFFFFF, true, 3.0f);
      else if (ret)
         draw_list->AddPolyline(offsets, 4, 0xFF80B0FF, true, 2.0f);
      else
         draw_list->AddPolyline(offsets, 4, 0xFF0080FF, true, 2.0f);

      return ret;
   }

   int Edit(Delegate& delegate, const ImVec2& size, unsigned int id, const ImRect* clippingRect, ImVector<EditPoint>* selectedPoints, const CurveEditOptions* options)
   {
      const CurveEditOptions defaultOptions;
      const CurveEditOptions& editOptions = options ? *options : defaultOptions;
      static bool selectingQuad = false;
      static ImVec2 quadSelection;
      static int overCurve = -1;
      static int movingCurve = -1;
      static bool scrollingV = false;
      static std::set<EditPoint> selection;
      if (selectedPoints)
      {
         selection.clear();
         for (const EditPoint& selectedPoint_ : *selectedPoints)
         {
            selection.insert(selectedPoint_);
         }
      }
      static bool overSelectedPoint = false;

      int ret = 0;

      ImGuiIO& io = ImGui::GetIO();
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
      ImGui::PushStyleColor(ImGuiCol_Border, 0);
      // Use the available region instead of a fixed editor size.
      ImGui::BeginChild(ImGui::GetID("my_frame"), ImVec2(0, 0), ImGuiChildFlags_FrameStyle);
      delegate.focused = ImGui::IsWindowFocused();
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      if (clippingRect)
         draw_list->PushClipRect(clippingRect->Min, clippingRect->Max, true);

      const ImVec2 offset = ImGui::GetCursorScreenPos() + ImVec2(0.f, size.y);
      const ImVec2 ssize(size.x, -size.y);
      const ImRect container(offset + ImVec2(0.f, ssize.y), offset + ImVec2(ssize.x, 0.f));
      ImVec2& min = delegate.GetMin();
      ImVec2& max = delegate.GetMax();

      // handle zoom and VScroll
      if (container.Contains(io.MousePos))
      {
          if (fabsf(io.MouseWheel) > FLT_EPSILON)
          {
              const float r = (io.MousePos.y - offset.y) / ssize.y;
              float ratioY = ImLerp(min.y, max.y, r);
              auto scaleValue = [&](float v) {
                  v -= ratioY;
                  v *= (1.f - io.MouseWheel * 0.05f);
                  v += ratioY;
                  return v;
              };
              min.y = scaleValue(min.y);
              max.y = scaleValue(max.y);
          }
          if (!scrollingV && ImGui::IsMouseDown(2))
          {
              scrollingV = true;
          }
      }
      ImVec2 range = max - min;  // +1 because of inclusive last frame

      const ImVec2 viewSize(size.x, -size.y);
      const ImVec2 sizeOfPixel = ImVec2(1.f, 1.f) / viewSize;
      const size_t curveCount = delegate.GetCurveCount();

      if (scrollingV)
      {
          float deltaH = io.MouseDelta.y * range.y * sizeOfPixel.y;
          min.y -= deltaH;
          max.y -= deltaH;
          if (!ImGui::IsMouseDown(2))
              scrollingV = false;
      }

      draw_list->AddRectFilled(offset, offset + ssize, delegate.GetBackgroundColor());

      auto pointToRange = [&](ImVec2 pt) { return (pt - min) / range; };
      auto rangeToPoint = [&](ImVec2 pt) { return (pt * range) + min; };

      const float top_ = offset.y - size.y;
      const float bottom_ = offset.y;
      const int xDivisionCount_ = (std::max)(2, (std::min)(10, static_cast<int>(size.x / 80.0f)));
      const int yDivisionCount_ = (std::max)(2, (std::min)(10, static_cast<int>(size.y / 44.0f)));
      const ImU32 gridColor_ = ImGui::GetColorU32(ImVec4(0.42f, 0.45f, 0.50f, 0.28f));
      const ImU32 labelColor_ = ImGui::GetColorU32(ImVec4(0.78f, 0.80f, 0.84f, 0.92f));
      const ImU32 zeroAxisColor_ = ImGui::GetColorU32(ImVec4(0.95f, 0.72f, 0.25f, 0.95f));

      for (int divisionIndex_ = 0; divisionIndex_ <= xDivisionCount_; ++divisionIndex_)
      {
         const float ratio_ = static_cast<float>(divisionIndex_) / static_cast<float>(xDivisionCount_);
         const float value_ = ImLerp(min.x, max.x, ratio_);
         const float x_ = offset.x + ratio_ * size.x;
         draw_list->AddLine(ImVec2(x_, top_), ImVec2(x_, bottom_), gridColor_, 1.0f);

         char label_[32]{};
         snprintf(label_, sizeof(label_), "%.2f", value_);
         const ImVec2 textSize_ = ImGui::CalcTextSize(label_);
         const float textX_ = (std::max)(offset.x + 2.0f, (std::min)(x_ - textSize_.x * 0.5f, offset.x + size.x - textSize_.x - 2.0f));
         draw_list->AddText(ImVec2(textX_, bottom_ - textSize_.y - 2.0f), labelColor_, label_);
      }

      for (int divisionIndex_ = 0; divisionIndex_ <= yDivisionCount_; ++divisionIndex_)
      {
         const float ratio_ = static_cast<float>(divisionIndex_) / static_cast<float>(yDivisionCount_);
         const float value_ = ImLerp(min.y, max.y, ratio_);
         const float y_ = bottom_ - ratio_ * size.y;
         draw_list->AddLine(ImVec2(offset.x, y_), ImVec2(offset.x + size.x, y_), gridColor_, 1.0f);

         char label_[32]{};
         snprintf(label_, sizeof(label_), "%.2f", value_);
         const ImVec2 textSize_ = ImGui::CalcTextSize(label_);
         const float textY_ = (std::max)(top_ + 2.0f, (std::min)(y_ - textSize_.y * 0.5f, bottom_ - textSize_.y - 2.0f));
         draw_list->AddText(ImVec2(offset.x + 4.0f, textY_), labelColor_, label_);
      }

      if (min.x <= 0.0f && max.x >= 0.0f && fabsf(range.x) > FLT_EPSILON)
      {
         const float zeroX_ = offset.x + ((0.0f - min.x) / range.x) * size.x;
         draw_list->AddLine(ImVec2(zeroX_, top_), ImVec2(zeroX_, bottom_), zeroAxisColor_, 2.0f);
         draw_list->AddText(ImVec2(zeroX_ + 4.0f, top_ + 3.0f), zeroAxisColor_, "X = 0");
      }
      if (min.y <= 0.0f && max.y >= 0.0f && fabsf(range.y) > FLT_EPSILON)
      {
         const float zeroY_ = bottom_ - ((0.0f - min.y) / range.y) * size.y;
         draw_list->AddLine(ImVec2(offset.x, zeroY_), ImVec2(offset.x + size.x, zeroY_), zeroAxisColor_, 2.0f);
         draw_list->AddText(ImVec2(offset.x + 4.0f, zeroY_ + 3.0f), zeroAxisColor_, "Y = 0");
      }
      bool overCurveOrPoint = false;

      int localOverCurve = -1;
      // make sure highlighted curve is rendered last
      int *curvesIndex = (int*)_malloca(sizeof(int) * curveCount);
      for (size_t c = 0; c < curveCount; c++)
          curvesIndex[c] = int(c);
      int highLightedCurveIndex = -1;
      if (overCurve != -1 && curveCount)
      {
          ImSwap(curvesIndex[overCurve], curvesIndex[curveCount - 1]);
          highLightedCurveIndex = overCurve;
      }

      for (size_t cur = 0; cur < curveCount; cur++)
      {
          int c = curvesIndex[cur];
         if (!delegate.IsVisible(c))
            continue;
         const size_t ptCount = delegate.GetPointCount(c);
         if (ptCount < 1)
            continue;
         CurveType curveType = delegate.GetCurveType(c);
         if (curveType == CurveNone)
             continue;
         const ImVec2* pts = delegate.GetPoints(c);
         uint32_t curveColor = delegate.GetCurveColor(c);
         if ((c == highLightedCurveIndex && selection.empty() && !selectingQuad) || movingCurve == c)
            curveColor = 0xFFFFFFFF;

         for (size_t p = 0; p < ptCount - 1; p++)
         {
            const ImVec2 p1 = pointToRange(pts[p]);
            const ImVec2 p2 = pointToRange(pts[p+1]);

            if (curveType == CurveSmooth || curveType == CurveLinear)
            {
                const size_t subStepCount = 20;
                float step = 1.f / float(subStepCount - 1);
                for (size_t substep = 0; substep < subStepCount - 1; substep++)
                {
                    float t = float(substep) * step;

                    const ImVec2 sp1 = ImLerp(p1, p2, t);
                    const ImVec2 sp2 = ImLerp(p1, p2, t + step);

                    const float value1 = delegate.EvaluateSegment(c, p, t);
                    const float value2 = delegate.EvaluateSegment(c, p, t + step);

                    const ImVec2 pos1 = ImVec2(sp1.x, pointToRange(ImVec2(0.0f, value1)).y) * viewSize + offset;
                    const ImVec2 pos2 = ImVec2(sp2.x, pointToRange(ImVec2(0.0f, value2)).y) * viewSize + offset;

                    if (distance(io.MousePos.x, io.MousePos.y, pos1.x, pos1.y, pos2.x, pos2.y) < 8.f && !scrollingV)
                    {
                        localOverCurve = int(c);
                        overCurve = int(c);
                        overCurveOrPoint = true;
                    }

                    draw_list->AddLine(pos1, pos2, curveColor, 1.3f);
                } // substep
            }
            else if (curveType == CurveDiscrete)
            {
                ImVec2 dp1 = p1 * viewSize + offset;
                ImVec2 dp2 = ImVec2(p2.x, p1.y) * viewSize + offset;
                ImVec2 dp3 = p2 * viewSize + offset;
                draw_list->AddLine(dp1, dp2, curveColor, 1.3f);
                draw_list->AddLine(dp2, dp3, curveColor, 1.3f);

                if ( (distance(io.MousePos.x, io.MousePos.y, dp1.x, dp1.y, dp3.x, dp1.y) < 8.f ||
                    distance(io.MousePos.x, io.MousePos.y, dp3.x, dp1.y, dp3.x, dp3.y) < 8.f )
                    /*&& localOverCurve == -1*/)
                {
                    localOverCurve = int(c);
                    overCurve = int(c);
                    overCurveOrPoint = true;
                }
            }
         } // point loop

         for (size_t p = 0; p < ptCount; p++)
         {
            const int drawState = DrawPoint(draw_list, pointToRange(pts[p]), viewSize, offset, (selection.find({int(c), int(p)}) != selection.end() && movingCurve == -1 && !scrollingV));
            if (drawState && delegate.IsEditable(c) && editOptions.allowPointMove && movingCurve == -1 && !selectingQuad)
            {
               overCurveOrPoint = true;
               overSelectedPoint = true;
               overCurve = -1;
               if (drawState == 2)
               {
                  if (!io.KeyShift && selection.find({ int(c), int(p) }) == selection.end())
                     selection.clear();
                  selection.insert({ int(c), int(p) });
               }
            }
         }
      } // curves loop

      if (localOverCurve == -1)
         overCurve = -1;

      // move selection
      static bool pointsMoved = false;
      static ImVec2 mousePosOrigin;
      static std::vector<ImVec2> originalPoints;
      if (overSelectedPoint && io.MouseDown[0])
      {
          if ((fabsf(io.MouseDelta.x) > 0.f || fabsf(io.MouseDelta.y) > 0.f) && !selection.empty())
          {
              if (!pointsMoved)
              {
                  delegate.BeginEdit(0);
                  mousePosOrigin = io.MousePos;
                  originalPoints.resize(selection.size());
                  int index = 0;
                  for (auto& sel : selection)
                  {
                      const ImVec2* pts = delegate.GetPoints(sel.curveIndex);
                      originalPoints[index++] = pts[sel.pointIndex];
                  }
              }
              pointsMoved = true;
              ret = 1;
              auto prevSelection = selection;
              int originalIndex = 0;
              for (auto& sel : prevSelection)
              {
                  const ImVec2* pts = delegate.GetPoints(sel.curveIndex);
                  const ImVec2 p = rangeToPoint(pointToRange(originalPoints[originalIndex]) + (io.MousePos - mousePosOrigin) * sizeOfPixel);
                  const int newIndex = delegate.EditPoint(sel.curveIndex, sel.pointIndex, p);
                  if (newIndex != sel.pointIndex)
                  {
                      selection.erase(sel);
                      selection.insert({ sel.curveIndex, newIndex });
                  }
                  originalIndex++;
              }
          }
      }

      if (overSelectedPoint && !io.MouseDown[0])
      {
          overSelectedPoint = false;
          if (pointsMoved)
          {
              pointsMoved = false;
              delegate.EndEdit();
          }
      }

      // add point
       if (editOptions.allowPointAdd && overCurve != -1 && io.MouseDoubleClicked[0])
      {
         const ImVec2 np = rangeToPoint((io.MousePos - offset) / viewSize);
         delegate.BeginEdit(overCurve);
         delegate.AddPoint(overCurve, np);
         delegate.EndEdit();
         ret = 1;
      }

      // move curve

       if (editOptions.allowCurveMove && movingCurve != -1)
      {
         const size_t ptCount = delegate.GetPointCount(movingCurve);
         const ImVec2* pts = delegate.GetPoints(movingCurve);
         if (!pointsMoved)
         {
             mousePosOrigin = io.MousePos;
             pointsMoved = true;
             originalPoints.resize(ptCount);
             for (size_t index = 0;index< ptCount;index++)
             {
                 originalPoints[index] = pts[index];
             }
         }
         if (ptCount >= 1)
         {
            for (size_t p = 0; p < ptCount; p++)
            {
               delegate.EditPoint(movingCurve, int(p), rangeToPoint(pointToRange(originalPoints[p]) + (io.MousePos - mousePosOrigin) * sizeOfPixel));
            }
            ret = 1;
         }
         if (!io.MouseDown[0])
         {
             movingCurve = -1;
             pointsMoved = false;
             delegate.EndEdit();
         }
      }
       if (editOptions.allowCurveMove && movingCurve == -1 && overCurve != -1 && ImGui::IsMouseClicked(0) && selection.empty() && !selectingQuad)
      {
         movingCurve = overCurve;
         delegate.BeginEdit(overCurve);
      }

      // quad selection
      if (selectingQuad)
      {
         const ImVec2 bmin = ImMin(quadSelection, io.MousePos);
         const ImVec2 bmax = ImMax(quadSelection, io.MousePos);
         draw_list->AddRectFilled(bmin, bmax, 0x40FF0000, 1.f);
         draw_list->AddRect(bmin, bmax, 0xFFFF0000, 1.f);
         const ImRect selectionQuad(bmin, bmax);
         if (!io.MouseDown[0])
         {
            if (!io.KeyShift)
               selection.clear();
            // select everythnig is quad
            for (size_t c = 0; c < curveCount; c++)
            {
               if (!delegate.IsVisible(c))
                  continue;

               const size_t ptCount = delegate.GetPointCount(c);
               if (ptCount < 1)
                  continue;

               const ImVec2* pts = delegate.GetPoints(c);
               for (size_t p = 0; p < ptCount; p++)
               {
                  const ImVec2 center = pointToRange(pts[p]) * viewSize + offset;
                  if (selectionQuad.Contains(center))
                     selection.insert({int(c), int(p)});
               }
            }
            // done
            selectingQuad = false;
         }
      }
      if (!overCurveOrPoint && ImGui::IsMouseClicked(0) && !selectingQuad && movingCurve == -1 && !overSelectedPoint && container.Contains(io.MousePos))
      {
         selectingQuad = true;
         quadSelection = io.MousePos;
      }
      if (clippingRect)
         draw_list->PopClipRect();

      ImGui::EndChild();
      ImGui::PopStyleVar();
      ImGui::PopStyleColor(1);

      if (selectedPoints)
      {
          selectedPoints->resize(int(selection.size()));
          int index = 0;
          for (auto& point : selection)
              (*selectedPoints)[index++] = point;
      }
      return ret;
   }
}

#include "PrecompiledHeader.h"
#include "InputIconAtlas.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/Sprite.h"
#include <algorithm>

namespace
{
    // セル内の透明な余白も保ち、全アイコンを正方形で描画する。
    struct IconCell { int code; int column; int row; };
    constexpr IconCell KEY_CELLS[] = {
        {'A',5,14},{'B',4,12},{'C',3,11},{'D',2,10},{'E',6,10},{'F',1,9},
        {'G',12,8},{'H',14,8},{'I',1,7},{'J',5,7},{'K',7,7},{'L',9,7},
        {'M',11,7},{'N',15,7},{'O',6,6},{'P',11,6},{'Q',10,5},{'R',16,5},
        {'S',3,4},{'T',4,3},{'U',13,3},{'V',1,2},{'W',3,2},{'X',7,2},{'Y',9,2},{'Z',11,2},
        {VK_SPACE,2,3},{VK_SHIFT,11,4},{VK_LSHIFT,11,4},{VK_RSHIFT,11,4},
        {VK_CONTROL,0,10},{VK_LCONTROL,0,10},{VK_RCONTROL,0,10},
        {VK_MENU,7,14},{VK_LMENU,7,14},{VK_RMENU,7,14},{VK_TAB,10,3},
        {VK_RETURN,10,10},{VK_ESCAPE,14,10},{VK_BACK,9,12},{VK_DELETE,4,10},
        {VK_INSERT,3,7},{VK_HOME,16,8},{VK_END,8,10},{VK_PRIOR,15,6},{VK_NEXT,13,6},
        {VK_UP,2,13},{VK_DOWN,13,14},{VK_LEFT,15,14},{VK_RIGHT,0,13},
        {VK_CAPITAL,7,11},{VK_NUMLOCK,0,6},{VK_SCROLL,5,4},{VK_SNAPSHOT,8,5},
        {VK_PAUSE,1,5},{VK_LWIN,5,2},{VK_RWIN,5,2},
        // 実シートの赤い押下部で識別する。左クリックは(14,2)、右クリックは(1,1)。
        {VK_LBUTTON,14,2},{VK_RBUTTON,1,1},{VK_MBUTTON,3,1},{VK_XBUTTON1,12,1},{VK_XBUTTON2,14,1},
        {VK_OEM_MINUS,13,7},{VK_OEM_PLUS,12,10},{VK_OEM_COMMA,13,11},{VK_OEM_PERIOD,4,5},
        {VK_OEM_1,7,4},{VK_OEM_2,15,4},{VK_OEM_3,11,14},
        {VK_OEM_5,13,4},{VK_OEM_7,14,5},
        {VK_ADD,4,6},{VK_SUBTRACT,13,7},{VK_MULTIPLY,2,12},{VK_DIVIDE,15,4}
    };
    struct PadCell { PadCode code; int xboxColumn; int xboxRow; int psColumn; int psRow; };
    constexpr PadCell PAD_CELLS[] = {
        {PadCode::A,3,8,0,10},{PadCode::B,5,8,10,11},{PadCode::X,7,8,2,10},{PadCode::Y,9,8,4,10},
        {PadCode::L1,8,3,4,6},{PadCode::R1,4,2,1,5},{PadCode::L2,2,2,8,6},{PadCode::R2,8,2,5,5},
        {PadCode::L3,0,2,8,10},{PadCode::R3,6,2,10,10},{PadCode::START,1,7,11,5},
        {PadCode::BACK,9,7,1,4},{PadCode::GUIDE,6,3,-1,-1},{PadCode::TOUCHPAD,-1,-1,5,4},
        {PadCode::UP,9,6,10,9},{PadCode::DOWN,2,4,5,9},{PadCode::LEFT,8,6,7,9},{PadCode::RIGHT,0,4,0,8},
        {PadCode::STICK_L_UP,4,1,10,8},{PadCode::STICK_L_DOWN,0,1,1,7},
        {PadCode::STICK_L_LEFT,1,1,8,8},{PadCode::STICK_L_RIGHT,2,1,11,8},
        {PadCode::STICK_R_UP,3,0,8,7},{PadCode::STICK_R_DOWN,4,0,3,7},
        {PadCode::STICK_R_LEFT,0,0,5,7},{PadCode::STICK_R_RIGHT,2,0,7,7}
    };
    constexpr const char* SPRITE_PATHS[] = {
        "Data/Sprite/Input/KeyMouse/KeyMouseInputSheet.png",
        "Data/Sprite/Input/GamePad/Xbox/XboxInputSheet.png",
        "Data/Sprite/Input/GamePad/PS/playstation-series_sheet_double.png"
    };
    constexpr const wchar_t* SHEET_PATHS[] = {
        L"Data/Sprite/Input/KeyMouse/KeyMouseInputSheet.png",
        L"Data/Sprite/Input/GamePad/Xbox/XboxInputSheet.png",
        L"Data/Sprite/Input/GamePad/PS/playstation-series_sheet_double.png"
    };
}

void InputIconAtlas::Initialize()
{
    BS_PROFILE_SCOPE("InputIconAtlas::Initialize");
    if (initialized) return;
    initialized = true;
    for (std::size_t index_ = 0; index_ < sheets.size(); ++index_)
        CPUResourceLoader::LoadTextureFromFile(GraphicsDeviceManager::Instance().GetDevice(),SHEET_PATHS[index_],
            sheets[index_].texture.GetAddressOf(),&sheets[index_].description,TextureType::Data);
}

InputIconAtlas::Region InputIconAtlas::Resolve(InputDisplayDevice device_, int code_)
{
    if (device_ == InputDisplayDevice::KEYBOARD_MOUSE && code_ >= VK_NUMPAD0 && code_ <= VK_NUMPAD9) code_ = '0' + code_ - VK_NUMPAD0;
    int column_ = -1;
    int row_ = -1;
    const bool keyboard_ = device_ == InputDisplayDevice::KEYBOARD_MOUSE;
    if (keyboard_)
    {
        if (code_ >= '0' && code_ <= '9')
        {
            const int digit_ = code_ - '0';
            column_ = digit_ < 8 ? digit_ * 2 + 2 : (digit_ - 8) * 2 + 1;
            row_ = digit_ < 8 ? 15 : 14;
        }
        else if (code_ >= VK_F1 && code_ <= VK_F12)
        {
            constexpr int COLUMNS[] = {3,11,13,15,0,2,4,6,8,5,7,9};
            constexpr int ROWS[] = {9,9,9,9,8,8,8,8,8,9,9,9};
            column_ = COLUMNS[code_ - VK_F1]; row_ = ROWS[code_ - VK_F1];
        }
        else for (const auto& cell_ : KEY_CELLS)
            if (cell_.code == code_) { column_ = cell_.column; row_ = cell_.row; break; }
    }
    else for (const auto& cell_ : PAD_CELLS)
        if (static_cast<int>(cell_.code) == code_)
        {
            const bool ps_ = device_ == InputDisplayDevice::PLAYSTATION_PAD;
            column_ = ps_ ? cell_.psColumn : cell_.xboxColumn;
            row_ = ps_ ? cell_.psRow : cell_.xboxRow;
            break;
        }
    if (column_ < 0 || row_ < 0) return {};
    const float cellSize_ = device_ == InputDisplayDevice::PLAYSTATION_PAD ? 128.0f : 64.0f;
    // 半テクセル内側で隣のセルの補間混入を避け、縦横比は1:1を維持する。
    return {{column_ * cellSize_ + 0.5f,row_ * cellSize_ + 0.5f,cellSize_ - 1.0f,cellSize_ - 1.0f},true};
}

InputCombo InputIconAtlas::FindBinding(const std::string& action_, InputDisplayDevice device_)
{
    const auto& map_ = KeyConfig::Instance().GetActionMap();
    const auto iterator_ = map_.find(action_);
    if (iterator_ == map_.end()) return {};
    const auto inputDevice_ = device_ == InputDisplayDevice::KEYBOARD_MOUSE ? DeviceType::KBM : DeviceType::Gamepad;
    // OR候補から表示デバイスだけで成立する最初のAND入力を選ぶ。
    for (const auto& combo_ : iterator_->second)
        if (!combo_.empty() && std::all_of(combo_.begin(),combo_.end(),[inputDevice_](const InputUnit& unit_) { return unit_.device == inputDevice_; })) return combo_;
    return {};
}

std::string InputIconAtlas::GetKeyLabel(InputDisplayDevice device_, int code_)
{
    if (device_ == InputDisplayDevice::KEYBOARD_MOUSE)
    {
        switch (code_)
        {
        case VK_LBUTTON: return "LMB"; case VK_RBUTTON: return "RMB"; case VK_MBUTTON: return "MMB";
        case VK_XBUTTON1: return "Mouse4"; case VK_XBUTTON2: return "Mouse5";
        default: break;
        }
        wchar_t name_[128]{};
        const auto scan_ = MapVirtualKeyW(static_cast<UINT>(code_),MAPVK_VK_TO_VSC_EX);
        const LONG bits_ = static_cast<LONG>((scan_ & 0xFF) << 16) | ((scan_ & 0xFF00) ? (1L << 24) : 0);
        if (GetKeyNameTextW(bits_,name_,128) > 0)
        {
            const int length_ = WideCharToMultiByte(CP_UTF8,0,name_,-1,nullptr,0,nullptr,nullptr);
            std::string result_(static_cast<std::size_t>(length_), '\0');
            WideCharToMultiByte(CP_UTF8,0,name_,-1,result_.data(),length_,nullptr,nullptr);
            result_.pop_back();
            return result_;
        }
        return "VK " + std::to_string(code_);
    }
    const bool ps_ = device_ == InputDisplayDevice::PLAYSTATION_PAD;
    switch (static_cast<PadCode>(code_))
    {
    case PadCode::A: return ps_ ? "×" : "A"; case PadCode::B: return ps_ ? "○" : "B";
    case PadCode::X: return ps_ ? "□" : "X"; case PadCode::Y: return ps_ ? "△" : "Y";
    case PadCode::L1: return ps_ ? "L1" : "LB"; case PadCode::R1: return ps_ ? "R1" : "RB";
    case PadCode::L2: return ps_ ? "L2" : "LT"; case PadCode::R2: return ps_ ? "R2" : "RT";
    case PadCode::L3: return ps_ ? "L3" : "LS"; case PadCode::R3: return ps_ ? "R3" : "RS";
    case PadCode::TOUCHPAD: return "Touchpad"; case PadCode::GUIDE: return "Guide";
    case PadCode::START: return ps_ ? "Options" : "Menu"; case PadCode::BACK: return ps_ ? "Share" : "View";
    case PadCode::UP: return "DPad Up"; case PadCode::DOWN: return "DPad Down";
    case PadCode::LEFT: return "DPad Left"; case PadCode::RIGHT: return "DPad Right";
    case PadCode::STICK_L_UP: return "LS Up"; case PadCode::STICK_L_DOWN: return "LS Down";
    case PadCode::STICK_L_LEFT: return "LS Left"; case PadCode::STICK_L_RIGHT: return "LS Right";
    case PadCode::STICK_R_UP: return "RS Up"; case PadCode::STICK_R_DOWN: return "RS Down";
    case PadCode::STICK_R_LEFT: return "RS Left"; case PadCode::STICK_R_RIGHT: return "RS Right";
    default: return "Pad " + std::to_string(code_);
    }
}

const char* InputIconAtlas::GetDeviceName(InputDisplayDevice device_)
{
    constexpr const char* NAMES[] = {"Keyboard & Mouse","Xbox","PlayStation"};
    return NAMES[static_cast<int>(device_)];
}

bool InputIconAtlas::Apply(SpriteRenderer& sprite_, InputDisplayDevice device_, int code_) const
{
    const auto region_ = Resolve(device_,code_);
    const auto& sheet_ = sheets[static_cast<int>(device_)];
    if (!region_.valid || !sheet_.texture) return false;
    sprite_.SetTextureResource(sheet_.texture,sheet_.description,SPRITE_PATHS[static_cast<int>(device_)]);
    sprite_.SetUVRect(region_.source.x,region_.source.y,region_.source.z,region_.source.w);
    return true;
}

void InputIconAtlas::Gui(InputDisplayDevice device_) const
{
    BS_PROFILE_SCOPE("InputIconAtlas::Gui");
    if (!ImGui::TreeNode("Input SpriteSheet / SourceRect")) return;
    const auto& sheet_ = sheets[static_cast<int>(device_)];
    if (sheet_.texture)
    {
        const float ratio_ = static_cast<float>(sheet_.description.Height) / static_cast<float>(sheet_.description.Width);
        ImGui::Image(reinterpret_cast<void*>(sheet_.texture.Get()),ImVec2(300,300 * ratio_));
        static int code_ = VK_SPACE;
        ImGui::InputInt("Input Code (VK / PadCode)",&code_);
        const auto region_ = Resolve(device_,code_);
        if (region_.valid)
        {
            const auto& source_ = region_.source;
            ImGui::Text("SourceRect %.1f %.1f %.1f %.1f",source_.x,source_.y,source_.z,source_.w);
            ImGui::Image(reinterpret_cast<void*>(sheet_.texture.Get()),ImVec2(64,64),
                ImVec2(source_.x / sheet_.description.Width,source_.y / sheet_.description.Height),
                ImVec2((source_.x + source_.z) / sheet_.description.Width,(source_.y + source_.w) / sheet_.description.Height));
        }
        else ImGui::TextUnformatted("Text fallback (icon unavailable)");
    }
    else ImGui::TextUnformatted("SpriteSheet load failed");
    ImGui::TreePop();
}

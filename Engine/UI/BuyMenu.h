#pragma once

#include "../Core/BuyCart.h"
#include "../Sim/PlayerInput.h"

#include <SDL3/SDL.h>

#include <array>
#include <string>
#include <vector>

namespace Atlas
{
    class Window;
    class World;
    struct WeaponDef;

    // What the buy menu needs to know about the match this frame.
    struct BuyMenuContext
    {
        int Gold = 0;
        int AlliesFielded = 0;
        int DronesFielded = 0;
        int LocalPlayer = 0;

        float Time = 0.0f;
    };

    // The Cortex Command-style order screen. Tab (or B) opens it; the
    // catalog sits on the left (tabs for bodies, weapons, tools, support,
    // crafts and saved presets), the order on the right, and the middle
    // of the screen stays clear so the player can keep moving.
    //
    // Immediate mode: Frame() handles this frame's mouse and keyboard and
    // draws in one pass, so what is clicked is always what is drawn. A
    // finished order is collected with TakeOrder().
    class BuyMenu
    {
    public:
        BuyMenu(const World& world, const WeaponDef* defaultWeapon);

        // Tab / B toggle, read before the frame's other input.
        void HandleToggleKeys();

        void Open();
        void Close();

        // Open or picking a landing zone: the world should not see
        // clicks, number keys or the scroll wheel.
        bool IsActive() const;
        bool IsOpen() const;

        // Runs and draws the menu (and the landing-zone beacon), in
        // screen space. Call once a frame, after the HUD.
        void Frame(Window& window, const BuyMenuContext& context);

        // Markers for orders in flight (world space, before lighting).
        void DrawInbound(Window& window, const BuyMenuContext& context) const;

        // An order the player confirmed, ready to submit.
        bool TakeOrder(OrderRequest& out);

    private:
        enum class Tab
        {
            Bodies,
            Weapons,
            Tools,
            Support,
            Crafts,
            Presets,
            Count,
        };

        enum class EntryKind
        {
            Body,
            Weapon,
            Drone,
            Crate,
            Craft,
        };

        struct Entry
        {
            EntryKind Kind;
            std::string Name;
            std::string Blurb;
            const WeaponDef* Weapon = nullptr;
            int Value = 0; // Body: BodyClass; Craft: CraftKind
            int Cost = 0;
        };

        struct Preset
        {
            bool Used = false;
            int Craft = BuyCart::AutoCraft;
            OrderRequest Order;
        };

        static constexpr int PresetCount = 8;

        // --- Catalog ---
        void BuildCatalog();
        const std::vector<Entry>& EntriesFor(Tab tab) const;
        std::string RejectReason(const Entry& entry, const BuyMenuContext& context) const;
        void Activate(const Entry& entry, const BuyMenuContext& context);

        // --- Order ---
        int PriceCart() const;
        int CargoUnits() const;
        OrderRequest MakeOrder() const;
        std::string SendBlocker(const BuyMenuContext& context) const;
        void Send(int dropX);
        void Toast(const std::string& text, bool bad = false);

        // --- Presets ---
        void LoadPresets();
        void SavePresets() const;
        void StorePreset(int slot);
        void ApplyPreset(int slot);
        std::string PresetTitle(const Preset& preset) const;

        // --- Drawing pieces (also hit-tested) ---
        void DrawCatalog(Window& window, const BuyMenuContext& context);
        void DrawPresetList(Window& window, const BuyMenuContext& context,
            float x, float y, float w, float h);
        void DrawDetail(Window& window, const BuyMenuContext& context,
            float x, float y, float w, float h);
        void DrawOrder(Window& window, const BuyMenuContext& context);
        void DrawZonePicker(Window& window, const BuyMenuContext& context);

        bool Hover(float x, float y, float w, float h) const;

        const World& m_World;
        const WeaponDef* m_DefaultWeapon;

        BuyCart m_Cart;

        std::array<std::vector<Entry>, static_cast<std::size_t>(Tab::Count)> m_Catalog;
        std::array<Preset, PresetCount> m_Presets;

        // Stat ranges across the catalog, for the detail bars.
        float m_MaxDamage = 1.0f;
        float m_MaxRate = 1.0f;
        float m_MaxVelocity = 1.0f;
        float m_MaxDig = 1.0f;
        float m_MaxReach = 1.0f;

        bool m_Open = false;
        bool m_PickingZone = false;
        bool m_ZoneOnSend = false; // BUY asks for a landing zone first

        Tab m_Tab = Tab::Bodies;
        int m_Highlight = 0;      // keyboard row in the current tab
        int m_HoverEntry = -1;    // mouse row this frame
        float m_ListScroll = 0.0f;
        float m_OrderScroll = 0.0f;
        float m_OpenAnim = 0.0f;
        float m_LastTime = 0.0f;

        bool m_HasOrder = false;
        OrderRequest m_Order;

        std::string m_Toast;
        bool m_ToastBad = false;
        float m_ToastTime = 0.0f;

        // This frame's input.
        float m_MouseX = 0.0f;
        float m_MouseY = 0.0f;
        bool m_Click = false;
        bool m_RightClick = false;
        int m_Wheel = 0;
    };
}

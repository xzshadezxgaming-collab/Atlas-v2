#include "BuyMenu.h"

#include "../Actors/Actor.h"
#include "../Audio/Audio.h"
#include "../Combat/Weapon.h"
#include "../Core/IniFile.h"
#include "../Core/Window.h"
#include "../Graphics/PixelFont.h"
#include "../Graphics/SpriteBank.h"
#include "../Input/Input.h"
#include "../Sim/World.h"
#include "../World/Terrain.h"
#include "Ui.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>

namespace Atlas
{
    namespace
    {
        constexpr float ViewW = 1280.0f;
        constexpr float ViewH = 720.0f;

        // Catalog panel (left) and order panel (right); the middle of the
        // screen stays clear so the player can see where they're going.
        constexpr float CatX = 16.0f;
        constexpr float CatY = 70.0f;
        constexpr float CatW = 500.0f;
        constexpr float CatH = 566.0f;

        constexpr float TabsY = 42.0f;
        constexpr float TabH = 28.0f;
        constexpr float ListY = 78.0f;
        constexpr float RowH = 42.0f;
        constexpr float RowGap = 4.0f;
        constexpr int RowsShown = 7;
        constexpr float HintsX = (CatX + CatW + ViewW - 16.0f - 330.0f) * 0.5f;
        constexpr float ListH = RowsShown * (RowH + RowGap) - RowGap;
        constexpr float DetailY = ListY + ListH + 10.0f;

        constexpr float OrdW = 330.0f;
        constexpr float OrdX = ViewW - 16.0f - OrdW;
        constexpr float OrdY = CatY;
        constexpr float OrdH = CatH;

        constexpr float CraftCardY = 40.0f;
        constexpr float CraftCardH = 62.0f;
        constexpr float CargoY = CraftCardY + CraftCardH + 8.0f;
        constexpr float FooterH = 142.0f;
        constexpr float CargoH = OrdH - CargoY - FooterH - 4.0f;

        constexpr float BodyRowH = 30.0f;
        constexpr float ItemRowH = 22.0f;
        constexpr float HeaderRowH = 20.0f;

        // Palette.
        constexpr SDL_Color Accent = { 255, 196, 92, 255 };
        constexpr SDL_Color GoldText = { 240, 204, 100, 255 };
        constexpr SDL_Color Bad = { 238, 96, 84, 255 };
        constexpr SDL_Color Good = { 110, 214, 130, 255 };
        constexpr SDL_Color Dim = { 128, 134, 152, 255 };
        constexpr SDL_Color Bright = { 236, 238, 244, 255 };

        const char* TabNames[] = { "BODIES", "WEAPONS", "TOOLS", "SUPPORT", "CRAFTS", "PRESETS" };

        std::string Upper(std::string text)
        {
            for (char& c : text)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

            return text;
        }

        std::string Trim(const std::string& text)
        {
            const std::size_t first = text.find_first_not_of(" \t");

            if (first == std::string::npos)
                return std::string();

            const std::size_t last = text.find_last_not_of(" \t");
            return text.substr(first, last - first + 1);
        }

        std::vector<std::string> Split(const std::string& text, char separator)
        {
            std::vector<std::string> parts;
            std::stringstream stream(text);
            std::string part;

            while (std::getline(stream, part, separator))
                parts.push_back(Trim(part));

            return parts;
        }

        std::string PresetsPath()
        {
            char* folder = SDL_GetPrefPath("Atlas", "Atlas");

            if (!folder)
                return std::string();

            std::string path = std::string(folder) + "presets.ini";
            SDL_free(folder);
            return path;
        }

        void Text(Window& window, float x, float y, float scale, const std::string& text,
            SDL_Color color, int align = 0, Uint8 alpha = 255)
        {
            const float width = PixelFont::Measure(text, scale);

            if (align == 1)
                x -= width * 0.5f;
            else if (align == 2)
                x -= width;

            const float shadow = std::max(1.0f, scale * 0.5f);
            PixelFont::Draw(window, x + shadow, y + shadow, scale, text, 6, 6, 10,
                static_cast<Uint8>(alpha * 0.6f));
            PixelFont::Draw(window, x, y, scale, text, color.r, color.g, color.b, alpha);
        }

        // Text height for a scale (5x7 face below 2, 3x5 face above).
        float TextH(float scale)
        {
            return scale < 2.0f ? 7.0f : 5.0f * scale;
        }

        // The house panel (chamfered glass with the amber accent).
        void PanelBox(Window& window, float x, float y, float w, float h)
        {
            Ui ui(window);
            ui.Panel(x, y, w, h);
        }

        // A row/card face. lit 0..1 brightens it and adds the accent bar.
        void Card(Window& window, float x, float y, float w, float h, float lit,
            bool enabled = true)
        {
            const float base = enabled ? 1.0f : 0.6f;
            const Uint8 top = static_cast<Uint8>((30.0f + lit * 22.0f) * base);
            const Uint8 bottom = static_cast<Uint8>((20.0f + lit * 12.0f) * base);

            window.DrawGradientScreen(x, y, w, h,
                { top, static_cast<Uint8>(top + 3), static_cast<Uint8>(top + 12), 235 },
                { bottom, static_cast<Uint8>(bottom + 2), static_cast<Uint8>(bottom + 9), 240 });
            window.DrawScreenRect(x, y, w, 1.0f, 255, 255, 255, static_cast<Uint8>(10 + lit * 26));

            if (lit > 0.0f)
            {
                window.DrawScreenRect(x, y, 3.0f, h, Accent.r, Accent.g, Accent.b,
                    static_cast<Uint8>(lit * 255.0f));
            }
        }

        // Draws a sprite as large as fits (whole-number scale, so pixel
        // art stays crisp), centered in the box.
        void Icon(Window& window, const std::string& name, float x, float y, float w, float h,
            Uint8 alpha = 255, int maxScale = 4)
        {
            const SpriteFrame* sprite = SpriteBank::Find(name);

            if (!sprite || sprite->Width <= 0.0f || sprite->Height <= 0.0f)
                return;

            float scale = std::floor(std::min(w / sprite->Width, h / sprite->Height));
            scale = std::clamp(scale, 1.0f, static_cast<float>(maxScale));

            const float dw = sprite->Width * scale;
            const float dh = sprite->Height * scale;

            window.DrawSpriteScreen(*sprite, std::round(x + (w - dw) * 0.5f),
                std::round(y + (h - dh) * 0.5f), scale, alpha);
        }

        const char* BodySprite(int bodyClass)
        {
            return bodyClass == static_cast<int>(BodyClass::Light) ? "body_scout"
                : bodyClass == static_cast<int>(BodyClass::Heavy) ? "body_heavy"
                : "body_rifleman";
        }

        std::string WeaponBlurb(const WeaponDef& def)
        {
            const std::string name = Upper(def.Name);

            if (name == "SMG") return "HIGH RATE OF FIRE. HOSES DOWN ANYTHING CLOSE.";
            if (name == "SHOTGUN") return "SEVEN PELLETS A SHOT. TEARS LIMBS OFF UP CLOSE.";
            if (name == "RIFLE") return "ACCURATE AND PUNCHY. SHOOTS THROUGH DIRT.";
            if (name == "SNIPER") return "HUGE DAMAGE AND A LASER SIGHT. SLOW TO CYCLE.";
            if (name == "ROCKET LAUNCHER") return "ROCKETS BLAST TERRAIN AND SQUADS ALIKE.";
            if (name == "FLAMETHROWER") return "SETS TARGETS ALIGHT. SHORT RANGE.";
            if (name == "DIGGER") return "CHEWS THROUGH ROCK AND GOLD ORE. CAN MAIM.";
            if (name == "SHOVEL") return "MOVES DIRT FAST. USELESS ON STONE.";

            switch (def.Kind)
            {
            case WeaponKind::Digger:
            case WeaponKind::Shovel: return "A DIGGING TOOL.";
            case WeaponKind::Launcher: return "FIRES EXPLOSIVE ROCKETS.";
            case WeaponKind::Flamer: return "SPRAYS FIRE.";
            default: return "A FIREARM.";
            }
        }

        bool IsTool(const WeaponDef& def)
        {
            return def.Kind == WeaponKind::Digger || def.Kind == WeaponKind::Shovel;
        }

        float WeaponDamage(const WeaponDef& def)
        {
            if (def.Kind == WeaponKind::Launcher)
                return static_cast<float>(def.BlastDamage);

            return static_cast<float>(def.Damage * std::max(1, def.Pellets));
        }

        std::string Fixed1(float value)
        {
            const int tenths = static_cast<int>(std::lround(value * 10.0f));
            return std::to_string(tenths / 10) + "." + std::to_string(std::abs(tenths % 10));
        }

        // Splits text into lines no wider than width.
        std::vector<std::string> Wrap(const std::string& text, float scale, float width)
        {
            std::vector<std::string> lines;
            std::string line;

            for (const std::string& word : Split(text, ' '))
            {
                const std::string candidate = line.empty() ? word : line + " " + word;

                if (!line.empty() && PixelFont::Measure(candidate, scale) > width)
                {
                    lines.push_back(line);
                    line = word;
                }
                else
                {
                    line = candidate;
                }
            }

            if (!line.empty())
                lines.push_back(line);

            return lines;
        }

        void StatBar(Window& window, float x, float y, float w, const std::string& label,
            float fraction, const std::string& value)
        {
            Text(window, x, y, 1.0f, label, Dim);

            const float barX = x + 84.0f;
            const float barW = w - 84.0f - 48.0f;

            window.DrawScreenRect(barX, y + 1.0f, barW, 6.0f, 40, 44, 58, 255);
            window.DrawGradientScreen(barX, y + 1.0f, barW * std::clamp(fraction, 0.04f, 1.0f), 6.0f,
                { 255, 214, 120, 255 }, { 220, 150, 60, 255 });

            Text(window, x + w, y, 1.0f, value, Bright, 2);
        }

        // A small square button with an X in it.
        bool RemoveButton(Window& window, float x, float y, float size, float mouseX, float mouseY,
            bool click)
        {
            const bool hover = mouseX >= x && mouseX < x + size && mouseY >= y && mouseY < y + size;

            window.DrawScreenRect(x, y, size, size,
                hover ? 150 : 52, hover ? 54 : 40, hover ? 50 : 46, 255);
            Text(window, x + size * 0.5f, y + (size - 7.0f) * 0.5f, 1.0f, "X",
                hover ? Bright : Dim, 1);

            return hover && click;
        }

        // A full-width text button. Returns true when clicked.
        bool Button(Window& window, float x, float y, float w, float h, const std::string& label,
            float mouseX, float mouseY, bool click, bool enabled, float scale = 2.0f,
            bool primary = false, float time = 0.0f)
        {
            const bool hover = enabled && mouseX >= x && mouseX < x + w && mouseY >= y && mouseY < y + h;

            if (primary && enabled)
            {
                const float pulse = 0.5f + 0.5f * std::sin(time * 4.0f);
                window.DrawScreenRect(x - 2.0f, y - 2.0f, w + 4.0f, h + 4.0f,
                    Accent.r, Accent.g, Accent.b, static_cast<Uint8>(30 + 40 * pulse));
                window.DrawGradientScreen(x, y, w, h,
                    hover ? SDL_Color{ 255, 214, 120, 255 } : SDL_Color{ 246, 186, 80, 255 },
                    hover ? SDL_Color{ 236, 160, 60, 255 } : SDL_Color{ 206, 132, 44, 255 });
                window.DrawScreenRect(x, y, w, 1.0f, 255, 255, 255, 90);
            }
            else
            {
                Card(window, x, y, w, h, hover ? 1.0f : 0.0f, enabled);
            }

            const SDL_Color color = primary && enabled ? SDL_Color{ 30, 20, 8, 255 }
                : enabled ? (hover ? Bright : SDL_Color{ 206, 210, 222, 255 }) : Dim;

            if (primary && enabled)
            {
                PixelFont::Draw(window, x + (w - PixelFont::Measure(label, scale)) * 0.5f,
                    y + (h - TextH(scale)) * 0.5f, scale, label, color.r, color.g, color.b);
            }
            else
            {
                Text(window, x + w * 0.5f, y + (h - TextH(scale)) * 0.5f, scale, label, color, 1);
            }

            if (hover && click)
            {
                Audio::Play(Sfx::UiClick, 0.45f);
                return true;
            }

            return false;
        }
    }

    BuyMenu::BuyMenu(const World& world, const WeaponDef* defaultWeapon)
        : m_World(world)
        , m_DefaultWeapon(defaultWeapon)
    {
        BuildCatalog();
        LoadPresets();
    }

    // ------------------------------------------------------------------
    // Catalog
    // ------------------------------------------------------------------

    void BuyMenu::BuildCatalog()
    {
        auto& bodies = m_Catalog[static_cast<std::size_t>(Tab::Bodies)];
        auto& weapons = m_Catalog[static_cast<std::size_t>(Tab::Weapons)];
        auto& tools = m_Catalog[static_cast<std::size_t>(Tab::Tools)];
        auto& support = m_Catalog[static_cast<std::size_t>(Tab::Support)];
        auto& crafts = m_Catalog[static_cast<std::size_t>(Tab::Crafts)];

        const char* bodyBlurbs[] =
        {
            "FAST AND LIGHT WITH A STRONG JETPACK. FRAGILE.",
            "THE BALANCED ALL-ROUNDER.",
            "SLOW, TOUGH, ARMORED. TAKES 30% LESS DAMAGE.",
        };

        for (int c = 0; c < static_cast<int>(BodyClass::Count); c++)
        {
            const BodyClassInfo& info = GetBodyClassInfo(static_cast<BodyClass>(c));
            bodies.push_back({ EntryKind::Body, info.Name, c < 3 ? bodyBlurbs[c] : "",
                nullptr, c, info.Cost });
        }

        for (const WeaponDef& def : m_World.GetWeaponDefs())
        {
            if (def.Cost <= 0)
                continue;

            Entry entry{ EntryKind::Weapon, Upper(def.Name), WeaponBlurb(def), &def, 0, def.Cost };
            (IsTool(def) ? tools : weapons).push_back(entry);

            if (IsTool(def))
            {
                m_MaxDig = std::max(m_MaxDig, def.DigPower * def.FireRate);
                m_MaxReach = std::max(m_MaxReach, def.DigRange);
            }
            else
            {
                m_MaxDamage = std::max(m_MaxDamage, WeaponDamage(def));
                m_MaxRate = std::max(m_MaxRate, def.FireRate);
                m_MaxVelocity = std::max(m_MaxVelocity, def.MuzzleVelocity);
            }
        }

        // Cheapest first inside each tab, like a shop shelf.
        auto byCost = [](const Entry& a, const Entry& b) { return a.Cost < b.Cost; };
        std::stable_sort(weapons.begin(), weapons.end(), byCost);
        std::stable_sort(tools.begin(), tools.end(), byCost);

        support.push_back({ EntryKind::Crate, "SUPPLY CRATE",
            "HEALS YOU 50 AND REFILLS ALL YOUR AMMO.", nullptr, 0, World::SupplyCrateCost });
        support.push_back({ EntryKind::Drone, "GUN DRONE",
            "AUTONOMOUS FLYING GUN. HUNTS NEARBY ENEMIES.", nullptr, 0, World::DroneCost });

        crafts.push_back({ EntryKind::Craft, "AUTO",
            "PICKS THE CHEAPEST CRAFT THAT FITS YOUR ORDER.", nullptr, BuyCart::AutoCraft, 0 });

        for (int c = 0; c < static_cast<int>(CraftKind::Count); c++)
        {
            const CraftInfo& info = GetCraftInfo(static_cast<CraftKind>(c));
            crafts.push_back({ EntryKind::Craft, info.Name, info.Blurb, nullptr, c, info.Cost });
        }
    }

    const std::vector<BuyMenu::Entry>& BuyMenu::EntriesFor(Tab tab) const
    {
        return m_Catalog[static_cast<std::size_t>(tab)];
    }

    std::string BuyMenu::RejectReason(const Entry& entry, const BuyMenuContext& context) const
    {
        const int biggest = GetCraftInfo(static_cast<CraftKind>(
            static_cast<int>(CraftKind::Count) - 1)).Capacity;

        switch (entry.Kind)
        {
        case EntryKind::Body:
            if (context.AlliesFielded + static_cast<int>(m_Cart.GetBodies().size()) >= World::MaxAllies)
                return "SQUAD FULL";

            if (CargoUnits() + Atlas::CargoUnits(DeliveryKind::Reinforcement, entry.Value) > biggest)
                return "NO ROOM";
            break;

        case EntryKind::Drone:
            if (context.DronesFielded + m_Cart.GetDroneCount() >= World::MaxDronesPerTeam)
                return "DRONE LIMIT";

            if (CargoUnits() + Atlas::CargoUnits(DeliveryKind::Drone) > biggest)
                return "NO ROOM";
            break;

        case EntryKind::Crate:
            if (CargoUnits() + Atlas::CargoUnits(DeliveryKind::SupplyCrate) > biggest)
                return "NO ROOM";
            break;

        case EntryKind::Weapon:
        {
            const int active = m_Cart.GetActiveBodyIndex();

            if (active >= 0)
            {
                const CartBody& body = m_Cart.GetBodies()[static_cast<std::size_t>(active)];

                if (std::find(body.Weapons.begin(), body.Weapons.end(), entry.Weapon) != body.Weapons.end())
                    return "CARRIED";

                if (static_cast<int>(body.Weapons.size()) >= BuyCart::MaxWeaponsPerBody)
                    return "HANDS FULL";
            }
            else if (CargoUnits() + Atlas::CargoUnits(DeliveryKind::WeaponPickup) > biggest)
            {
                return "NO ROOM";
            }
            break;
        }

        case EntryKind::Craft:
            break;
        }

        return std::string();
    }

    void BuyMenu::Activate(const Entry& entry, const BuyMenuContext& context)
    {
        const std::string reason = RejectReason(entry, context);

        if (!reason.empty())
        {
            const std::string why =
                reason == "SQUAD FULL" ? "SQUAD FULL: " + std::to_string(World::MaxAllies) + " BODIES MAX"
                : reason == "DRONE LIMIT" ? "DRONE LIMIT: " + std::to_string(World::MaxDronesPerTeam) + " IN THE AIR"
                : reason == "NO ROOM" ? "NO CRAFT CAN CARRY ANY MORE"
                : reason == "CARRIED" ? "THIS BODY ALREADY CARRIES ONE"
                : reason == "HANDS FULL" ? "THIS BODY CAN'T CARRY ANY MORE"
                : reason;

            Toast(why, true);
            return;
        }

        switch (entry.Kind)
        {
        case EntryKind::Body:
            m_Cart.AddBody(entry.Value);
            break;

        case EntryKind::Weapon:
            if (m_Cart.AddWeapon(entry.Weapon) == BuyCart::AddResult::Loose)
                Toast("NO BODY SELECTED: " + entry.Name + " DROPS AS LOOSE CARGO");
            break;

        case EntryKind::Crate:
            m_Cart.AddSupplyCrate();
            break;

        case EntryKind::Drone:
            m_Cart.AddDrone();
            break;

        case EntryKind::Craft:
            m_Cart.SetCraftChoice(entry.Value);
            break;
        }

        Audio::Play(Sfx::UiClick, 0.45f);
    }

    // ------------------------------------------------------------------
    // Order
    // ------------------------------------------------------------------

    OrderRequest BuyMenu::MakeOrder() const
    {
        OrderRequest order = m_Cart.ToOrder(m_DefaultWeapon);
        order.Craft = static_cast<std::uint8_t>(m_Cart.ResolveCraft());
        return order;
    }

    int BuyMenu::PriceCart() const
    {
        return m_World.PriceOrder(MakeOrder());
    }

    int BuyMenu::CargoUnits() const
    {
        return World::OrderCargoUnits(m_Cart.ToOrder(m_DefaultWeapon));
    }

    std::string BuyMenu::SendBlocker(const BuyMenuContext& context) const
    {
        if (m_Cart.IsEmpty())
            return "ADD SOMETHING TO THE ORDER";

        const CraftInfo& craft = GetCraftInfo(m_Cart.ResolveCraft());

        if (CargoUnits() > craft.Capacity)
            return std::string("TOO MUCH CARGO FOR THE ") + craft.Name;

        if (context.AlliesFielded + static_cast<int>(m_Cart.GetBodies().size()) > World::MaxAllies)
            return "SQUAD FULL";

        if (context.DronesFielded + m_Cart.GetDroneCount() > World::MaxDronesPerTeam)
            return "TOO MANY DRONES IN THE AIR";

        const int price = PriceCart();

        if (price > context.Gold)
            return "NEED " + std::to_string(price - context.Gold) + " MORE GOLD";

        return std::string();
    }

    void BuyMenu::Send(int dropX)
    {
        m_Order = MakeOrder();
        m_Order.DropX = dropX;
        m_HasOrder = true;

        const CraftInfo& craft = GetCraftInfo(static_cast<CraftKind>(m_Order.Craft));
        Toast(std::string(craft.Name) + (dropX >= 0 ? " INBOUND TO YOUR LANDING ZONE" : " INBOUND"));

        m_Cart.Clear();
        m_Open = false;
        m_PickingZone = false;
        Audio::Play(Sfx::OrderSent, 0.6f);
    }

    bool BuyMenu::TakeOrder(OrderRequest& out)
    {
        if (!m_HasOrder)
            return false;

        out = m_Order;
        m_HasOrder = false;
        return true;
    }

    void BuyMenu::Toast(const std::string& text, bool bad)
    {
        m_Toast = text;
        m_ToastBad = bad;
        m_ToastTime = 2.4f;
    }

    // ------------------------------------------------------------------
    // Presets
    // ------------------------------------------------------------------

    void BuyMenu::LoadPresets()
    {
        const std::string path = PresetsPath();
        IniFile ini;

        if (path.empty() || !ini.Load(path))
            return;

        auto findWeapon = [&](const std::string& name) -> const WeaponDef*
        {
            for (const WeaponDef& def : m_World.GetWeaponDefs())
            {
                if (Upper(def.Name) == Upper(name) && def.Cost > 0)
                    return &def;
            }

            return nullptr;
        };

        for (int i = 0; i < PresetCount; i++)
        {
            const std::string section = "Preset" + std::to_string(i + 1);

            if (!ini.HasSection(section))
                continue;

            Preset& preset = m_Presets[static_cast<std::size_t>(i)];
            preset = Preset{};
            preset.Used = true;
            preset.Craft = std::clamp(ini.GetInt(section, "Craft", BuyCart::AutoCraft),
                BuyCart::AutoCraft, static_cast<int>(CraftKind::Count) - 1);

            // Bodies = class:weapon,weapon;class:...
            for (const std::string& body : Split(ini.GetString(section, "Bodies", ""), ';'))
            {
                if (body.empty())
                    continue;

                const std::size_t colon = body.find(':');
                const int bodyClass = std::clamp(std::atoi(body.substr(0, colon).c_str()),
                    0, static_cast<int>(BodyClass::Count) - 1);

                OrderBody entry;
                entry.Class = static_cast<std::uint8_t>(bodyClass);
                entry.Weapon = m_DefaultWeapon;

                if (colon != std::string::npos)
                {
                    std::vector<const WeaponDef*> weapons;

                    for (const std::string& name : Split(body.substr(colon + 1), ','))
                    {
                        if (const WeaponDef* def = findWeapon(name))
                            weapons.push_back(def);
                    }

                    if (!weapons.empty())
                    {
                        entry.Weapon = weapons.front();
                        entry.Extras.assign(weapons.begin() + 1, weapons.end());
                    }
                }

                preset.Order.Bodies.push_back(entry);
            }

            for (const std::string& name : Split(ini.GetString(section, "Loose", ""), ','))
            {
                if (const WeaponDef* def = findWeapon(name))
                    preset.Order.LooseWeapons.push_back(def);
            }

            preset.Order.SupplyCrates = std::clamp(ini.GetInt(section, "Crates", 0), 0, 24);
            preset.Order.Drones = std::clamp(ini.GetInt(section, "Drones", 0), 0, World::MaxDronesPerTeam);
        }
    }

    void BuyMenu::SavePresets() const
    {
        const std::string path = PresetsPath();

        if (path.empty())
            return;

        std::ofstream out(path);

        if (!out)
            return;

        out << "; Buy menu presets (saved from the game).\n\n";

        for (int i = 0; i < PresetCount; i++)
        {
            const Preset& preset = m_Presets[static_cast<std::size_t>(i)];

            if (!preset.Used)
                continue;

            out << "[Preset" << (i + 1) << "]\n";
            out << "Craft = " << preset.Craft << "\n";
            out << "Bodies = ";

            for (std::size_t b = 0; b < preset.Order.Bodies.size(); b++)
            {
                const OrderBody& body = preset.Order.Bodies[b];
                out << (b > 0 ? ";" : "") << static_cast<int>(body.Class) << ":";

                std::vector<const WeaponDef*> weapons;

                if (body.Weapon && (body.Weapon != m_DefaultWeapon || !body.Extras.empty()))
                    weapons.push_back(body.Weapon);

                weapons.insert(weapons.end(), body.Extras.begin(), body.Extras.end());

                for (std::size_t w = 0; w < weapons.size(); w++)
                    out << (w > 0 ? "," : "") << weapons[w]->Name;
            }

            out << "\nLoose = ";

            for (std::size_t w = 0; w < preset.Order.LooseWeapons.size(); w++)
                out << (w > 0 ? "," : "") << preset.Order.LooseWeapons[w]->Name;

            out << "\nCrates = " << preset.Order.SupplyCrates << "\n";
            out << "Drones = " << preset.Order.Drones << "\n\n";
        }
    }

    void BuyMenu::StorePreset(int slot)
    {
        if (slot < 0 || slot >= PresetCount)
            return;

        if (m_Cart.IsEmpty())
        {
            Toast("ADD SOMETHING BEFORE SAVING A PRESET", true);
            return;
        }

        Preset& preset = m_Presets[static_cast<std::size_t>(slot)];
        preset.Used = true;
        preset.Craft = m_Cart.GetCraftChoice();
        preset.Order = m_Cart.ToOrder(m_DefaultWeapon);

        SavePresets();
        Toast("SAVED AS PRESET " + std::to_string(slot + 1) + ": " + PresetTitle(preset));
        Audio::Play(Sfx::UiClick, 0.5f);
    }

    void BuyMenu::ApplyPreset(int slot)
    {
        if (slot < 0 || slot >= PresetCount)
            return;

        const Preset& preset = m_Presets[static_cast<std::size_t>(slot)];

        if (!preset.Used)
        {
            Toast("PRESET " + std::to_string(slot + 1) + " IS EMPTY. SHIFT+" +
                std::to_string(slot + 1) + " SAVES THE ORDER THERE", true);
            return;
        }

        m_Cart.FromOrder(preset.Order, m_DefaultWeapon);
        m_Cart.SetCraftChoice(preset.Craft);
        Toast("LOADED PRESET " + std::to_string(slot + 1));
        Audio::Play(Sfx::UiClick, 0.5f);
    }

    std::string BuyMenu::PresetTitle(const Preset& preset) const
    {
        // "2 HEAVY, RIFLEMAN + 3 ITEMS"
        int counts[static_cast<int>(BodyClass::Count)] = {};

        for (const OrderBody& body : preset.Order.Bodies)
            counts[std::min<int>(body.Class, static_cast<int>(BodyClass::Count) - 1)]++;

        std::string title;

        for (int c = static_cast<int>(BodyClass::Count) - 1; c >= 0; c--)
        {
            if (counts[c] == 0)
                continue;

            if (!title.empty())
                title += ", ";

            title += (counts[c] > 1 ? std::to_string(counts[c]) + " " : std::string()) +
                GetBodyClassInfo(static_cast<BodyClass>(c)).Name;
        }

        int items = static_cast<int>(preset.Order.LooseWeapons.size()) +
            preset.Order.SupplyCrates + preset.Order.Drones;

        for (const OrderBody& body : preset.Order.Bodies)
        {
            items += static_cast<int>(body.Extras.size()) +
                (body.Weapon && body.Weapon != m_DefaultWeapon ? 1 : 0);
        }

        if (items > 0)
        {
            const std::string gear = std::to_string(items) + (items == 1 ? " ITEM" : " ITEMS");
            title = title.empty() ? gear : title + " + " + gear;
        }

        return title.empty() ? "EMPTY" : title;
    }

    // ------------------------------------------------------------------
    // Frame
    // ------------------------------------------------------------------

    void BuyMenu::HandleToggleKeys()
    {
        if (Input::WasKeyPressed(SDL_SCANCODE_TAB) || Input::WasKeyPressed(SDL_SCANCODE_B))
        {
            if (m_PickingZone)
            {
                m_PickingZone = false;
                m_Open = true;
            }
            else if (m_Open)
            {
                Close();
            }
            else
            {
                Open();
            }
        }
    }

    void BuyMenu::Open()
    {
        m_Open = true;
        m_PickingZone = false;
        m_OpenAnim = 0.0f;
        Audio::Play(Sfx::UiClick, 0.35f);
    }

    void BuyMenu::Close()
    {
        m_Open = false;
        m_PickingZone = false;
    }

    bool BuyMenu::IsActive() const
    {
        return m_Open || m_PickingZone;
    }

    bool BuyMenu::IsOpen() const
    {
        return m_Open;
    }

    bool BuyMenu::Hover(float x, float y, float w, float h) const
    {
        return m_MouseX >= x && m_MouseX < x + w && m_MouseY >= y && m_MouseY < y + h;
    }

    void BuyMenu::Frame(Window& window, const BuyMenuContext& context)
    {
        const float dt = std::clamp(context.Time - m_LastTime, 0.0f, 0.1f);
        m_LastTime = context.Time;
        m_ToastTime = std::max(0.0f, m_ToastTime - dt);

        m_MouseX = Input::GetMouseX();
        m_MouseY = Input::GetMouseY();
        m_Click = Input::WasMouseButtonPressed(SDL_BUTTON_LEFT);
        m_RightClick = Input::WasMouseButtonPressed(SDL_BUTTON_RIGHT);
        m_Wheel = IsActive() ? Input::ConsumeWheelSteps() : 0;

        if (m_PickingZone)
        {
            DrawZonePicker(window, context);
        }
        else if (m_Open)
        {
            m_OpenAnim = std::min(1.0f, m_OpenAnim + dt * 9.0f);

            // --- Keyboard ---
            const std::vector<Entry>& entries = EntriesFor(m_Tab);
            const int tabCount = static_cast<int>(Tab::Count);
            const int rowCount = m_Tab == Tab::Presets ? PresetCount : static_cast<int>(entries.size());

            if (Input::WasKeyPressed(SDL_SCANCODE_RIGHT))
            {
                m_Tab = static_cast<Tab>((static_cast<int>(m_Tab) + 1) % tabCount);
                m_Highlight = 0;
                m_ListScroll = 0.0f;
            }

            if (Input::WasKeyPressed(SDL_SCANCODE_LEFT))
            {
                m_Tab = static_cast<Tab>((static_cast<int>(m_Tab) + tabCount - 1) % tabCount);
                m_Highlight = 0;
                m_ListScroll = 0.0f;
            }

            if (Input::WasKeyPressed(SDL_SCANCODE_DOWN) && rowCount > 0)
                m_Highlight = (m_Highlight + 1) % rowCount;

            if (Input::WasKeyPressed(SDL_SCANCODE_UP) && rowCount > 0)
                m_Highlight = (m_Highlight + rowCount - 1) % rowCount;

            m_Highlight = std::clamp(m_Highlight, 0, std::max(0, rowCount - 1));

            if (Input::WasKeyPressed(SDL_SCANCODE_RETURN) || Input::WasKeyPressed(SDL_SCANCODE_KP_ENTER))
            {
                if (m_Tab == Tab::Presets)
                    ApplyPreset(m_Highlight);
                else if (m_Highlight < static_cast<int>(entries.size()))
                    Activate(entries[static_cast<std::size_t>(m_Highlight)], context);
            }

            if (Input::WasKeyPressed(SDL_SCANCODE_BACKSPACE))
                m_Cart.Undo();

            if (Input::WasKeyPressed(SDL_SCANCODE_DELETE))
                m_Cart.Clear();

            if (Input::WasKeyPressed(SDL_SCANCODE_C))
            {
                const int next = m_Cart.GetCraftChoice() + 1;
                m_Cart.SetCraftChoice(next >= static_cast<int>(CraftKind::Count) ? BuyCart::AutoCraft : next);
                Audio::Play(Sfx::UiClick, 0.4f);
            }

            if (Input::WasKeyPressed(SDL_SCANCODE_L))
                m_ZoneOnSend = !m_ZoneOnSend;

            const bool shift = Input::IsKeyDown(SDL_SCANCODE_LSHIFT) || Input::IsKeyDown(SDL_SCANCODE_RSHIFT);

            for (int i = 0; i < PresetCount; i++)
            {
                if (Input::WasKeyPressed(static_cast<SDL_Scancode>(SDL_SCANCODE_1 + i)))
                {
                    if (shift)
                        StorePreset(i);
                    else
                        ApplyPreset(i);
                }
            }

            // Keep the keyboard row on screen.
            const float rowTop = static_cast<float>(m_Highlight) * (RowH + RowGap);

            if (rowTop < m_ListScroll)
                m_ListScroll = rowTop;
            else if (rowTop + RowH > m_ListScroll + ListH)
                m_ListScroll = rowTop + RowH - ListH;

            // Soft backdrop so the panels read against a busy scene.
            window.DrawScreenRect(0.0f, 0.0f, ViewW, ViewH, 4, 6, 12, static_cast<Uint8>(70 * m_OpenAnim));

            DrawCatalog(window, context);
            DrawOrder(window, context);

            // F sends, after the order panel has had its say this frame.
            if (m_Open && Input::WasKeyPressed(SDL_SCANCODE_F))
            {
                const std::string blocker = SendBlocker(context);

                if (!blocker.empty())
                    Toast(blocker, true);
                else if (m_ZoneOnSend)
                    m_PickingZone = true, m_Open = false;
                else
                    Send(-1);
            }

            // Key hints in the clear space between the panels.
            if (m_Open)
            {
                const char* hints[] =
                {
                    "CLICK / ENTER   ADD",
                    "RIGHT CLICK   REMOVE",
                    "BACKSPACE   UNDO",
                    "ARROWS   BROWSE",
                    "1-8   LOAD PRESET",
                    "SHIFT+1-8   SAVE PRESET",
                    "F   BUY",
                    "TAB / ESC   CLOSE",
                };

                float hy = CatY + 6.0f;

                for (const char* hint : hints)
                {
                    Text(window, HintsX, hy, 1.0f, hint, Dim, 1, static_cast<Uint8>(200 * m_OpenAnim));
                    hy += 12.0f;
                }
            }
        }

        // Toast (shown even after the menu closes, e.g. "INBOUND").
        if (m_ToastTime > 0.0f && !m_Toast.empty())
        {
            const float alpha = std::min(1.0f, m_ToastTime * 3.0f);
            const float w = PixelFont::Measure(m_Toast, 2.0f) + 32.0f;
            const float x = (ViewW - w) * 0.5f;
            const float y = CatY + CatH - 44.0f;
            const SDL_Color edge = m_ToastBad ? Bad : Good;

            window.DrawScreenRect(x, y, w, 30.0f, 12, 14, 20, static_cast<Uint8>(230 * alpha));
            window.DrawScreenRect(x, y, 3.0f, 30.0f, edge.r, edge.g, edge.b, static_cast<Uint8>(255 * alpha));
            Text(window, ViewW * 0.5f, y + 10.0f, 2.0f, m_Toast,
                m_ToastBad ? SDL_Color{ 255, 170, 160, 255 } : Bright, 1, static_cast<Uint8>(255 * alpha));
        }
    }

    void BuyMenu::DrawCatalog(Window& window, const BuyMenuContext& context)
    {
        const float slide = (1.0f - m_OpenAnim) * (1.0f - m_OpenAnim) * 60.0f;
        const float x = CatX - slide;
        const float y = CatY;

        PanelBox(window, x, y, CatW, CatH);

        Text(window, x + 14.0f, y + 13.0f, 3.0f, "ORDER BAY", Bright);

        // Gold, top right.
        {
            const std::string gold = std::to_string(context.Gold);
            const float goldW = PixelFont::Measure(gold, 3.0f);
            Text(window, x + CatW - 14.0f, y + 13.0f, 3.0f, gold, GoldText, 2);
            Icon(window, "icon_gold", x + CatW - 14.0f - goldW - 24.0f, y + 9.0f, 20.0f, 22.0f);
        }

        // --- Tabs ---
        const int tabCount = static_cast<int>(Tab::Count);
        const float tabGap = 4.0f;
        const float tabW = (CatW - 16.0f - tabGap * static_cast<float>(tabCount - 1)) / static_cast<float>(tabCount);

        for (int i = 0; i < tabCount; i++)
        {
            const float tx = x + 8.0f + static_cast<float>(i) * (tabW + tabGap);
            const float ty = y + TabsY;
            const bool current = static_cast<int>(m_Tab) == i;
            const bool hover = Hover(tx, ty, tabW, TabH);

            if (current)
            {
                window.DrawGradientScreen(tx, ty, tabW, TabH, { 60, 54, 40, 255 }, { 34, 30, 24, 255 });
                window.DrawScreenRect(tx, ty + TabH - 2.0f, tabW, 2.0f, Accent.r, Accent.g, Accent.b, 255);
            }
            else
            {
                Card(window, tx, ty, tabW, TabH, hover ? 0.5f : 0.0f);
            }

            Text(window, tx + tabW * 0.5f, ty + (TabH - 7.0f) * 0.5f, 1.0f, TabNames[i],
                current ? Accent : hover ? Bright : SDL_Color{ 170, 176, 194, 255 }, 1);

            if (hover && m_Click && !current)
            {
                m_Tab = static_cast<Tab>(i);
                m_Highlight = 0;
                m_ListScroll = 0.0f;
                Audio::Play(Sfx::UiClick, 0.35f);
            }
        }

        const float listX = x + 8.0f;
        const float listY = y + ListY;
        const float listW = CatW - 16.0f;

        if (m_Tab == Tab::Presets)
        {
            DrawPresetList(window, context, listX, listY, listW, ListH);
        }
        else
        {
            const std::vector<Entry>& entries = EntriesFor(m_Tab);
            const float contentH = static_cast<float>(entries.size()) * (RowH + RowGap) - RowGap;
            const float maxScroll = std::max(0.0f, contentH - ListH);

            if (m_Wheel != 0 && Hover(listX, listY, listW, ListH))
                m_ListScroll -= static_cast<float>(m_Wheel) * (RowH + RowGap);

            m_ListScroll = std::clamp(m_ListScroll, 0.0f, maxScroll);

            const SDL_Rect clip = { static_cast<int>(listX), static_cast<int>(listY),
                static_cast<int>(listW), static_cast<int>(ListH) };
            SDL_SetRenderClipRect(window.GetRenderer(), &clip);

            m_HoverEntry = -1;
            const int price = PriceCart();

            for (std::size_t i = 0; i < entries.size(); i++)
            {
                const Entry& entry = entries[i];
                const float ry = listY + static_cast<float>(i) * (RowH + RowGap) - m_ListScroll;

                if (ry + RowH < listY || ry > listY + ListH)
                    continue;

                const bool inside = Hover(listX, std::max(ry, listY), listW,
                    std::min(ry + RowH, listY + ListH) - std::max(ry, listY));
                const bool keyboard = static_cast<int>(i) == m_Highlight;
                const std::string reason = RejectReason(entry, context);
                const bool enabled = reason.empty();

                bool selected = false;

                if (entry.Kind == EntryKind::Craft)
                    selected = m_Cart.GetCraftChoice() == entry.Value;

                if (inside)
                {
                    m_HoverEntry = static_cast<int>(i);
                    m_Highlight = static_cast<int>(i);
                }

                Card(window, listX, ry, listW, RowH, inside ? 1.0f : keyboard ? 0.6f : selected ? 0.4f : 0.0f, enabled);

                // Icon well.
                window.DrawScreenRect(listX + 8.0f, ry + 4.0f, 68.0f, RowH - 8.0f, 8, 10, 16, 200);
                const Uint8 iconAlpha = enabled ? 255 : 110;

                switch (entry.Kind)
                {
                case EntryKind::Body:
                    Icon(window, BodySprite(entry.Value), listX + 8.0f, ry + 4.0f, 68.0f, RowH - 8.0f, iconAlpha, 2);
                    break;
                case EntryKind::Weapon:
                    Icon(window, WeaponSpriteName(*entry.Weapon), listX + 10.0f, ry + 4.0f, 64.0f, RowH - 8.0f, iconAlpha, 2);
                    break;
                case EntryKind::Crate:
                    Icon(window, "crate_supply", listX + 8.0f, ry + 4.0f, 68.0f, RowH - 8.0f, iconAlpha, 2);
                    break;
                case EntryKind::Drone:
                    Icon(window, "drone_team0", listX + 8.0f, ry + 4.0f, 68.0f, RowH - 8.0f, iconAlpha, 2);
                    break;
                case EntryKind::Craft:
                    if (entry.Value == BuyCart::AutoCraft)
                        Text(window, listX + 42.0f, ry + 14.0f, 2.0f, "A", Accent, 1);
                    else
                        Icon(window, "ship_team0", listX + 8.0f, ry + 4.0f, 68.0f, RowH - 8.0f, iconAlpha, 1);
                    break;
                }

                const SDL_Color nameColor = enabled ? (inside ? SDL_Color{ 255, 255, 255, 255 } : Bright) : Dim;
                Text(window, listX + 88.0f, ry + 8.0f, 2.0f, entry.Name, nameColor);

                // One line of the numbers that matter.
                std::string summary;

                switch (entry.Kind)
                {
                case EntryKind::Body:
                {
                    const BodyClassInfo& info = GetBodyClassInfo(static_cast<BodyClass>(entry.Value));
                    summary = "HEALTH " + std::to_string(info.MaxHealth) + "   SPEED " +
                        std::to_string(static_cast<int>(info.Speed * 100.0f)) + "%   " +
                        std::to_string(Atlas::CargoUnits(DeliveryKind::Reinforcement, entry.Value)) + " CARGO";
                    break;
                }
                case EntryKind::Weapon:
                    if (IsTool(*entry.Weapon))
                        summary = "DIG " + std::to_string(static_cast<int>(entry.Weapon->DigPower * entry.Weapon->FireRate)) +
                            "   REACH " + std::to_string(static_cast<int>(entry.Weapon->DigRange));
                    else
                        summary = "DMG " + std::to_string(static_cast<int>(WeaponDamage(*entry.Weapon))) +
                            "   RATE " + Fixed1(entry.Weapon->FireRate) + "/S   CLIP " +
                            (entry.Weapon->ClipSize > 0 ? std::to_string(entry.Weapon->ClipSize) : std::string("INF"));
                    break;
                case EntryKind::Crate:
                    summary = "HEAL 50   FULL AMMO   1 CARGO";
                    break;
                case EntryKind::Drone:
                    summary = "HEALTH " + std::to_string(World::DroneHealth) + "   2 CARGO";
                    break;
                case EntryKind::Craft:
                    if (entry.Value == BuyCart::AutoCraft)
                    {
                        summary = "NOW: " + std::string(GetCraftInfo(m_Cart.ResolveCraft()).Name);
                    }
                    else
                    {
                        const CraftInfo& info = GetCraftInfo(static_cast<CraftKind>(entry.Value));
                        summary = "HOLDS " + std::to_string(info.Capacity) + " CARGO   SPEED " +
                            std::to_string(static_cast<int>(info.Speed));
                    }
                    break;
                }

                Text(window, listX + 88.0f, ry + 26.0f, 1.0f, summary,
                    enabled ? SDL_Color{ 150, 156, 176, 255 } : SDL_Color{ 90, 94, 108, 255 });

                // Price, right; red when it would take the order over budget.
                if (entry.Kind != EntryKind::Craft || entry.Value != BuyCart::AutoCraft)
                {
                    const bool overBudget = price + entry.Cost > context.Gold;
                    const std::string cost = entry.Cost > 0 ? std::to_string(entry.Cost) : "FREE";
                    Text(window, listX + listW - 12.0f, ry + 8.0f, 2.0f, cost,
                        !enabled ? Dim : overBudget ? Bad : GoldText, 2);
                }

                if (selected)
                    Text(window, listX + listW - 12.0f, ry + 26.0f, 1.0f, "SELECTED", Good, 2);
                else if (!enabled)
                    Text(window, listX + listW - 12.0f, ry + 26.0f, 1.0f, reason, Bad, 2);
                else if (inside)
                    Text(window, listX + listW - 12.0f, ry + 26.0f, 1.0f,
                        entry.Kind == EntryKind::Craft ? "CLICK TO USE" : "CLICK TO ADD", Accent, 2);

                if (inside && m_Click)
                    Activate(entry, context);
            }

            SDL_SetRenderClipRect(window.GetRenderer(), nullptr);

            // Scrollbar.
            if (maxScroll > 0.0f)
            {
                const float trackX = listX + listW + 2.0f;
                const float thumbH = std::max(24.0f, ListH * ListH / contentH);
                const float thumbY = listY + (ListH - thumbH) * (m_ListScroll / maxScroll);
                window.DrawScreenRect(trackX, listY, 3.0f, ListH, 40, 44, 58, 200);
                window.DrawScreenRect(trackX, thumbY, 3.0f, thumbH, Accent.r, Accent.g, Accent.b, 220);
            }
        }

        DrawDetail(window, context, x + 8.0f, y + DetailY, CatW - 16.0f, CatH - DetailY - 8.0f);
    }

    void BuyMenu::DrawPresetList(Window& window, const BuyMenuContext& context,
        float x, float y, float w, float h)
    {
        (void)h;

        const float rowH = (ListH - RowGap * static_cast<float>(PresetCount - 1)) / static_cast<float>(PresetCount);

        for (int i = 0; i < PresetCount; i++)
        {
            const Preset& preset = m_Presets[static_cast<std::size_t>(i)];
            const float ry = y + static_cast<float>(i) * (rowH + RowGap);
            const float buttonsW = 112.0f;
            const bool inside = Hover(x, ry, w - buttonsW, rowH);
            const bool keyboard = i == m_Highlight;

            if (inside)
                m_Highlight = i;

            Card(window, x, ry, w, rowH, inside ? 1.0f : keyboard ? 0.6f : 0.0f, preset.Used);

            Text(window, x + 14.0f, ry + (rowH - 10.0f) * 0.5f, 2.0f, std::to_string(i + 1), Accent);

            if (preset.Used)
            {
                Text(window, x + 40.0f, ry + 6.0f, 2.0f, PresetTitle(preset), Bright);

                OrderRequest order = preset.Order;
                order.Craft = 0;

                BuyCart probe;
                probe.FromOrder(preset.Order, m_DefaultWeapon);
                probe.SetCraftChoice(preset.Craft);
                order.Craft = static_cast<std::uint8_t>(probe.ResolveCraft());

                const int cost = m_World.PriceOrder(order);
                Text(window, x + 40.0f, ry + rowH - 14.0f, 1.0f,
                    std::string(GetCraftInfo(probe.ResolveCraft()).Name) + "   " + std::to_string(cost) + " GOLD",
                    cost > context.Gold ? Bad : SDL_Color{ 150, 156, 176, 255 });
            }
            else
            {
                Text(window, x + 40.0f, ry + (rowH - 7.0f) * 0.5f, 1.0f,
                    "EMPTY SLOT. BUILD AN ORDER, THEN SAVE IT HERE.", Dim);
            }

            if (inside && m_Click && preset.Used)
                ApplyPreset(i);

            // SAVE / X on the right.
            const float bx = x + w - buttonsW;

            if (Button(window, bx, ry + (rowH - 22.0f) * 0.5f, 72.0f, 22.0f, "SAVE",
                m_MouseX, m_MouseY, m_Click, !m_Cart.IsEmpty(), 1.0f))
            {
                StorePreset(i);
            }

            if (preset.Used &&
                RemoveButton(window, bx + 80.0f, ry + (rowH - 22.0f) * 0.5f, 22.0f, m_MouseX, m_MouseY, m_Click))
            {
                m_Presets[static_cast<std::size_t>(i)] = Preset{};
                SavePresets();
                Toast("PRESET " + std::to_string(i + 1) + " DELETED");
            }
        }
    }

    void BuyMenu::DrawDetail(Window& window, const BuyMenuContext& context,
        float x, float y, float w, float h)
    {
        (void)context;

        window.DrawScreenRect(x, y, w, h, 8, 10, 16, 210);
        window.DrawScreenRect(x, y, w, 1.0f, 120, 132, 170, 80);

        if (m_Tab == Tab::Presets)
        {
            Text(window, x + 14.0f, y + 14.0f, 2.0f, "PRESETS", Accent);

            const char* lines[] =
            {
                "SAVE ORDERS YOU USE OFTEN AND CALL THEM IN WITH ONE KEY.",
                "",
                "1-8          LOAD A PRESET INTO THE ORDER",
                "SHIFT+1-8    SAVE THE CURRENT ORDER TO A SLOT",
                "F            BUY",
                "",
                "PRESETS ARE KEPT BETWEEN GAMES.",
            };

            float ly = y + 40.0f;

            for (const char* line : lines)
            {
                Text(window, x + 14.0f, ly, 1.0f, line, SDL_Color{ 186, 192, 208, 255 });
                ly += 13.0f;
            }
            return;
        }

        const std::vector<Entry>& entries = EntriesFor(m_Tab);

        if (m_Highlight < 0 || m_Highlight >= static_cast<int>(entries.size()))
            return;

        const Entry& entry = entries[static_cast<std::size_t>(m_Highlight)];

        // Big picture, left.
        const float artW = 150.0f;
        const float artH = h - 20.0f;
        window.DrawGradientScreen(x + 10.0f, y + 10.0f, artW, artH, { 30, 34, 50, 255 }, { 14, 16, 24, 255 });
        window.DrawGlowScreen(x + 10.0f + artW * 0.5f, y + 10.0f + artH * 0.55f, 60.0f, 255, 196, 120, 40);

        switch (entry.Kind)
        {
        case EntryKind::Body:
            Icon(window, BodySprite(entry.Value), x + 10.0f, y + 10.0f, artW, artH, 255, 5);
            break;
        case EntryKind::Weapon:
            Icon(window, WeaponSpriteName(*entry.Weapon), x + 14.0f, y + 10.0f, artW - 8.0f, artH, 255, 5);
            break;
        case EntryKind::Crate:
            Icon(window, "crate_supply", x + 10.0f, y + 10.0f, artW, artH, 255, 5);
            break;
        case EntryKind::Drone:
            Icon(window, "drone_team0", x + 10.0f, y + 10.0f, artW, artH, 255, 5);
            break;
        case EntryKind::Craft:
            Icon(window, "ship_team0", x + 10.0f, y + 10.0f, artW, artH, 255, 3);
            break;
        }

        const float tx = x + artW + 26.0f;
        const float tw = w - artW - 40.0f;

        Text(window, tx, y + 14.0f, 2.0f, entry.Name, Bright);

        float ly = y + 32.0f;

        for (const std::string& line : Wrap(entry.Blurb, 1.0f, tw))
        {
            Text(window, tx, ly, 1.0f, line, SDL_Color{ 186, 192, 208, 255 });
            ly += 11.0f;
        }

        ly = std::max(ly + 8.0f, y + 66.0f);
        const float step = 16.0f;

        switch (entry.Kind)
        {
        case EntryKind::Body:
        {
            const BodyClassInfo& info = GetBodyClassInfo(static_cast<BodyClass>(entry.Value));
            StatBar(window, tx, ly, tw, "HEALTH", info.MaxHealth / 200.0f, std::to_string(info.MaxHealth));
            StatBar(window, tx, ly + step, tw, "SPEED", info.Speed / 1.3f, std::to_string(static_cast<int>(info.Speed * 100.0f)) + "%");
            StatBar(window, tx, ly + step * 2.0f, tw, "JETPACK", info.Jet / 1.25f, std::to_string(static_cast<int>(info.Jet * 100.0f)) + "%");
            StatBar(window, tx, ly + step * 3.0f, tw, "ARMOR", (1.0f - info.Armor) / 0.3f * 0.8f + 0.2f,
                std::to_string(static_cast<int>(std::lround((1.0f - info.Armor) * 100.0f))) + "%");
            Text(window, tx, ly + step * 4.0f + 4.0f, 1.0f,
                "LANDS WITH AN " + Upper(m_DefaultWeapon ? m_DefaultWeapon->Name : "SMG") + " UNLESS YOU ARM IT.", Dim);
            break;
        }
        case EntryKind::Weapon:
        {
            const WeaponDef& def = *entry.Weapon;

            if (IsTool(def))
            {
                StatBar(window, tx, ly, tw, "DIG SPEED", def.DigPower * def.FireRate / m_MaxDig,
                    std::to_string(static_cast<int>(def.DigPower * def.FireRate)));
                StatBar(window, tx, ly + step, tw, "REACH", def.DigRange / m_MaxReach, std::to_string(static_cast<int>(def.DigRange)));
                StatBar(window, tx, ly + step * 2.0f, tw, "BITE", def.DigRadius / 15.0f, std::to_string(static_cast<int>(def.DigRadius)));
                Text(window, tx, ly + step * 3.0f, 1.0f, "ROCK", Dim);
                Text(window, tx + tw, ly + step * 3.0f, 1.0f,
                    def.MaxDigStrength >= 8.0f ? (def.HardMaterialCost < 1.0f ? "FAST" : "YES") : "NO",
                    def.MaxDigStrength >= 8.0f ? Good : Bad, 2);
            }
            else
            {
                StatBar(window, tx, ly, tw, "DAMAGE", WeaponDamage(def) / m_MaxDamage,
                    std::to_string(static_cast<int>(WeaponDamage(def))));
                StatBar(window, tx, ly + step, tw, "FIRE RATE", def.FireRate / m_MaxRate, Fixed1(def.FireRate) + "/S");
                StatBar(window, tx, ly + step * 2.0f, tw, "VELOCITY", def.MuzzleVelocity / m_MaxVelocity,
                    std::to_string(static_cast<int>(def.MuzzleVelocity)));
                StatBar(window, tx, ly + step * 3.0f, tw, "ACCURACY", 1.0f / (1.0f + def.SpreadDegrees * 0.35f),
                    std::to_string(static_cast<int>(std::lround(100.0f / (1.0f + def.SpreadDegrees * 0.35f)))) + "%");
                Text(window, tx, ly + step * 4.0f + 4.0f, 1.0f,
                    "CLIP " + (def.ClipSize > 0 ? std::to_string(def.ClipSize) : std::string("INFINITE")) +
                    "    RELOAD " + Fixed1(def.ReloadTime) + "S" + (def.LaserSight ? "    LASER SIGHT" : ""), Dim);
            }
            break;
        }
        case EntryKind::Craft:
        {
            const CraftKind kind = entry.Value == BuyCart::AutoCraft ? m_Cart.ResolveCraft()
                : static_cast<CraftKind>(entry.Value);
            const CraftInfo& info = GetCraftInfo(kind);
            const int biggest = GetCraftInfo(static_cast<CraftKind>(static_cast<int>(CraftKind::Count) - 1)).Capacity;

            StatBar(window, tx, ly, tw, "CAPACITY", static_cast<float>(info.Capacity) / static_cast<float>(biggest),
                std::to_string(info.Capacity));
            StatBar(window, tx, ly + step, tw, "SPEED", info.Speed / 640.0f, std::to_string(static_cast<int>(info.Speed)));
            Text(window, tx, ly + step * 2.0f + 4.0f, 1.0f, "SCOUT 2, RIFLEMAN 3, HEAVY 4 CARGO.", Dim);
            Text(window, tx, ly + step * 3.0f, 1.0f, "DRONE 2. CRATE OR LOOSE WEAPON 1.", Dim);
            Text(window, tx, ly + step * 4.0f - 4.0f, 1.0f, "WEAPONS CARRIED BY A BODY RIDE FREE.", Dim);
            break;
        }
        case EntryKind::Crate:
        case EntryKind::Drone:
            Text(window, tx, ly, 1.0f, "DROPS WITH THE REST OF THE ORDER.", Dim);
            break;
        }
    }

    void BuyMenu::DrawOrder(Window& window, const BuyMenuContext& context)
    {
        const float slide = (1.0f - m_OpenAnim) * (1.0f - m_OpenAnim) * 60.0f;
        const float x = OrdX + slide;
        const float y = OrdY;

        PanelBox(window, x, y, OrdW, OrdH);
        Text(window, x + 14.0f, y + 13.0f, 3.0f, "YOUR ORDER", Bright);

        if (Button(window, x + OrdW - 74.0f, y + 10.0f, 62.0f, 22.0f, "CLEAR",
            m_MouseX, m_MouseY, m_Click, !m_Cart.IsEmpty(), 1.0f))
        {
            m_Cart.Clear();
        }

        // --- Craft card: click to cycle ---
        {
            const float cx = x + 8.0f;
            const float cy = y + CraftCardY;
            const float cw = OrdW - 16.0f;
            const bool hover = Hover(cx, cy, cw, CraftCardH);
            const CraftKind craft = m_Cart.ResolveCraft();
            const CraftInfo& info = GetCraftInfo(craft);
            const int units = CargoUnits();
            const bool over = units > info.Capacity;

            Card(window, cx, cy, cw, CraftCardH, hover ? 1.0f : 0.0f);
            window.DrawScreenRect(cx + 8.0f, cy + 6.0f, 84.0f, CraftCardH - 12.0f, 8, 10, 16, 200);
            Icon(window, "ship_team0", cx + 8.0f, cy + 6.0f, 84.0f, CraftCardH - 12.0f, 255, 1);

            Text(window, cx + 104.0f, cy + 8.0f, 2.0f, info.Name, Bright);

            const std::string mode = m_Cart.GetCraftChoice() == BuyCart::AutoCraft ? "AUTO" : "LOCKED";
            Text(window, cx + 104.0f + PixelFont::Measure(info.Name, 2.0f) + 10.0f, cy + 10.0f, 1.0f, mode,
                m_Cart.GetCraftChoice() == BuyCart::AutoCraft ? Good : Accent);

            Text(window, cx + cw - 10.0f, cy + 8.0f, 2.0f, info.Cost > 0 ? std::to_string(info.Cost) : "FREE", GoldText, 2);

            // Capacity bar, one notch per cargo unit.
            const float barX = cx + 104.0f;
            const float barY = cy + 30.0f;
            const float barW = cw - 114.0f;
            const float notch = barW / static_cast<float>(info.Capacity);

            for (int u = 0; u < info.Capacity; u++)
            {
                const bool filled = u < units;
                window.DrawScreenRect(barX + static_cast<float>(u) * notch, barY,
                    std::max(1.0f, notch - 2.0f), 8.0f,
                    filled ? (over ? Bad.r : Accent.r) : 44,
                    filled ? (over ? Bad.g : Accent.g) : 48,
                    filled ? (over ? Bad.b : Accent.b) : 62, 255);
            }

            Text(window, barX, cy + 44.0f, 1.0f,
                "CARGO " + std::to_string(units) + "/" + std::to_string(info.Capacity) +
                (hover ? "   CLICK TO CHANGE (C)" : ""),
                over ? Bad : SDL_Color{ 150, 156, 176, 255 });

            if (hover && m_Click)
            {
                const int next = m_Cart.GetCraftChoice() + 1;
                m_Cart.SetCraftChoice(next >= static_cast<int>(CraftKind::Count) ? BuyCart::AutoCraft : next);
                Audio::Play(Sfx::UiClick, 0.4f);
            }
        }

        // --- Cargo list ---
        const float lx = x + 8.0f;
        const float ly = y + CargoY;
        const float lw = OrdW - 16.0f;

        window.DrawScreenRect(lx, ly, lw, CargoH, 6, 8, 12, 160);

        if (m_Cart.IsEmpty())
        {
            Text(window, lx + lw * 0.5f, ly + CargoH * 0.5f - 22.0f, 2.0f, "NOTHING ORDERED", Dim, 1);
            Text(window, lx + lw * 0.5f, ly + CargoH * 0.5f, 1.0f, "ADD A BODY, THEN WEAPONS TO ARM IT.", Dim, 1);
            Text(window, lx + lw * 0.5f, ly + CargoH * 0.5f + 12.0f, 1.0f, "OR PRESS 1-8 FOR A SAVED PRESET.", Dim, 1);
        }
        else
        {
            // Lay the rows out first so scrolling knows the height.
            struct Row
            {
                int Kind;   // 0 body, 1 body weapon, 2 included default, 3 loose header, 4 loose weapon, 5 crates, 6 drones
                int Body;
                int Index;
                float H;
            };

            std::vector<Row> rows;
            const std::vector<CartBody>& bodies = m_Cart.GetBodies();

            for (std::size_t b = 0; b < bodies.size(); b++)
            {
                rows.push_back({ 0, static_cast<int>(b), 0, BodyRowH });

                if (bodies[b].Weapons.empty())
                    rows.push_back({ 2, static_cast<int>(b), 0, ItemRowH });

                for (std::size_t w = 0; w < bodies[b].Weapons.size(); w++)
                    rows.push_back({ 1, static_cast<int>(b), static_cast<int>(w), ItemRowH });
            }

            const bool anyLoose = !m_Cart.GetLooseWeapons().empty() ||
                m_Cart.GetSupplyCrateCount() > 0 || m_Cart.GetDroneCount() > 0;

            if (anyLoose || (!bodies.empty() && m_Cart.GetActiveBodyIndex() < 0))
                rows.push_back({ 3, -1, 0, HeaderRowH });

            for (std::size_t w = 0; w < m_Cart.GetLooseWeapons().size(); w++)
                rows.push_back({ 4, -1, static_cast<int>(w), ItemRowH });

            if (m_Cart.GetSupplyCrateCount() > 0)
                rows.push_back({ 5, -1, 0, ItemRowH });

            if (m_Cart.GetDroneCount() > 0)
                rows.push_back({ 6, -1, 0, ItemRowH });

            float contentH = 4.0f;

            for (const Row& row : rows)
                contentH += row.H + 2.0f;

            const float maxScroll = std::max(0.0f, contentH - CargoH);

            if (m_Wheel != 0 && Hover(lx, ly, lw, CargoH))
                m_OrderScroll -= static_cast<float>(m_Wheel) * 24.0f;

            m_OrderScroll = std::clamp(m_OrderScroll, 0.0f, maxScroll);

            const SDL_Rect clip = { static_cast<int>(lx), static_cast<int>(ly),
                static_cast<int>(lw), static_cast<int>(CargoH) };
            SDL_SetRenderClipRect(window.GetRenderer(), &clip);

            // Removals wait until the walk is done, so indices hold.
            int removeKind = -1;
            int removeBody = -1;
            int removeIndex = -1;
            int selectBody = -2;

            float ry = ly + 4.0f - m_OrderScroll;

            for (const Row& row : rows)
            {
                const float rh = row.H;
                const bool visible = ry + rh > ly && ry < ly + CargoH;
                const bool inside = visible && Hover(lx, std::max(ry, ly), lw,
                    std::min(ry + rh, ly + CargoH) - std::max(ry, ly));

                if (!visible)
                {
                    ry += rh + 2.0f;
                    continue;
                }

                const float xButton = lx + lw - 24.0f;

                switch (row.Kind)
                {
                case 0:
                {
                    const CartBody& body = bodies[static_cast<std::size_t>(row.Body)];
                    const bool active = row.Body == m_Cart.GetActiveBodyIndex();
                    const BodyClassInfo& info = GetBodyClassInfo(static_cast<BodyClass>(body.Class));

                    Card(window, lx + 2.0f, ry, lw - 4.0f, rh, active ? 1.0f : inside ? 0.4f : 0.0f);
                    Icon(window, BodySprite(body.Class), lx + 8.0f, ry + 2.0f, 26.0f, rh - 4.0f, 255, 2);
                    Text(window, lx + 40.0f, ry + (rh - 10.0f) * 0.5f, 2.0f, info.Name, Bright);

                    if (active)
                        Text(window, lx + 48.0f + PixelFont::Measure(info.Name, 2.0f), ry + (rh - 7.0f) * 0.5f,
                            1.0f, "ARMING", Accent);
                    else if (inside)
                        Text(window, lx + 48.0f + PixelFont::Measure(info.Name, 2.0f), ry + (rh - 7.0f) * 0.5f,
                            1.0f, "CLICK TO ARM", Dim);

                    Text(window, xButton - 8.0f, ry + (rh - 10.0f) * 0.5f, 2.0f, std::to_string(info.Cost), GoldText, 2);

                    if (RemoveButton(window, xButton, ry + (rh - 18.0f) * 0.5f, 18.0f, m_MouseX, m_MouseY, m_Click) ||
                        (inside && m_RightClick))
                    {
                        removeKind = 0;
                        removeBody = row.Body;
                    }
                    else if (inside && m_Click)
                    {
                        selectBody = active ? -1 : row.Body;
                    }
                    break;
                }

                case 1:
                case 2:
                case 4:
                {
                    const WeaponDef* def = nullptr;
                    bool free = false;

                    if (row.Kind == 1)
                    {
                        const CartBody& body = bodies[static_cast<std::size_t>(row.Body)];
                        def = body.Weapons[static_cast<std::size_t>(row.Index)];
                        free = row.Index == 0 && def == m_DefaultWeapon;
                    }
                    else if (row.Kind == 2)
                    {
                        def = m_DefaultWeapon;
                        free = true;
                    }
                    else
                    {
                        def = m_Cart.GetLooseWeapons()[static_cast<std::size_t>(row.Index)];
                    }

                    const float indent = row.Kind == 4 ? 8.0f : 22.0f;

                    if (inside && row.Kind != 2)
                        window.DrawScreenRect(lx + indent, ry, lw - indent - 2.0f, rh, 255, 255, 255, 12);

                    window.DrawScreenRect(lx + indent - 8.0f, ry + rh * 0.5f, 6.0f, 1.0f, 90, 96, 116, 255);

                    if (def)
                        Icon(window, WeaponSpriteName(*def), lx + indent, ry + 2.0f, 44.0f, rh - 4.0f,
                            row.Kind == 2 ? 120 : 255, 1);

                    const std::string name = def ? Upper(def->Name) : "?";
                    Text(window, lx + indent + 50.0f, ry + (rh - 7.0f) * 0.5f, 1.0f, name,
                        row.Kind == 2 ? Dim : SDL_Color{ 214, 218, 230, 255 });

                    if (row.Kind == 2)
                    {
                        Text(window, xButton + 18.0f, ry + (rh - 7.0f) * 0.5f, 1.0f, "INCLUDED", Dim, 2);
                    }
                    else
                    {
                        Text(window, xButton - 8.0f, ry + (rh - 7.0f) * 0.5f, 1.0f,
                            free ? "FREE" : std::to_string(def ? def->Cost : 0), GoldText, 2);

                        if (RemoveButton(window, xButton + 2.0f, ry + (rh - 16.0f) * 0.5f, 16.0f, m_MouseX, m_MouseY, m_Click) ||
                            (inside && m_RightClick))
                        {
                            removeKind = row.Kind;
                            removeBody = row.Body;
                            removeIndex = row.Index;
                        }
                    }
                    break;
                }

                case 3:
                {
                    const bool target = m_Cart.GetActiveBodyIndex() < 0;
                    Text(window, lx + 8.0f, ry + (rh - 7.0f) * 0.5f, 1.0f, "LOOSE CARGO",
                        target ? Accent : Dim);
                    Text(window, lx + lw - 8.0f, ry + (rh - 7.0f) * 0.5f, 1.0f,
                        target ? "NEW WEAPONS GO HERE" : inside ? "CLICK TO ADD WEAPONS HERE" : "", target ? Accent : Dim, 2);

                    if (inside && m_Click)
                        selectBody = -1;
                    break;
                }

                case 5:
                case 6:
                {
                    const bool crates = row.Kind == 5;
                    const int count = crates ? m_Cart.GetSupplyCrateCount() : m_Cart.GetDroneCount();
                    const int each = crates ? World::SupplyCrateCost : World::DroneCost;

                    if (inside)
                        window.DrawScreenRect(lx + 8.0f, ry, lw - 10.0f, rh, 255, 255, 255, 12);

                    Icon(window, crates ? "crate_supply" : "drone_team0", lx + 8.0f, ry + 2.0f, 44.0f, rh - 4.0f, 255, 1);
                    Text(window, lx + 58.0f, ry + (rh - 7.0f) * 0.5f, 1.0f,
                        std::string(crates ? "SUPPLY CRATE" : "GUN DRONE") + (count > 1 ? "  X" + std::to_string(count) : ""),
                        SDL_Color{ 214, 218, 230, 255 });
                    Text(window, xButton - 8.0f, ry + (rh - 7.0f) * 0.5f, 1.0f, std::to_string(each * count), GoldText, 2);

                    if (RemoveButton(window, xButton + 2.0f, ry + (rh - 16.0f) * 0.5f, 16.0f, m_MouseX, m_MouseY, m_Click) ||
                        (inside && m_RightClick))
                    {
                        removeKind = row.Kind;
                    }
                    break;
                }
                }

                ry += rh + 2.0f;
            }

            SDL_SetRenderClipRect(window.GetRenderer(), nullptr);

            if (maxScroll > 0.0f)
            {
                const float thumbH = std::max(20.0f, CargoH * CargoH / contentH);
                const float thumbY = ly + (CargoH - thumbH) * (m_OrderScroll / maxScroll);
                window.DrawScreenRect(lx + lw - 3.0f, thumbY, 3.0f, thumbH, Accent.r, Accent.g, Accent.b, 200);
            }

            if (removeKind >= 0)
            {
                switch (removeKind)
                {
                case 0: m_Cart.RemoveBody(removeBody); break;
                case 1: m_Cart.RemoveBodyWeapon(removeBody, removeIndex); break;
                case 4: m_Cart.RemoveLooseWeapon(removeIndex); break;
                case 5: m_Cart.RemoveSupplyCrate(); break;
                case 6: m_Cart.RemoveDrone(); break;
                default: break;
                }

                Audio::Play(Sfx::UiClick, 0.35f);
            }
            else if (selectBody >= 0)
            {
                m_Cart.SelectBody(selectBody);
                Audio::Play(Sfx::UiClick, 0.35f);
            }
            else if (selectBody == -1)
            {
                m_Cart.DeselectBody();
                Audio::Play(Sfx::UiClick, 0.35f);
            }
        }

        // --- Footer: total, landing zone, buy ---
        const float fy = y + OrdH - FooterH;
        const int price = PriceCart();
        const std::string blocker = SendBlocker(context);
        const bool canSend = blocker.empty();

        window.DrawScreenRect(x + 8.0f, fy - 2.0f, OrdW - 16.0f, 1.0f, 120, 132, 170, 80);

        Text(window, x + 14.0f, fy + 8.0f, 2.0f, "TOTAL", Bright);
        Text(window, x + OrdW - 14.0f, fy + 4.0f, 3.0f, std::to_string(price),
            price > context.Gold ? Bad : GoldText, 2);

        Text(window, x + 14.0f, fy + 26.0f, 1.0f,
            canSend ? "LEAVES YOU " + std::to_string(context.Gold - price) + " GOLD" : blocker,
            canSend ? SDL_Color{ 150, 156, 176, 255 } : Bad);

        // Landing zone: on me / pick a spot.
        {
            const float segY = fy + 40.0f;
            const float segW = (OrdW - 16.0f - 4.0f) * 0.5f;
            const float segH = 24.0f;
            const char* labels[2] = { "DROP ON ME", "PICK LZ (L)" };

            for (int i = 0; i < 2; i++)
            {
                const float sx = x + 8.0f + static_cast<float>(i) * (segW + 4.0f);
                const bool on = (i == 1) == m_ZoneOnSend;
                const bool hover = Hover(sx, segY, segW, segH);

                if (on)
                {
                    window.DrawGradientScreen(sx, segY, segW, segH, { 60, 54, 40, 255 }, { 34, 30, 24, 255 });
                    window.DrawScreenRect(sx, segY + segH - 2.0f, segW, 2.0f, Accent.r, Accent.g, Accent.b, 255);
                }
                else
                {
                    Card(window, sx, segY, segW, segH, hover ? 0.5f : 0.0f);
                }

                Text(window, sx + segW * 0.5f, segY + (segH - 7.0f) * 0.5f, 1.0f, labels[i],
                    on ? Accent : hover ? Bright : Dim, 1);

                if (hover && m_Click)
                {
                    m_ZoneOnSend = i == 1;
                    Audio::Play(Sfx::UiClick, 0.35f);
                }
            }
        }

        const std::string buyLabel = m_ZoneOnSend ? "BUY + PICK LZ   F" : "BUY   F";

        if (Button(window, x + 8.0f, fy + 72.0f, OrdW - 16.0f, 38.0f, buyLabel,
            m_MouseX, m_MouseY, m_Click, canSend, 3.0f, true, context.Time))
        {
            if (m_ZoneOnSend)
            {
                m_PickingZone = true;
                m_Open = false;
            }
            else
            {
                Send(-1);
            }
        }
        else if (!canSend && m_Click && Hover(x + 8.0f, fy + 72.0f, OrdW - 16.0f, 38.0f))
        {
            Toast(blocker, true);
        }

        const float smallW = (OrdW - 16.0f - 4.0f) * 0.5f;

        if (Button(window, x + 8.0f, fy + 116.0f, smallW, 22.0f, "UNDO  BKSP",
            m_MouseX, m_MouseY, m_Click, !m_Cart.IsEmpty(), 1.0f))
        {
            m_Cart.Undo();
        }

        if (Button(window, x + 12.0f + smallW, fy + 116.0f, smallW, 22.0f, "SAVE PRESET",
            m_MouseX, m_MouseY, m_Click, !m_Cart.IsEmpty(), 1.0f))
        {
            // First empty slot, else the last one.
            int slot = PresetCount - 1;

            for (int i = 0; i < PresetCount; i++)
            {
                if (!m_Presets[static_cast<std::size_t>(i)].Used)
                {
                    slot = i;
                    break;
                }
            }

            StorePreset(slot);
        }
    }

    void BuyMenu::DrawZonePicker(Window& window, const BuyMenuContext& context)
    {
        const Terrain& terrain = m_World.GetTerrain();
        const float worldX = std::clamp(window.ScreenToWorldX(m_MouseX), 40.0f,
            static_cast<float>(terrain.GetWidth()) - 40.0f);
        const float groundY = window.WorldToScreenY(
            static_cast<float>(terrain.GetSurfaceY(static_cast<int>(worldX))));
        const float sx = window.WorldToScreenX(worldX);
        const float pulse = 0.5f + 0.5f * std::sin(context.Time * 6.0f);

        // A beam from the sky to the ground where the craft will hover.
        const float top = std::max(0.0f, window.WorldToScreenY(24.0f));
        const float bottom = std::clamp(groundY, top, ViewH);

        for (float yy = top; yy < bottom; yy += 10.0f)
            window.DrawScreenRect(sx - 1.0f, yy, 2.0f, 6.0f, Accent.r, Accent.g, Accent.b, 170);

        if (groundY > 0.0f && groundY < ViewH)
        {
            window.DrawGlowScreen(sx, groundY, 26.0f + pulse * 8.0f, Accent.r, Accent.g, Accent.b, 120);
            window.DrawScreenRect(sx - 14.0f, groundY - 1.0f, 28.0f, 2.0f, Accent.r, Accent.g, Accent.b, 255);
            Text(window, sx, groundY - 22.0f, 2.0f, "LZ", Accent, 1);
        }

        const float boxW = 420.0f;
        window.DrawScreenRect((ViewW - boxW) * 0.5f, 84.0f, boxW, 46.0f, 10, 12, 18, 220);
        window.DrawScreenRect((ViewW - boxW) * 0.5f, 84.0f, boxW, 2.0f, Accent.r, Accent.g, Accent.b, 255);
        Text(window, ViewW * 0.5f, 94.0f, 2.0f, "CLICK TO SET THE LANDING ZONE", Bright, 1);
        Text(window, ViewW * 0.5f, 114.0f, 1.0f, "RIGHT CLICK OR TAB: BACK TO THE ORDER   ESC: CANCEL", Dim, 1);

        if (m_Click)
        {
            if (SendBlocker(context).empty())
                Send(static_cast<int>(worldX));
            else
            {
                Toast(SendBlocker(context), true);
                m_PickingZone = false;
                m_Open = true;
            }
        }
        else if (m_RightClick)
        {
            m_PickingZone = false;
            m_Open = true;
        }
    }

    void BuyMenu::DrawInbound(Window& window, const BuyMenuContext& context) const
    {
        const Terrain& terrain = m_World.GetTerrain();
        const float pulse = 0.5f + 0.5f * std::sin(context.Time * 5.0f);

        for (const Craft& craft : m_World.GetDeliveries().GetCrafts())
        {
            if (craft.Team != 0 || craft.OwnerPlayer != context.LocalPlayer ||
                craft.Phase == CraftPhase::FlyOut)
                continue;

            const float sx = window.WorldToScreenX(craft.TargetX);
            const float gy = window.WorldToScreenY(
                static_cast<float>(terrain.GetSurfaceY(static_cast<int>(craft.TargetX))));

            if (sx < -40.0f || sx > ViewW + 40.0f)
            {
                // Off screen: an arrow at the edge pointing to it.
                const bool right = sx > ViewW;
                const float ex = right ? ViewW - 30.0f : 30.0f;
                const float ey = std::clamp(gy - 30.0f, 140.0f, ViewH - 140.0f);
                Text(window, ex, ey, 2.0f, right ? "LZ >" : "< LZ", Accent, right ? 2 : 0);
                continue;
            }

            if (gy < -20.0f || gy > ViewH + 20.0f)
                continue;

            window.DrawGlowScreen(sx, gy, 18.0f + pulse * 6.0f, Accent.r, Accent.g, Accent.b, 90);
            window.DrawScreenRect(sx - 10.0f, gy - 1.0f, 20.0f, 2.0f, Accent.r, Accent.g, Accent.b, 220);

            for (int i = 0; i < 3; i++)
            {
                const float cy = gy - 18.0f - static_cast<float>(i) * 6.0f - pulse * 4.0f;
                window.DrawScreenRect(sx - 3.0f, cy, 6.0f, 2.0f, Accent.r, Accent.g, Accent.b,
                    static_cast<Uint8>(220 - i * 60));
            }

            Text(window, sx, gy - 44.0f, 1.0f, GetCraftInfo(craft.Kind).Name, Accent, 1);
        }
    }
}

#pragma once

#include <map>
#include <string>
#include <vector>

namespace Atlas
{
    // Minimal INI reader: [Section] headers, Key = Value lines, and
    // comments starting with ; or #.
    class IniFile
    {
    public:
        bool Load(const std::string& filename);

        bool HasSection(const std::string& section) const;

        std::string GetString(
            const std::string& section,
            const std::string& key,
            const std::string& fallback) const;

        float GetFloat(
            const std::string& section,
            const std::string& key,
            float fallback) const;

        int GetInt(
            const std::string& section,
            const std::string& key,
            int fallback) const;

        // Section names in file order.
        const std::vector<std::string>& GetSectionNames() const;

    private:
        std::map<std::string, std::map<std::string, std::string>> m_Sections;
        std::vector<std::string> m_SectionOrder;
    };
}

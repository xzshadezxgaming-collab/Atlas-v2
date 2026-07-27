#include "IniFile.h"

#include <cstdlib>
#include <fstream>

namespace Atlas
{
    namespace
    {
        std::string Trim(const std::string& text)
        {
            const std::size_t first = text.find_first_not_of(" \t\r\n");

            if (first == std::string::npos)
                return "";

            const std::size_t last = text.find_last_not_of(" \t\r\n");

            return text.substr(first, last - first + 1);
        }
    }

    bool IniFile::Load(const std::string& filename)
    {
        std::ifstream file(filename);

        if (!file.is_open())
            return false;

        m_Sections.clear();
        m_SectionOrder.clear();

        std::string currentSection;
        std::string line;

        while (std::getline(file, line))
        {
            line = Trim(line);

            if (line.empty() || line[0] == ';' || line[0] == '#')
                continue;

            if (line.front() == '[' && line.back() == ']')
            {
                currentSection = Trim(line.substr(1, line.size() - 2));

                if (m_Sections.find(currentSection) == m_Sections.end())
                {
                    m_Sections[currentSection] = {};
                    m_SectionOrder.push_back(currentSection);
                }

                continue;
            }

            const std::size_t equals = line.find('=');

            if (equals == std::string::npos)
                continue;

            const std::string key = Trim(line.substr(0, equals));
            const std::string value = Trim(line.substr(equals + 1));

            if (!key.empty())
                m_Sections[currentSection][key] = value;
        }

        return true;
    }

    bool IniFile::HasSection(const std::string& section) const
    {
        return m_Sections.find(section) != m_Sections.end();
    }

    std::string IniFile::GetString(
        const std::string& section,
        const std::string& key,
        const std::string& fallback) const
    {
        const auto sectionIt = m_Sections.find(section);

        if (sectionIt == m_Sections.end())
            return fallback;

        const auto keyIt = sectionIt->second.find(key);

        if (keyIt == sectionIt->second.end())
            return fallback;

        return keyIt->second;
    }

    float IniFile::GetFloat(
        const std::string& section,
        const std::string& key,
        float fallback) const
    {
        const std::string value = GetString(section, key, "");

        if (value.empty())
            return fallback;

        return static_cast<float>(std::atof(value.c_str()));
    }

    int IniFile::GetInt(
        const std::string& section,
        const std::string& key,
        int fallback) const
    {
        const std::string value = GetString(section, key, "");

        if (value.empty())
            return fallback;

        return std::atoi(value.c_str());
    }

    const std::vector<std::string>& IniFile::GetSectionNames() const
    {
        return m_SectionOrder;
    }
}

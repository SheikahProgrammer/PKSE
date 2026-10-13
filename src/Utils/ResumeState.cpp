#include "Utils/ResumeState.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <utility>
#include <vector>

#include "Globals.h"
#include "Utils/Logger.h"

namespace Utils
{
    namespace Resume
    {
        // A section per save opened; past this many the least recently used go.
        static constexpr size_t MAX_SECTIONS = 48;

        static std::vector<std::pair<std::string, Fields>> s_sections; // most recent first
        static bool s_loaded = false;

        static std::string resumePath() { return BASE_SAVE_DIRECTORY + "/resume.cfg"; }

        static void load()
        {
            if (s_loaded)
                return;
            s_loaded = true;
            FILE *f = fopen(resumePath().c_str(), "r");
            if (!f)
                f = fopen((resumePath() + ".tmp").c_str(), "r"); // interrupted between remove and rename
            if (!f)
                return;
            // Long enough for an absolute SD path as a section name.
            char line[1024];
            Fields *current = nullptr;
            while (fgets(line, sizeof(line), f))
            {
                char *nl = strpbrk(line, "\r\n");
                if (nl)
                    *nl = '\0';
                const size_t length = strlen(line);
                if (length >= 2 && line[0] == '[' && line[length - 1] == ']')
                {
                    s_sections.emplace_back(std::string(line + 1, length - 2), Fields{});
                    current = &s_sections.back().second;
                    continue;
                }
                char *eq = strchr(line, '=');
                if (!eq || !current)
                    continue;
                *eq = '\0';
                (*current)[line] = eq + 1;
            }
            fclose(f);
        }

        static void write()
        {
            mkdir(BASE_SAVE_DIRECTORY.c_str(), 0777);
            // Temp file + rename: a crash mid-write keeps the previous file.
            const std::string tempPath = resumePath() + ".tmp";
            FILE *f = fopen(tempPath.c_str(), "w");
            if (!f)
            {
                logErrorToFile("Failed to write resume file", resumePath().c_str());
                return;
            }
            for (const auto &[section, fields] : s_sections)
            {
                fprintf(f, "[%s]\n", section.c_str());
                for (const auto &[key, value] : fields)
                    fprintf(f, "%s=%s\n", key.c_str(), value.c_str());
            }
            // FAT cannot rename over an existing file, so the old one goes first; until the rename
            // lands, load() reads the .tmp copy.
            if (fclose(f) == 0 && std::rename(tempPath.c_str(), resumePath().c_str()) != 0)
            {
                std::remove(resumePath().c_str());
                std::rename(tempPath.c_str(), resumePath().c_str());
            }
        }

        Fields get(const std::string &section)
        {
            load();
            for (const auto &[name, fields] : s_sections)
                if (name == section)
                    return fields;
            return {};
        }

        void put(const std::string &section, const Fields &fields)
        {
            load();
            for (auto it = s_sections.begin(); it != s_sections.end(); ++it)
            {
                if (it->first == section)
                {
                    s_sections.erase(it);
                    break;
                }
            }
            s_sections.insert(s_sections.begin(), {section, fields});
            if (s_sections.size() > MAX_SECTIONS)
                s_sections.resize(MAX_SECTIONS);
            write();
        }

        int intOf(const Fields &fields, const std::string &key, int fallback)
        {
            const auto it = fields.find(key);
            if (it == fields.end() || it->second.empty())
                return fallback;
            char *end = nullptr;
            const long value = strtol(it->second.c_str(), &end, 10);
            return (end && *end == '\0') ? static_cast<int>(value) : fallback;
        }

        std::string textOf(const Fields &fields, const std::string &key)
        {
            const auto it = fields.find(key);
            return it == fields.end() ? std::string() : it->second;
        }
    }
}

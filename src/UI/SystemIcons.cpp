#include "UI/SystemIcons.h"

#include <Libs/stb_image.h> // implementation lives in SpriteManager.cpp; we only need the decls

#include <cstdlib>
#include <map>
#include <vector>

#include "Utils/Logger.h"

using namespace Utils;

namespace UI
{
    namespace
    {
        // AccountUid is a 128-bit value (u64[2]); make it usable as a map key.
        struct UidKey
        {
            u64 highHalf, lowHalf;
            bool operator<(const UidKey &other) const
            {
                return highHalf != other.highHalf ? highHalf < other.highHalf
                                                  : lowHalf < other.lowHalf;
            }
        };

        std::map<UidKey, IconImage> s_userCache;
        std::map<u64, IconImage> s_titleCache;
        std::map<std::string, IconImage> s_fileCache;

        // Decode a JPEG/PNG blob to a session-owned RGBA IconImage (invalid on failure).
        IconImage decodeToRGBA(const unsigned char *jpg, int length)
        {
            IconImage image;
            int imageWidth = 0, h = 0, comp = 0;
            unsigned char *rgba = stbi_load_from_memory(jpg, length, &imageWidth, &h, &comp, 4);
            if (!rgba)
                return image;
            image.data = rgba;
            image.width = imageWidth;
            image.height = h;
            return image;
        }
    }

    const IconImage &SystemIcons::fileIcon(const std::string &path)
    {
        if (path.empty())
        {
            static const IconImage empty{};
            return empty;
        }

        auto iterator = s_fileCache.find(path);
        if (iterator != s_fileCache.end())
            return iterator->second;

        IconImage image;
        int width = 0, height = 0, comp = 0;
        std::string fullPath = path;
        if (fullPath.rfind("romfs:/", 0) != 0 && fullPath.rfind("sdmc:/", 0) != 0 && fullPath.rfind("/", 0) != 0)
        {
            fullPath = "romfs:/" + path;
        }

        unsigned char *rgba = stbi_load(fullPath.c_str(), &width, &height, &comp, 4);
        if (rgba)
        {
            image.data = rgba;
            image.width = width;
            image.height = height;
        }
        else if (path.rfind("sdmc:/", 0) != 0 && path.rfind("romfs:/", 0) != 0)
        {
            std::string alt = "sdmc:/" + path;
            rgba = stbi_load(alt.c_str(), &width, &height, &comp, 4);
            if (rgba)
            {
                image.data = rgba;
                image.width = width;
                image.height = height;
            }
        }

        if (!image.valid())
        {
            logInfoToFile("SystemIcons: failed to load icon from file", path.c_str());
        }
        return s_fileCache.emplace(path, image).first->second;
    }

    const IconImage &SystemIcons::userIcon(AccountUid accountUid)
    {
        UidKey key{accountUid.uid[0], accountUid.uid[1]};
        auto iterator = s_userCache.find(key);
        if (iterator != s_userCache.end())
            return iterator->second;

        IconImage image;
        AccountProfile profile;
        if (R_SUCCEEDED(accountGetProfile(&profile, accountUid)))
        {
            u32 imgSize = 0;
            if (R_SUCCEEDED(accountProfileGetImageSize(&profile, &imgSize)) && imgSize > 0)
            {
                std::vector<unsigned char> jpg(imgSize);
                u32 realSize = 0;
                if (R_SUCCEEDED(accountProfileLoadImage(&profile, jpg.data(), imgSize, &realSize)) && realSize > 0)
                {
                    image = decodeToRGBA(jpg.data(), static_cast<int>(realSize));
                }
            }
            accountProfileClose(&profile);
        }
        if (!image.valid())
            logErrorToFile("SystemIcons: failed to load user avatar");
        return s_userCache.emplace(key, image).first->second;
    }

    const IconImage &SystemIcons::titleIcon(u64 titleId)
    {
        auto iterator = s_titleCache.find(titleId);
        if (iterator != s_titleCache.end())
            return iterator->second;

        IconImage image;
        // NsApplicationControlData is large (~0x24000); heap-allocate it.
        NsApplicationControlData *ctl =
            static_cast<NsApplicationControlData *>(malloc(sizeof(NsApplicationControlData)));
        if (ctl)
        {
            u64 outSize = 0;
            Result resultCode = nsGetApplicationControlData(NsApplicationControlSource_Storage, titleId,
                                                    ctl, sizeof(NsApplicationControlData), &outSize);
            if (R_SUCCEEDED(resultCode) && outSize > sizeof(ctl->nacp))
            {
                int iconLen = static_cast<int>(outSize - sizeof(ctl->nacp));
                image = decodeToRGBA(ctl->icon, iconLen);
            }
            free(ctl);
        }
        if (!image.valid())
            logErrorToFile("SystemIcons: failed to load title icon");
        return s_titleCache.emplace(titleId, image).first->second;
    }

    void SystemIcons::cleanup()
    {
        for (auto &kv : s_userCache)
            if (kv.second.data)
                stbi_image_free(kv.second.data);
        for (auto &kv : s_titleCache)
            if (kv.second.data)
                stbi_image_free(kv.second.data);
        for (auto &kv : s_fileCache)
            if (kv.second.data)
                stbi_image_free(kv.second.data);
        s_userCache.clear();
        s_titleCache.clear();
        s_fileCache.clear();
    }
}

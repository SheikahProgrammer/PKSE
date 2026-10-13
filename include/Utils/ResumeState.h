#ifndef UTILS_RESUME_STATE_H
#define UTILS_RESUME_STATE_H

#include <map>
#include <string>

namespace Utils
{
    /**
     * Where the user left off, so going back to a screen lands where they were.
     *
     * Persisted in sdmc:/PKSE/resume.cfg as `[section]` blocks of key=value lines, so it also
     * survives closing the app. One section per save (keyed by title + account, or by file path
     * for a loose save -- an emulator save under the Tico or Citra user) plus one for the save
     * picker itself. The newest sections are kept first and the oldest dropped past a cap: this
     * is a convenience, and a file that only grows would be a bug.
     *
     * Values are plain text; a missing or unreadable field reads back as the caller's default, so
     * a resume written by an older build -- or a save whose layout changed since -- never does
     * worse than starting from the top.
     */
    namespace Resume
    {
        using Fields = std::map<std::string, std::string>;

        /// The stored fields for `section`, empty when there are none. Loads the file on first use.
        Fields get(const std::string &section);
        /// Replaces `section` with `fields`, moves it to the front, and writes the file.
        void put(const std::string &section, const Fields &fields);

        /// `fields[key]` as an int, or `fallback` when absent or not a number.
        int intOf(const Fields &fields, const std::string &key, int fallback);
        /// `fields[key]`, or "" when absent.
        std::string textOf(const Fields &fields, const std::string &key);
    }
}

#endif

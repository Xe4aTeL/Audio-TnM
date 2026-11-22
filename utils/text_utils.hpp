#ifndef TEXT_UTILS_HPP
#define TEXT_UTILS_HPP

#include <string>
#include <vector>

namespace TextUtils
{
    struct RGB
    {
        unsigned char r, g, b;
    };

    struct WordData
    {
        float prob;
        int64_t t0, t1;
        std::string word;
    };

    // Converts the 0.0 - 1.0 value into an RGB color based on the selected method
    // Method 1 = Gradient (Red -> Yellow -> Green)
    // Method 2 = Keyword Selection (Red if == 1.0)
    RGB GetColorForProb(float value);

    class TextBank
    {
    public:
        TextBank();

        void LoadWordFromTranscription(const std::string &data, const float prob, const int64_t t0, const int64_t t1);
        void ClearLastTranscription() { m_words.clear(); }
        void UpdateFromUserInput(const std::string &data);

        void InsertKeyword(const std::string &word);
        void ClearKeywords() { m_keywords.clear(); }

        const std::vector<WordData> &GetWords() const;
        std::string GetFullText() const;

        std::string GetKeywords() const;
        RGB GetColorKeyword(std::string word);
        std::vector<std::string> SplitByWhitespace(const std::string &data) const;

    private:
        std::vector<WordData> m_words{};
        std::vector<std::string> m_keywords{};
    };
}

#endif // TEXT_UTILS_HPP

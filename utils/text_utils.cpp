#include "text_utils.hpp"
#include <algorithm>
#include <cctype>

namespace TextUtils
{
    RGB GetColorForProb(float value)
    {
        if (value <= 0.5f)
            return {200, static_cast<unsigned char>(value * 2.0f * 200), 0};
        else
            return {static_cast<unsigned char>((1.0f - value) * 2.0f * 200), 200, 0};
    }

    TextBank::TextBank() {}

    void TextBank::LoadWordFromTranscription(const std::string &data, const float prob, const int64_t t0, const int64_t t1)
    {
        m_words.push_back({prob, t0, t1, data});
    }

    void TextBank::UpdateFromUserInput(const std::string &fullText)
    {
        // If I implement it, this program will be able to handle new text by user in the UI
    }

    void TextBank::InsertKeyword(const std::string &word)
    {
        m_keywords.push_back(word);
    }

    const std::vector<WordData> &TextBank::GetWords() const
    {
        return m_words;
    }

    std::string TextBank::GetFullText() const
    {
        std::string result;
        for (size_t i = 0; i < m_words.size(); ++i)
            result += m_words[i].word;
        return result;
    }

    std::string TextBank::GetKeywords() const
    {
        std::string result;
        for (size_t i = 0; i < m_keywords.size(); ++i)
        {
            result += m_keywords[i];
            if (i < m_keywords.size() - 1)
                result += " ";
        }
        return result;
    }

    RGB TextBank::GetColorKeyword(std::string word)
    {
        if (std::find(m_keywords.begin(), m_keywords.end(), word) != m_keywords.end())
        {
            // Found a keyword
            return {200, 0, 0};
        }

        return {0, 0, 0};
    }

    std::vector<std::string> TextBank::SplitByWhitespace(const std::string &str) const
    {
        std::vector<std::string> tokens;
        std::string current;
        for (char c : str)
        {
            if (std::isspace(static_cast<unsigned char>(c)))
            {
                if (!current.empty())
                {
                    tokens.push_back(current);
                    current.clear();
                }
            }
            else
            {
                current += c;
            }
        }
        if (!current.empty())
            tokens.push_back(current);
        return tokens;
    }
}

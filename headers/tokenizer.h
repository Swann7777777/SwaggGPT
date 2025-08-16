#ifndef SWAGGGPT_TOKENIZERS_H
#define SWAGGGPT_TOKENIZERS_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <ranges>
#include <string_view>
#include <regex>


class tokenizer {
public:

    class trieNode {
    public:

        trieNode* children[26]{};

        int index;

        trieNode() {

            index = -1;

            for (auto & i : children) {
                i = nullptr;
            }
        }
    };



    struct pairHash {

        std::size_t operator()(const std::pair<int, int>& p) const {

            return static_cast<std::size_t>(p.first) * 31 + p.second;
        }
    };

    struct vectorHash {
        std::size_t operator()(const std::vector<int> &v) const {

            std::size_t seed = v.size();

            for(auto& i : v) {
                seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }

            return seed;
        }
    };


    static void buildTrie(trieNode* &root, const std::vector<std::string> &vocabulary) {


        int index = 0;

        for (const auto& word : vocabulary) {

            trieNode* node = root;

            for (const auto& c : word) {

                if (node->children[c - 'a'] == nullptr) {

                    node->children[c - 'a'] = new trieNode();
                }

                node = node->children[c - 'a'];
            }

            node->index = index;

            index++;
        }

    }


    static void loadVocabulary(std::vector<std::string> &vocabulary) {

        std::ifstream vocabularyFileIn("../output/vocabulary.txt");

        vocabulary.reserve(10000);

        std::string line;

        while (std::getline(vocabularyFileIn, line)) {
            vocabulary.push_back(line);
        }
    }


    static void insertTrie(const std::string &key, trieNode* &root, const int &index) {

        trieNode* node = root;

        for (const char i : key) {

            if (node->children[i - 'a'] == nullptr) {

                const auto newNode = new trieNode();

                node->children[i - 'a'] = newNode;
            }

            node = node->children[i - 'a'];
        }

        node->index = index;
    }



    static void loadWords(std::unordered_map<std::string, int> &words) {

        //std::ifstream corpusFile("../testCorpus.txt");
        std::ifstream corpusFile("/home/swann7777777/Documents/simplewiki-20250701-pages-articles-multistream.xml");


        std::string line;

        std::unordered_map<std::string, char> specialChar = {
            {"\xC3\xA9", 'e'}, {"\xC3\xA0", 'a'}, {"\xC3\xA8", 'e'}, {"\xC3\xB9", 'u'},
            {"\xC3\xAA", 'e'}, {"\xC3\xA2", 'a'}, {"\xC3\xA7", 'c'}, {"\xC3\xB4", 'o'},
            {"\xC3\xAE", 'i'}, {"\xC3\xAF", 'i'}, {"\xC3\xBB", 'u'}
        };

        std::unordered_map<std::string_view, char> htmlEntities = {
            {"&quot;", '"'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&amp;", '&'},
            {"&apos;", '\''}
        };

        std::regex url("url=.+?;");

        std::stringstream word;

        bool stop = false;

        while (!stop) {

            bool match = false;

            while (!match) {

                if (!getline(corpusFile, line)) {
                    stop = true;
                    break;
                }

                if (line.find("<text") != std::string::npos && line.find("</text") == std::string::npos) {

                    while (!match) {

                        if (!getline(corpusFile, line)) {
                            stop = true;
                            break;
                        }

                        if (line.find("</text") != std::string::npos) {

                            match = true;
                            break;
                        }

                        if (line[0] != '[' && line[0] != '!' && line[0] != '|') {

                            line = std::regex_replace(line, url, "");

                            for (int i = 0; i < line.size(); i++) {

                                unsigned char c = line[i];

                                if (c == '[') {
                                    i++;
                                    c = line[i];
                                    if (c == '[') {
                                        continue;
                                    }

                                    while (c != ']' && i + 1 < line.size()) {
                                        i++;
                                        c = line[i];
                                    }

                                }

                                if (c >= 0xC3 && i + 1 < line.size()) {
                                    if (std::string utf8Char = line.substr(i, 2); specialChar.contains(utf8Char)) {

                                        word << specialChar[utf8Char];
                                    }

                                    i += 2;
                                }

                                if (c == '&') {
                                    for (auto& [entity, ch] : htmlEntities) {

                                        if (i + entity.length() <= line.size() && line.compare(i, entity.length(), entity) == 0) {

                                            line.replace(i, entity.length(), std::string(1, ch));

                                            i += entity.length();

                                            break;
                                        }
                                    }
                                }

                                if (std::isalpha(c)) {

                                    word << static_cast<char>(std::tolower(c));
                                }

                                else {
                                    if (!word.str().empty()) {
                                        words[word.str()]++;
                                        word.str("");
                                        word.clear();

                                    }
                                }
                            }
                            if (!word.str().empty()) {
                                words[word.str()]++;
                                word.str("");
                                word.clear();
                            }
                        }
                    }
                }
            }
        }
    }


    static void tokenizeWords(const std::unordered_map<std::string, int> &words, std::unordered_map<std::vector<int>, int, vectorHash> &tokenizedWords, const trieNode* root) {

        for (const auto& [word, count] : words) {

            const trieNode* node = root;

            std::vector<int> tempTokens;

            int index = 0;

            int token = -1;

            for (int i = 0; i < word.size();) {

                if (node->children[word[i] - 'a'] != nullptr) {

                    node = node->children[word[i] - 'a'];

                    if (node->index != -1) {
                        token = node->index;
                        index = i;
                    }

                    i++;
                }

                else {
                    tempTokens.push_back(token);
                    i = index + 1;
                    node = root;
                }
            }

            tempTokens.push_back(token);

            tokenizedWords[tempTokens] = count;
        }
    }



    static void tokenize() {

        std::vector<std::string> vocabulary;

        vocabulary.reserve(30000);


        loadVocabulary(vocabulary);

        auto *root = new trieNode();

        buildTrie(root, vocabulary);

        std::unordered_map<std::string, int> words;

        words.reserve(10000);

        loadWords(words);

        std::unordered_map<std::vector<int>, int, vectorHash> tokenizedWords;

        tokenizedWords.reserve(words.size());

        tokenizeWords(words, tokenizedWords, root);


        for (const auto&[fst, snd] : tokenizedWords) {
            for (const auto& i : fst) {
                std::cout << vocabulary[i] << " ";
            }
            std::cout << ": " << snd << "\n";
        }

        std::cout << tokenizedWords.size();








    }



};











#endif //SWAGGGPT_TOKENIZERS_H
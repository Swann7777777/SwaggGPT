#ifndef SWAGGGPT_BACKUP_H
#define SWAGGGPT_BACKUP_H

#ifndef SWAGGGPT_TOKENIZERS_H
#define SWAGGGPT_TOKENIZERS_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <thread>
#include <mutex>
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


    static void loadVocabulary(std::ifstream &vocabularyFileIn, std::vector<std::string> &vocabulary) {

        vocabulary.reserve(10000);

        std::string line;

        while (std::getline(vocabularyFileIn, line)) {
            vocabulary.push_back(line);
        }
    }


    static void insertTrie(const std::string &key, trieNode* root, const int &index) {

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



    static void loadWords(std::unordered_map<std::string, int> &words, std::ifstream &corpusFile) {

        std::unordered_map<std::string_view, char> htmlEntities = {
            {"&quot;", '"'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&amp;", '&'},
            {"&apos;", '\''}
        };


        std::string line;


        while (true) {

            if (!getline(corpusFile, line)) {
                break;
            }

            if (line.find("<text") != std::string::npos) {

                while (true) {

                    if (!getline(corpusFile, line)) {
                        break;
                    }

                    if (line.find("</text") != std::string::npos) {
                        break;
                    }

                    if (line[0] == '[' || line[0] == '!' || line[0] == '|' || line[0] == '=' || line[0] == ':') {
                        continue;
                    }

                    std::string word;

                    for (int i = 0; i < line.size(); i++) {

                        unsigned char c = line[i];

                        if (c == '[') {

                            i++;

                            if (line[i] == '[') {
                                continue;
                            }

                            while (i + 1 < line.size() && line[i] != ']') {
                                i++;
                            }
                        }

                        if (c == '{') {

                            i++;

                            if (line[i] != '{') {
                                continue;
                            }

                            while (line[i] != '}' && i + 1 < line.size() && line[i + 1] != '}') {
                                i++;
                            }
                        }

                        if (c == '&') {
                            for (auto& [entity, ch] : htmlEntities) {

                                if (i + entity.length() <= line.size() && line.compare(i, entity.length(), entity) == 0) {

                                    line.replace(i, entity.length(), std::string(1, ch));

                                    i += static_cast<int>(entity.length());

                                    break;
                                }
                            }
                        }

                        if (std::isalpha(c)) {
                            word += static_cast<char>(std::tolower(c));
                        }

                        else if (!word.empty()) {
                            words[word]++;
                            word.clear();
                        }
                    }

                    if (!word.empty()) {
                        words[word]++;
                        word.clear();
                    }
                }
            }
        }
    }


    static std::vector<int> tokenizeWord(const std::string &word,
        const trieNode* root) {

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

        return tempTokens;
    }




    static void createPairs(const std::vector<int> &tokenizedWord,
        const int &count,
        std::unordered_map<std::pair<int, int>, int, pairHash> &pairs) {


        for (int i = 0; i < tokenizedWord.size() - 1; i++) {

            pairs[{tokenizedWord[i], tokenizedWord[i + 1]}] += count;
        }
    }

    static void outputVocabulary(
        std::ofstream &vocabularyFileOut,
        std::vector<std::string> &vocabulary) {


        for (const auto& i : vocabulary) {

            vocabularyFileOut << "\n" << i;
        }
    }


    static void tokenize() {

        std::vector<std::thread> threads;

        std::vector<std::string> vocabulary;

        vocabulary.reserve(30000);

        std::ifstream vocabularyFileIn("../output/vocabulary.txt");

        loadVocabulary(vocabularyFileIn, vocabulary);

        auto *root = new trieNode();

        buildTrie(root, vocabulary);

        std::unordered_map<std::string, int> words;

        words.reserve(1000000);

        std::ifstream corpusFile("/home/swann7777777/Documents/simplewiki-20250701-pages-articles-multistream.xml");

        loadWords(words, corpusFile);


        std::unordered_map<std::vector<int>, int, vectorHash> tokenizedWords;

        tokenizedWords.reserve(words.size());

        std::unordered_map<std::pair<int, int>, int, pairHash> pairs;

        pairs.reserve(10000000);



        for (const auto& [word, count] : words) {
            if (std::vector<int> tokens = tokenizeWord(word, root); tokens.size() > 1) {
                tokenizedWords[tokens] = count;
            }
        }

        std::pair<std::pair<int, int>, int> max = {{0, 0}, 0};


        for (int i = 0; i < 1000; i++) {

            std::vector<std::vector<int>> tmp;


            for (auto& [word, count] : tokenizedWords) {

                std::vector<int> newWord;

                if (word.size() < 2) {
                    continue;
                }

                for (int j = 0; j < word.size() - 1; j++) {
                    if (word[j] == max.first.first && word[j + 1] == max.first.second) {

                        newWord = word;

                        for (int k = 0; k < 2; k++) {
                            newWord.erase(newWord.begin() + j);
                        }

                        newWord.insert(newWord.begin() + j, vocabulary.size() - 1);
                    }
                }

                if (!newWord.empty()) {
                    tmp.push_back(word);
                    tokenizedWords[newWord] = count;
                }
            }

            for (const auto& j : tmp) {
                tokenizedWords.erase(j);
            }

            max = {{0, 0}, 0};



            auto start = std::chrono::high_resolution_clock::now();

            pairs.clear();

            for (const auto& [tokenizedWord, count] : tokenizedWords) {

                createPairs(tokenizedWord, count, pairs);
            }

            for (const auto& [pair, count] : pairs) {
                if (count > max.second) {
                    max = {pair, count};
                }
            }

            insertTrie(vocabulary[max.first.first] + vocabulary[max.first.second], root, vocabulary.size());

            vocabulary.push_back(vocabulary[max.first.first] + vocabulary[max.first.second]);


            std::cout << vocabulary[max.first.first] + vocabulary[max.first.second] << " : " << max.second << "\n";

            std::cout << static_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - start) << "\n";
        }

        std::ofstream vocabularyFileOut("../output/vocabulary.txt");

        outputVocabulary(vocabularyFileOut, vocabulary);

        vocabularyFileOut.close();
    }
};





#endif //SWAGGGPT_TOKENIZERS_H
#endif //SWAGGGPT_BACKUP_H
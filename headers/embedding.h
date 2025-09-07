#ifndef EMBEDDING_H
#define EMBEDDING_H


#include <cmath>
#include <random>
#include <numeric>


class embedding {
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


    static void loadVocabulary(std::ifstream &vocabularyFile, std::vector<std::string> &vocabulary) {

        vocabulary.reserve(10000);

        std::string line;

        while (std::getline(vocabularyFile, line)) {
            vocabulary.push_back(line);
        }
    }


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



    static bool loadWords(std::vector<std::string> &article, std::ifstream &corpusFile) {

        std::unordered_map<std::string_view, char> htmlEntities = {
            {"&quot;", '"'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&amp;", '&'},
            {"&apos;", '\''}
        };


        std::string line;

        while (getline(corpusFile, line)) {

            if (line.find("<text") != std::string::npos) {

                while (true) {

                    if (!getline(corpusFile, line)) {
                        return false;
                    }

                    if (line.find("</text") != std::string::npos) {
                        return true;
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
                            article.push_back(word);
                            word.clear();
                        }
                    }

                    if (!word.empty()) {
                        article.push_back(word);
                    }
                }
            }
        }

        return false;
    }


    static void tokenizeWords(const std::vector<std::string> &words,
    const trieNode* root,
    std::vector<int> &tokenizedWords) {

        for (const auto& word : words) {

            const trieNode* node = root;

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

                    tokenizedWords.push_back(token);
                    i = index + 1;
                    node = root;
                }
            }
            tokenizedWords.push_back(token);
        }
    }


    static void loadEmbeddings(std::ifstream &embeddingsFileIn, const int &dimension, const int &size, std::vector<std::vector<float>> &embeddings) {

        embeddings.resize(size);

        for (auto& i : embeddings) {

            i.resize(dimension);
        }

        for (auto& i : embeddings) {

            embeddingsFileIn.read(reinterpret_cast<char*>(i.data()), dimension * sizeof(float));
        }
    }


    static void generateEmbeddings(std::ofstream &embeddingsFileOut, const int &dimension, const int &size) {

        std::random_device dev;
        std::mt19937 rng(dev());
        std::uniform_int_distribution<> dist(-10000, 10000);

        std::vector<std::vector<float>> embeddings(size);

        for (int i = 0; i < size; i++) {

            for (int j = 0; j < dimension; j++) {

                embeddings[i].push_back(static_cast<float>(dist(rng)) / static_cast<float>(10000));
            }

            embeddingsFileOut.write(reinterpret_cast<const char*>(embeddings[i].data()), dimension * sizeof(float));
        }
    }


    static float sigmoid(const float x) {
        return 1.0f / (1.0f + std::exp(-x));
    }

    static void forwardPass(std::vector<std::vector<float>> &embeddings,
        const int &windowSize,
        const int &negativeSamplesCount,
        const std::vector<int> &tokenizedWords) {


        std::random_device dev;
        std::mt19937 rng(dev());
        std::uniform_int_distribution<> dist(0, static_cast<int>(tokenizedWords.size() - 1));


        for (int i = 0; i < 100; i++) {



            for (int j = i - windowSize; j < i + windowSize + 1; j++) {

                std::vector<float> negativeSamplesDotProducts;

                if (j == i || j < 0 || j >= tokenizedWords.size()) {
                    continue;
                }

                const float positiveSampleDotProduct = std::inner_product(embeddings[tokenizedWords[j]].begin(),
                    embeddings[tokenizedWords[j]].end(),
                    embeddings[tokenizedWords[i]].begin(),
                    0.0f);

                //std::cout << positiveSamplesDotProduct << "\n\n";

                for (int k = 0; k < negativeSamplesCount; k++) {

                    const int negativeSampleIndex = dist(rng);

                    negativeSamplesDotProducts.push_back(std::inner_product(embeddings[tokenizedWords[negativeSampleIndex]].begin(),
                        embeddings[tokenizedWords[negativeSampleIndex]].end(),
                        embeddings[tokenizedWords[i]].begin(),
                        0.0f));

                    //std::cout << negativeSamplesDotProducts.back() << "\n";
                }

                //std::cout << "\n";

                float loss = -std::log(sigmoid(positiveSampleDotProduct));

                for (const auto& dotProduct : negativeSamplesDotProducts) {

                    loss -= std::log(sigmoid(-dotProduct));
                }

                std::cout << loss << "\n";
            }
        }
    }









    static void outputEmbeddings(const int &dimension,const std::vector<std::vector<float>> &embeddings, std::ofstream &embeddingsFileOut) {


        for (const auto& i : embeddings) {

            embeddingsFileOut.write(reinterpret_cast<const char*>(i.data()), dimension * sizeof(float));
        }
    }



    static void embed() {

        constexpr int dimension = 512;

        constexpr int windowSize = 3;

        constexpr float learningRate = 1.0f;

        constexpr int negativeSamplesCount = 5;

        std::ofstream embeddingsFileOut("../output/embeddings.bin", std::ios::binary);

        auto *root = new trieNode;

        std::vector<std::string> vocabulary;

        std::ifstream vocabularyFile("../output/vocabulary.txt");

        loadVocabulary(vocabularyFile, vocabulary);

        std::vector<std::vector<float>> embeddings;

        generateEmbeddings(embeddingsFileOut, dimension, static_cast<int>(vocabulary.size()));

        embeddingsFileOut.close();

        std::ifstream embeddingsFileIn("../output/embeddings.bin", std::ios::binary);

        loadEmbeddings(embeddingsFileIn, dimension, vocabulary.size(), embeddings);

        buildTrie(root, vocabulary);

        std::ifstream corpusFile("/home/swann7777777/Documents/simplewiki-20250701-pages-articles-multistream.xml");

        std::vector<std::string> words;

        while (loadWords(words, corpusFile)) {

            std::vector<int> tokenizedWords;

            tokenizeWords(words, root, tokenizedWords);

            words.clear();

            forwardPass(embeddings, windowSize, negativeSamplesCount, tokenizedWords);

            break;
        }

        outputEmbeddings(dimension, embeddings, embeddingsFileOut);
    }


};




#endif //EMBEDDING_H

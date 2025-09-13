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


    static void generateEmbeddings(std::ofstream &embeddingsFileOut, const int &dimension, const int &size, std::mt19937 rng) {

        std::uniform_real_distribution<float> dist(-0.5f, 0.5f);

        std::vector<std::vector<float>> embeddings(size);

        for (int i = 0; i < size; i++) {

            for (int j = 0; j < dimension; j++) {

                embeddings[i].push_back(dist(rng));
            }

            embeddingsFileOut.write(reinterpret_cast<const char*>(embeddings[i].data()), dimension * sizeof(float));
        }
    }







    static float sigmoid(const float x) {
        return 1.0f / (1.0f + std::exp(-x));
    }





    static void forwardPass(
        float &positiveError,
        std::vector<float> &negativeErrors,
        const std::vector<float> &centreEmbedding,
        const std::vector<float> &contextEmbedding,
        const std::vector<int> &negativeIndices,
        const std::vector<std::vector<float>> &embeddings) {



        std::vector<float> negativeSamplesDotProducts(negativeIndices.size());

        const float positiveSampleDotProduct = std::inner_product(contextEmbedding.begin(),
            contextEmbedding.end(),
            centreEmbedding.begin(),
            0.0f);

        positiveError = sigmoid(positiveSampleDotProduct);

        for (int i = 0; i < negativeIndices.size(); i++) {

            negativeSamplesDotProducts[i] = std::inner_product(embeddings[negativeIndices[i]].begin(),
                embeddings[negativeIndices[i]].end(),
                centreEmbedding.begin(),
                0.0f);

        }

        for (int i = 0; i < negativeSamplesDotProducts.size(); i++) {

            negativeErrors[i] = sigmoid(-negativeSamplesDotProducts[i]);
        }
    }


    static void backpropagation(
        std::vector<float> &centreEmbedding,
        std::vector<float> &contextEmbedding,
        const float &learningRate,
        const std::vector<int> &negativeIndices,
        std::vector<std::vector<float>> &embeddings,
        const float &positiveError,
        const std::vector<float> &negativeErrors) {


        std::vector<float> centreSignal(contextEmbedding.size());
        std::vector<float> contextSignal(centreEmbedding.size());
        std::vector<std::vector<float>> negativeSignals(negativeIndices.size());

        for (auto& i : negativeSignals) {

            i.resize(centreEmbedding.size());
        }


        for (int i = 0; i < contextEmbedding.size(); i++) {

            centreSignal[i] = (positiveError - 1) * contextEmbedding[i];
        }

        for (int i = 0; i < negativeIndices.size(); i++) {
            for (int j = 0; j < embeddings[negativeIndices[i]].size(); j++) {

                centreSignal[j] += (1 - negativeErrors[i]) * embeddings[negativeIndices[i]][j];
            }
        }

        for (int i = 0; i < centreEmbedding.size(); i++) {

            contextSignal[i] = (positiveError - 1) * centreEmbedding[i];
        }


        for (int i = 0; i < negativeIndices.size(); i++) {

            for (int j = 0; j < centreEmbedding.size(); j++) {

                negativeSignals[i][j] = (1 - negativeErrors[i]) * centreEmbedding[j];
            }

            for (int j = 0; j < embeddings[negativeIndices[i]].size(); j++) {

                embeddings[negativeIndices[i]][j] -= negativeSignals[i][j] * learningRate;
            }
        }

        for (int i = 0; i < centreEmbedding.size(); i++) {

            centreEmbedding[i] -= centreSignal[i] * learningRate;
        }

        for (int i = 0; i < contextEmbedding.size(); i++) {

            contextEmbedding[i] -= contextSignal[i] * learningRate;
        }
    }



    static void outputEmbeddings(const std::vector<std::vector<float>> &embeddings, std::ofstream &embeddingsFileOut) {


        for (const auto& i : embeddings) {

            embeddingsFileOut.write(reinterpret_cast<const char*>(i.data()), i.size() * sizeof(float));
        }
    }



    static void embed() {

        constexpr int dimension = 512;

        constexpr int windowSize = 5;

        constexpr float learningRate = 0.05f;

        constexpr int negativeSamplesCount = 5;





        std::random_device dev;
        std::mt19937 rng(dev());






        auto *root = new trieNode;

        std::vector<std::string> vocabulary;

        std::ifstream vocabularyFile("../output/vocabulary.txt");

        loadVocabulary(vocabularyFile, vocabulary);

        buildTrie(root, vocabulary);




        std::ofstream embeddingsFileOut("../output/embeddings.bin", std::ios::binary);

        generateEmbeddings(embeddingsFileOut, dimension, static_cast<int>(vocabulary.size()), rng);

        embeddingsFileOut.close();





        std::ifstream embeddingsFileIn("../output/embeddings.bin", std::ios::binary);

        std::vector<std::vector<float>> embeddings;

        loadEmbeddings(embeddingsFileIn, dimension, static_cast<int>(vocabulary.size()), embeddings);



        //while (true) {
        //}


        std::vector<std::string> words;

        std::ifstream corpusFile("/home/swann7777777/Documents/simplewiki-20250701-pages-articles-multistream.xml");

        std::uniform_int_distribution dist(0, static_cast<int>(vocabulary.size() - 1));

        float average_loss = 0;


        size_t iterations = 0;


        while (loadWords(words, corpusFile)) {

            std::vector<int> tokenizedWords;

            float positiveError;

            std::vector<float> negativeErrors(negativeSamplesCount);

            std::vector<int> negativeSamplesIndices;

            tokenizeWords(words, root, tokenizedWords);

            words.clear();


            for (int i = 0; i < tokenizedWords.size(); i++) {

                for (int j = i - windowSize; j < i + windowSize + 1; j++) {

                    if (j == i || j < 0 || j >= tokenizedWords.size()) {
                        continue;
                    }


                    std::vector<float> &centreEmbedding = embeddings[tokenizedWords[i]];

                    std::vector<float> &contextEmbedding = embeddings[tokenizedWords[j]];

                    std::vector<int> negativeIndices(negativeSamplesCount);

                    for (int k = 0; k < negativeSamplesCount; k++) {

                        negativeIndices[k] = dist(rng);
                    }

                    forwardPass(
                        positiveError,
                        negativeErrors,
                        embeddings[tokenizedWords[i]],
                        contextEmbedding,
                        negativeIndices,
                        embeddings);


                    float loss = -std::log(positiveError);

                    for (const auto& k : negativeErrors) {

                        loss += -std::log(k);
                    }


                    //std::cout << loss << "\n";

                    average_loss += loss;

                    iterations++;

                    backpropagation(
                        embeddings[tokenizedWords[i]],
                        contextEmbedding,
                        learningRate,
                        negativeIndices,
                        embeddings,
                        positiveError,
                        negativeErrors);



                    negativeErrors.clear();
                }
            }

            std::cout << "\n" << average_loss / iterations << "\n";
        }

        std::ofstream embeddingsFileOut2("../output/embeddings.bin", std::ios::binary);

        outputEmbeddings(embeddings, embeddingsFileOut2);

        embeddingsFileOut2.close();
    }
};




#endif //EMBEDDING_H
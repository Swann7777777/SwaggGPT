#ifndef EMBEDDING_H
#define EMBEDDING_H



#include <iostream>
#include <vector>

class embedding {
public:

    static void loadWords(std::vector<std::string> &words) {

        std::ifstream corpusFile("../testCorpus.txt");
        //std::ifstream corpusFile("/home/swann7777777/Documents/simplewiki-20250701-pages-articles-multistream.xml");

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

                            while (line[i] != ']') {
                                i++;
                            }
                        }

                        if (c == '{') {

                            i++;

                            if (line[i] != '{') {
                                continue;
                            }

                            while (line[i] != '}' && line[i + 1] != '}') {
                                i++;
                            }
                        }

                        if (std::isalpha(c)) {
                            word += static_cast<char>(std::tolower(c));
                        }

                        else if (!word.empty()) {
                            words.push_back(word);
                            word.clear();
                        }
                    }
                }
            }
        }
    }



    static void embed() {

        std::vector<std::string> words;

        loadWords(words);
    }


};






#endif //EMBEDDING_H

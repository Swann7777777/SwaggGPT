#ifndef TOKENIZER_H
#define TOKENIZER_H


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





	struct pair_hash {

		std::size_t operator()(const std::pair<int, int>& p) const {
			return static_cast<std::size_t>(p.first) * 31 + p.second;
		}
	};


	static std::vector<std::string> loadVocabulary() {

		std::ifstream vocabularyFileIn("../output/vocabulary.txt");

		std::vector<std::string> vocabulary;

		vocabulary.reserve(10000);

		std::string line;

		while (std::getline(vocabularyFileIn, line)) {
			vocabulary.push_back(line);
		}

		return vocabulary;
	}


	static void buildTrie(const std::vector<std::string> &vocabulary, trieNode* root) {

		for (int i = 0; i < vocabulary.size(); i++) {

			trieNode* node = root;

			for (const char j : vocabulary[i]) {

				if (node->children[j - 'a'] == nullptr) {


					const auto newNode = new trieNode();

					node->children[j - 'a'] = newNode;

				}

				node = node->children[j - 'a'];

			}

			node->index = i;
		}
	}


	static void stripTagsAndTemplates(std::string &line) {
		std::string result;
		bool inTag = false;
		bool inTemplate = false;

		for (size_t i = 0; i < line.size(); ++i) {
			if (!inTag && !inTemplate && line[i] == '<') {
				inTag = true;
				continue;
			}
			if (inTag && line[i] == '>') {
				inTag = false;
				continue;
			}
			if (!inTag && !inTemplate && line[i] == '{' && i + 1 < line.size() && line[i+1] == '{') {
				inTemplate = true;
				++i;
				continue;
			}
			if (inTemplate && line[i] == '}' && i + 1 < line.size() && line[i+1] == '}') {
				inTemplate = false;
				++i;
				continue;
			}
			if (!inTag && !inTemplate) {
				result += line[i];
			}
		}

		line = std::move(result);
	}


	static bool loadCorpus(std::ifstream &corpusStream, std::string &corpusString) {

		corpusString.reserve(20000);


		corpusString = "";

		std::string line;



		bool match = false;


		while (!match) {


			if (!std::getline(corpusStream, line)) {
				return false;
			}


			if (int pos = line.find("<text"); pos != std::string::npos) {

				while (!match) {


					if (!std::getline(corpusStream, line)) {
						return false;
					}


					pos = line.find("</text");

					if (pos != std::string::npos) {
						match = true;
					}

					else {
						if (line[0] != '[' && line[0] != '!' && line[0] != '|') {


							stripTagsAndTemplates(line);


							corpusString += line + " ";
						}
					}
				}
			}
		}

		return true;

	}


	static std::vector<std::string> normalizeCorpus(std::string &corpusString) {

		std::unordered_map<std::string, char> specialChar = {
			{"\xC3\xA9", 'e'}, {"\xC3\xA0", 'a'}, {"\xC3\xA8", 'e'}, {"\xC3\xB9", 'u'},
			{"\xC3\xAA", 'e'}, {"\xC3\xA2", 'a'}, {"\xC3\xA7", 'c'}, {"\xC3\xB4", 'o'},
			{"\xC3\xAE", 'i'}, {"\xC3\xAF", 'i'}, {"\xC3\xBB", 'u'}
		};

		std::unordered_map<std::string_view, char> htmlEntities = {
			{"&quot;", '"'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&amp;", '&'},
			{"&apos;", '\''}
		};


		std::vector<std::string> words;
		words.reserve(4000);

		std::stringstream word;
		for (size_t i = 0; i < corpusString.size();) {
			const unsigned char c = corpusString[i];

			if (c >= 0xC3 && i + 1 < corpusString.size()) {
				if (std::string utf8Char = corpusString.substr(i, 2); specialChar.contains(utf8Char)) {
					word << specialChar[utf8Char];
				}
				i += 2;
				continue;
			}


			if (c == '&') {
				for (auto& [entity, ch] : htmlEntities) {
					if (i + entity.length() <= corpusString.size() && corpusString.compare(i, entity.length(), entity) == 0) {

						corpusString.replace(i, entity.length(), std::string(1, ch));

						i += entity.length();
						break;
					}
				}
			}





			if (std::isalpha(c)) {
				word << static_cast<char>(std::tolower(c));
			} else {
				if (!word.str().empty()) {
					words.push_back(word.str());
					word.str("");
					word.clear();
				}
			}

			i++;
		}

		if (!word.str().empty()) {
			words.push_back(word.str());
		}

		return words;
	}


	static std::vector<std::vector<int>> tokenizeCorpus(const std::vector<std::string> &words, const trieNode* root) {

		std::vector<std::vector<int>> tokenizedWords(words.size());

		tokenizedWords.reserve(words.size());



		for (int i = 0; i < words.size(); i++) {

			const trieNode* node = root;

			int token = -1;
			int index = 0;

			for (int j = 0; j < words[i].size();) {

				if (node->children[words[i][j] - 'a'] != nullptr) {

					node = node->children[words[i][j] - 'a'];

					if (node->index != -1) {
						token = node->index;
						index = j;
					}

					j++;
				}

				else {
					tokenizedWords[i].push_back(token);
					j = index + 1;
					node = root;
				}
			}

			tokenizedWords[i].push_back(token);
		}

		return tokenizedWords;
	}


	static std::vector<std::pair<int, int>> createPairs(const std::vector<std::vector<int>> &tokenizedWords) {

		std::vector<std::pair<int, int>> pairs;


		pairs.reserve(tokenizedWords.size());


		for (const auto& i : tokenizedWords) {

			if (i.size() > 1) {

				for (int j = 0; j < i.size() - 1; j++) {

					pairs.emplace_back(i[j], i[j + 1] );
				}
			}
		}

		return pairs;
	}


	static void calculateFrequencies(const std::vector<std::pair<int, int>> &pairs, std::unordered_map<std::pair<int, int>, int, pair_hash> &frequencies) {

		for (auto& i : pairs) {
			frequencies[i]++;
		}

	}


	static std::vector<std::pair<std::pair<int, int>, int>> orderFrequencies(const std::unordered_map<std::pair<int, int>, int, pair_hash> &frequencies) {

		std::vector<std::pair<std::pair<int, int>, int>> orderedFrequencies;

		orderedFrequencies.reserve(frequencies.size());



		for (const auto&[fst, snd] : frequencies) {
			std::pair<std::pair<int, int>, int> pair;
			pair.first = fst;
			pair.second = snd;
			orderedFrequencies.push_back(pair);

		}


		auto comp = [](const std::pair<std::pair<int, int>, int> &a, const std::pair<std::pair<int, int>, int> &b) {

			return a.second > b.second;
		};



		std::ranges::partial_sort(orderedFrequencies, orderedFrequencies.begin() + 1,
		                          comp);


		return orderedFrequencies;
	}


	static void outputPair(const std::vector<std::string> &vocabulary, const std::vector<std::pair<std::pair<int, int>, int>> &orderedFrequencies) {

		std::ofstream vocabularyFileOut("../output/vocabulary.txt", std::ios::app);


		vocabularyFileOut << "\n" << vocabulary[orderedFrequencies[0].first.first] + vocabulary[orderedFrequencies[0].first.second];

	}


	static void deleteTrie(trieNode *node) {

		if (!node) {
			return;
		}

		for (const auto & i : node->children) {
			deleteTrie(i);
		}

		delete node;
	}





	void tokenize() const {

		for (int v = 0; v < 1000; v++) {

			auto runStart = std::chrono::high_resolution_clock::now();


			std::vector<std::string> vocabulary = loadVocabulary();





			auto* root = new trieNode();



			buildTrie(vocabulary, root);



			//std::ifstream corpusStream("/home/swann7777777/Documents/simplewiki-20250701-pages-articles-multistream.xml");
			std::ifstream corpusStream("/home/swann7777777/CLionProjects/SwaggTransformer/testCorpus.txt");




			std::unordered_map<std::pair<int, int>, int, pair_hash> frequencies;




			std::vector<std::thread> threads;

			int threadCount = std::thread::hardware_concurrency();

			std::mutex corpusMutex;
			std::mutex frequenciesMutex;


			for (int i = 0; i < threadCount; i++) {

				threads.emplace_back([&corpusStream, &root, &frequencies, &corpusMutex, &frequenciesMutex, this]() {


					std::string corpusString;

					while (true) {


						{
							std::lock_guard<std::mutex> lock(corpusMutex);
							if (!tokenizer::loadCorpus(corpusStream, corpusString)) {
								break;
							}
						}

						std::vector<std::string> words = tokenizer::normalizeCorpus(corpusString);




						std::vector<std::vector<int>> tokenizedWords = tokenizer::tokenizeCorpus(words, root);




						std::vector<std::pair<int, int>> pairs = tokenizer::createPairs(tokenizedWords);


						{
							std::lock_guard<std::mutex> lock(frequenciesMutex);
							tokenizer::calculateFrequencies(pairs, frequencies);
						}

						words.clear();
						tokenizedWords.clear();
						pairs.clear();

					}
				});
			}

			for (auto& t : threads) {
				t.join();
			}



			std::vector<std::pair<std::pair<int, int>, int>> orderedFrequencies = orderFrequencies(frequencies);


			auto runEnd = std::chrono::high_resolution_clock::now();


			std::chrono::duration<double> runtime = runEnd - runStart;



			outputPair(vocabulary, orderedFrequencies);



			deleteTrie(root);



			vocabulary.clear();
			frequencies.clear();
			orderedFrequencies.clear();

			std::cout << "runtime : " << runtime << "\n";
		}
	}

};

#endif //TOKENIZER_H
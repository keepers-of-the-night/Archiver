#include <windows.h>
#include <iostream>
#include <fstream>
#include <array>
#include <queue>
#include <vector>
#include <iomanip>
#include <cstdint>
#include <unordered_map>

using CodeTable = std::unordered_map<unsigned char, std::string>;

class BitWriter{
private:
    std::ofstream& out;
    unsigned char buffer;
    int bitCount;

public:
    BitWriter(std::ofstream& output) : out(output), buffer(0), bitCount(0){}

    void writeBit(int bit){
        buffer |= (bit & 1) << bitCount;
        bitCount++;
        if(bitCount == 8){
            flush();
        }
    }

    void writeCode(const std::string& code){
        for(char ch : code){
            writeBit(ch == '1' ? 1 : 0);
        }
    }

    void flush(){
        if(bitCount > 0){
            out.write(reinterpret_cast<const char*>(&buffer), 1);
            buffer = 0;
            bitCount = 0;
        }
    }
};

struct HuffmanNode{
    unsigned char symbol;
    int freq;
    HuffmanNode* left;
    HuffmanNode* right;

    HuffmanNode(unsigned char sym, int f)
        : symbol(sym), freq(f), left(nullptr), right(nullptr){}

    HuffmanNode(HuffmanNode* l, HuffmanNode* r)
        : symbol(0), freq(l->freq + r->freq), left(l), right(r){}

    bool isLeaf() const{
        return left == nullptr && right == nullptr;
    }
};

void generateCodes(HuffmanNode* node, const std::string& prefix, CodeTable& table){
    if(node == nullptr) return;

    if(node->isLeaf()){
        table[node->symbol] = prefix;
        return;
    }

    generateCodes(node->left,  prefix + "0", table);
    generateCodes(node->right, prefix + "1", table);
}

std::array<int, 256> countFrequencies(const std::string& filename){
    std::ifstream file(filename, std::ios::binary);
    if(!file.is_open()){
        throw std::runtime_error("Не удалось открыть файл: " + filename);
    }

    std::array<int, 256> freqMap {};

    const size_t BUFFER_SIZE = 1024 * 1024;
    std::vector<char> buffer(BUFFER_SIZE);

    while(file.read(buffer.data(), buffer.size())){
        for(size_t i = 0; i < buffer.size(); ++i){
            freqMap[static_cast<unsigned char>(buffer[i])]++;
        }
    }
    
    size_t lastCount = file.gcount();
    
    for(size_t i = 0; i < lastCount; ++i){
        freqMap[static_cast<unsigned char>(buffer[i])]++;
    }

    return freqMap;
}

HuffmanNode* buildHuffmanTree(const std::array<int, 256>& freqMap){
    
    auto rule = [](HuffmanNode* a, HuffmanNode* b){ return a->freq > b->freq; };
    
    std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, decltype(rule)> minHeap(rule);

    for(size_t i{0}; i < freqMap.size(); ++i){
        if(freqMap[i] > 0){
            minHeap.push(new HuffmanNode((unsigned char)i, freqMap[i]));
        }
    }

    if(minHeap.empty()){
        return nullptr;
    }

    while(minHeap.size() > 1){
        HuffmanNode* left = minHeap.top();
        minHeap.pop();
        HuffmanNode* right = minHeap.top();
        minHeap.pop();

        HuffmanNode* parent = new HuffmanNode(left, right);
        minHeap.push(parent);
    }

    return minHeap.top();
}

int main(int argc, char* argv[]){
    #ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    #endif

    if(argc != 3){
        std::cerr << "Использование: " << argv[0] << " <входной_файл> <выходной_сжатый>\n";
        return 1;
    }
    std::string inputFile = argv[1];
    std::string outputFile = argv[2];

    try{
        auto freqMap = countFrequencies(inputFile);

        HuffmanNode* root = buildHuffmanTree(freqMap);
        if(!root){
            std::cerr << "Файл пуст, сжатие невозможно.\n";
            return 1;
        }

        CodeTable codeTable;
        generateCodes(root, "", codeTable);

        std::ofstream outFile(outputFile, std::ios::binary);
        if(!outFile){
            throw std::runtime_error("Не удалось создать выходной файл");
        }

        uint32_t magic = 0x48464D4E;
        outFile.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        uint8_t version = 1;
        outFile.write(reinterpret_cast<const char*>(&version), sizeof(version));

        for(int freq : freqMap){
            outFile.write(reinterpret_cast<const char*>(&freq), sizeof(freq));
        }

        std::ifstream inFile(inputFile, std::ios::binary);
        if(!inFile){
            throw std::runtime_error("Не удалось повторно открыть входной файл");
        }

        BitWriter bitWriter(outFile);
        unsigned char byte;
        while (inFile.read(reinterpret_cast<char*>(&byte), 1)){
            auto it = codeTable.find(byte);
            if (it == codeTable.end()){
                throw std::runtime_error("Не найден код для символа");
            }
            bitWriter.writeCode(it->second);
        }

        bitWriter.flush();

        std::cout << "Сжатие завершено. Результат сохранён в " << outputFile << "\n";

    }catch (const std::exception& e){
        std::cerr << "Ошибка: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
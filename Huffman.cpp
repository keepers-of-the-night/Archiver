#include <windows.h>
#include <iostream>
#include <fstream>
#include <array>
#include <queue>
#include <vector>
#include <iomanip>
#include <cstdint>
#include <string>
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

class BitReader {
private:
    std::ifstream& in;
    unsigned char buffer;
    int bitPos;
public:
    BitReader(std::ifstream& input) : in(input), buffer(0), bitPos(8){}

    int readBit(){
        if(bitPos == 8){
            if(!in.read(reinterpret_cast<char*>(&buffer), 1)){
                return -1;
            }
            bitPos = 0;
        }
        int bit = (buffer >> bitPos) & 1;
        bitPos++;
        return bit;
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

void deleteTree(HuffmanNode* node){
    if(node == nullptr) return;
    deleteTree(node->left);
    deleteTree(node->right);
    delete node;
}

void compress(const std::string& inputFile, const std::string& outputFile){
    auto freqMap = countFrequencies(inputFile);
    HuffmanNode* root = buildHuffmanTree(freqMap);
    if(!root) throw std::runtime_error("Файл пуст, сжатие невозможно.");

    CodeTable codeTable;
    generateCodes(root, "", codeTable);

    std::ofstream outFile(outputFile, std::ios::binary);
    if(!outFile) throw std::runtime_error("Не удалось создать выходной файл");

    uint64_t originalSize = 0;
    for(int f : freqMap) originalSize += f;
    outFile.write(reinterpret_cast<const char*>(&originalSize), sizeof(originalSize));

    uint32_t magic = 0x48464D4E;
    outFile.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    uint8_t version = 1;
    outFile.write(reinterpret_cast<const char*>(&version), sizeof(version));

    for (int f : freqMap){
        outFile.write(reinterpret_cast<const char*>(&f), sizeof(f));
    }

    std::ifstream inFile(inputFile, std::ios::binary);
    if(!inFile) throw std::runtime_error("Не удалось повторно открыть входной файл");

    BitWriter bitWriter(outFile);
    unsigned char byte;
    while (inFile.read(reinterpret_cast<char*>(&byte), 1)){
        auto it = codeTable.find(byte);
        if(it == codeTable.end()) throw std::runtime_error("Не найден код для символа");
        bitWriter.writeCode(it->second);
    }
    bitWriter.flush();

    deleteTree(root);
    std::cout << "Сжатие завершено. Результат: " << outputFile << "\n";
}

void decompress(const std::string& inputFile, const std::string& outputFile){
    std::ifstream inFile(inputFile, std::ios::binary);
    if(!inFile) throw std::runtime_error("Не удалось открыть сжатый файл: " + inputFile);

    uint64_t originalSize;
    inFile.read(reinterpret_cast<char*>(&originalSize), sizeof(originalSize));
    if(!inFile) throw std::runtime_error("Ошибка чтения размера файла");

    uint32_t magic;
    inFile.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if(magic != 0x48464D4E) throw std::runtime_error("Неверное магическое число, файл не является сжатым Хаффманом");

    uint8_t version;
    inFile.read(reinterpret_cast<char*>(&version), sizeof(version));
    if(version != 1) throw std::runtime_error("Неподдерживаемая версия формата");

    std::array<int, 256> freqMap{};
    for(int i = 0; i < 256; ++i){
        inFile.read(reinterpret_cast<char*>(&freqMap[i]), sizeof(int));
        if(!inFile) throw std::runtime_error("Ошибка чтения таблицы частот");
    }

    HuffmanNode* root = buildHuffmanTree(freqMap);
    if(!root) throw std::runtime_error("Ошибка восстановления дерева");

    std::ofstream outFile(outputFile, std::ios::binary);
    if(!outFile) throw std::runtime_error("Не удалось создать выходной файл: " + outputFile);

    BitReader bitReader(inFile);
    HuffmanNode* current = root;
    uint64_t bytesWritten = 0;

    while(bytesWritten < originalSize){
        int bit = bitReader.readBit();
        if(bit == -1) throw std::runtime_error("Неожиданный конец файла при декодировании");

        if(bit == 0) current = current->left;
        else current = current->right;

        if(current->isLeaf()){
            outFile.write(reinterpret_cast<const char*>(&current->symbol), 1);
            bytesWritten++;
            current = root;
        }
    }

    deleteTree(root);
    std::cout << "Распаковка завершена. Восстановлено " << bytesWritten << " байт в " << outputFile << "\n";
}

int main(int argc, char* argv[]){
    #ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    #endif

    if(argc < 4){
        std::cerr << "Использование:\n"
                  << "  Сжатие:   " << argv[0] << " -c <входной> <выходной>\n"
                  << "  Распаковка: " << argv[0] << " -d <сжатый> <восстановленный>\n";
        return 1;
    }

    std::string mode = argv[1];
    std::string input = argv[2];
    std::string output = argv[3];

    try{
        if (mode == "-c"){
            compress(input, output);
        } else if (mode == "-d"){
            decompress(input, output);
        } else{
            std::cerr << "Неизвестный режим: " << mode << ". Используйте -c или -d.\n";
            return 1;
        }
    } catch(const std::exception& e){
        std::cerr << "Ошибка: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
#include <windows.h>
#include <iostream>
#include <fstream>
#include <array>
#include <queue>
#include <vector>
#include <iomanip>

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
    
    if(argc != 2){
        std::cerr << "Использование: " << argv[0] << " <имя_файла>\n";
        return 1;
    }

    std::string filename = argv[1];

    try{
        auto freqMap = countFrequencies(filename);

        HuffmanNode* root = buildHuffmanTree(freqMap);

        if(root == nullptr){
            std::cout << "Файл пуст или не содержит данных. Дерево не построено.\n";
            return 0;
        }


        std::cout << "\nДерево построено успешно!\n";

    }catch (const std::exception& e){
        std::cerr << "Ошибка: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
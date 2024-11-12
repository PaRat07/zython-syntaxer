import lexer;
#include <iostream>
#include <vector>

int main() {
    Lexer lexer("../../../huy.txt");
    std::vector<Lexem> result = lexer.Scan();

    for (auto& u : result) {
        u.Print();
    }    
}

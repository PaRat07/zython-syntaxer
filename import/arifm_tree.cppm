module;

#include <map>
#include <string>
#include <vector>

export module arifm_tree;

import lexem;
import tid;

export class ArifmTree {
  public:

    void Insert(const Lexem& lex, variable_type& type) {
      data_.emplace_back(lex, type);
    }
    void build() {
      root = buildTreeFromTokens(infixToPostfix(data_));
    }

  private:

    struct Data {
      Data(const Lexem lex, const variable_type& type) : lexem(lex), type(type) {}
      Lexem lexem;
      variable_type type;
    };

    std::vector<Data> data_;

    struct Node {
      Node(const Data& data) : data(data) {}
      Data data;
      std::unique_ptr<Node> left, right;
    };

    std::unique_ptr<Node> root;

    static int GetPriority(const Lexem oper) {
      static const std::map<std::string, int> priority = {
        { "+", 1 },
        { "-", 1 },
        { "*", 2 },
        { "/", 2 },
        { "%", 2 },
        { "**", 3 },
      };
      auto it = priority.find(oper.GetData());
      if (it == priority.end()) {
        return 0;
      }
      return it->second;
    }
    std::vector<Data> infixToPostfix(const std::vector<Data>& tokens) {
      std::vector<Data> operators;
      std::vector<Data> output;
      for (const Data& token : tokens) {
        if (GetPriority(token.lexem)) {
          output.push_back(token);
        } else if (token.lexem.GetData() == "(") {
          operators.push_back(token);
        } else if (token.lexem.GetData() == ")") {
          while (!operators.empty() && operators.back().lexem.GetData() != "(") {
            output.push_back(operators.back());
            operators.pop_back();
          }
          operators.pop_back();
        } else {
          while (!operators.empty() && GetPriority(operators.back().lexem) >= GetPriority(token.lexem)) {
            output.push_back(operators.back());
            operators.pop_back();
          }
          operators.push_back(token);
        }
      }
      while (!operators.empty()) {
        output.push_back(operators.back());
        operators.pop_back();
      }
      return output;
    }
    std::unique_ptr<Node> buildTreeFromTokens(const std::vector<Data>& tokens) {
      std::vector<std::unique_ptr<Node>> stack;
      for (const Data& token : tokens) {
        if (GetPriority(token.lexem)) {
          stack.push_back(std::make_unique<Node>(token));
        } else {
          auto node = std::make_unique<Node>(token);
          node->right = std::move(stack.back());
          stack.pop_back();
          node->left = std::move(stack.back());
          stack.pop_back();
          stack.push_back(std::move(node));
        }
      }
      return std::move(stack.back());
    }
};

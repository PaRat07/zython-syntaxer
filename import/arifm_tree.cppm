module;

#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <set>

export module arifm_tree;

import codegen;
import lexem;
import tid;

using namespace std::string_view_literals;
using namespace std::string_literals;

export class ArifmTree {
  public:

    void Insert(Lexem lex, Tid::Variable_Node type) {
        data_.emplace_back(std::make_unique<Node>(lex, type));
    }
    void Insert(Lexem lex, std::vector<ExprPtr> v) {
      data_.emplace_back(std::make_unique<func_node>(lex, std::move(v)));
    }

    void build() {
        auto u = infixToPostfix(data_);
        root = buildTreeFromTokens(u);
    }

    Tid::Variable_Node check() {
        return dfs(root.get());
    }

    Tid::Variable_Node GetLats() {
        auto u = std::move(data_.back()->type);
        data_.pop_back();
        return u;
    }

  private:
    static TypePtr getType(Lexem lex) {
      if (lex.GetType() == Lex::kIntLiter) {
        return std::move(std::make_unique<Integer>());
      }
      if (lex.GetType() == Lex::kFloatLiter) {
        return std::move(std::make_unique<Number>());
      }
    }

    static Lexem VarToLex(const variable_type& lex) {
      if (lex == variable_type::Integer) {
        return Lexem(Lex::kIntLiter, "");
      }
      if (lex == variable_type::Float) {
        return Lexem(Lex::kFloatLiter, "");
      }
    }

    struct Node {
      Node() = default;
      Node(Lexem lex) : lexem(lex) {}
      Node(Lexem& lex, Tid::Variable_Node type)
          : lexem(lex), type(type) {}
      Node(Node&&) noexcept = default;
      Node& operator=(Node&&) noexcept = default;

      virtual const ExprPtr& convert() const {
        if (lexem.GetType() == Lex::kId) {

        }
        if (lexem.GetType() == Lex::kIntLiter) {

        }
        if (lexem.GetType() == Lex::kFloatLiter) {

        }
      }

      Lexem lexem;
      Tid::Variable_Node type;

      std::unique_ptr<Node> left, right;
    };

    struct func_node : Node {
      func_node(Lexem lex, std::vector<ExprPtr> args) : Node(lex), args(std::move(args)) {}
      virtual ExprPtr convert() {

      }
      std::vector<ExprPtr> args;
    };

    std::vector<std::unique_ptr<Node>> data_;
    std::unique_ptr<Node> root;

    static int GetPriority(Lexem oper) {
        static std::map<std::string, int> priority = {
            { "+", 3 },
            { "-", 3 },
            { "*", 4 },
            { "/", 4 },
            { "%", 4 },
            { "**", 5 },
            { ">", 2 },
            { "<", 2 },
            { ">=", 2 },
            { "<=", 2 },
            { "==", 1 },
            { "!=", 1 },
            { "not", 1 },
            { "or", 1 },
            { "and", 1 },
        };
        if (oper.GetData() == "(" || oper.GetData() == ")") {
            return 1;
        }
        auto it = priority.find(oper.GetData());
        if (it == priority.end()) {
            return 0;
        }
        return it->second;
    }

    std::vector<std::unique_ptr<Node>> infixToPostfix(std::vector<std::unique_ptr<Node>>& tokens) {
        std::vector<std::unique_ptr<Node>> operators;
        std::vector<std::unique_ptr<Node>> output;

        for (auto& token : tokens) {
            if (!GetPriority(token->lexem)) {
                output.push_back(std::move(token));
            } else if (token->lexem.GetData() == "(") {
                operators.push_back(std::move(token));
            } else if (token->lexem.GetData() == ")") {
                while (!operators.empty() && operators.back()->lexem.GetData() != "(") {
                    output.push_back(std::move(operators.back()));
                    operators.pop_back();
                }
                if (!operators.empty()) {
                    operators.pop_back();
                }
            } else {
                while (!operators.empty() && GetPriority(operators.back()->lexem) >= GetPriority(token->lexem)) {
                    output.push_back(std::move(operators.back()));
                    operators.pop_back();
                }
                operators.push_back(std::move(token));
            }
        }
        while (!operators.empty()) {
            output.push_back(std::move(operators.back()));
            operators.pop_back();
        }

        for (auto& u : output) {
            std::cout << u->lexem.GetData() << " ";
        }
        std::cout << '\n';
        return output;
    }

    std::unique_ptr<Node> buildTreeFromTokens(std::vector<std::unique_ptr<Node>>& tokens) {
        std::vector<std::unique_ptr<Node>> stack;
        for (auto& token : tokens) {
            if (!GetPriority(token->lexem)) {
                stack.push_back(std::move(token));
            } else {
                auto node = std::unique_ptr<Node>(std::move(token));
                node->right = std::move(stack.back());
                stack.pop_back();
                if (!stack.empty()) {
                    node->left = std::move(stack.back());
                    stack.pop_back();
                }
                stack.push_back(std::move(node));
            }
        }
        return std::move(stack.back());
    }

    static bool IsLogicOperator(Lexem& oper) {
        static std::set st = {
            "=="sv, "!="sv, "<="sv, ">="sv, "<"sv, ">"sv, "not"sv, "and"sv, "or"sv
        };
        return st.contains(oper.GetData());
    }

    Tid::Variable_Node dfs(Node* node) {
        if (node == nullptr) {
            return Tid::Variable_Node("", variable_type::Undefined);
        }
        if (GetPriority(node->lexem) == 0) {
            return node->type;
        }
        auto f_type = dfs(node->left.get());
        auto s_type = dfs(node->right.get());
        if (f_type != s_type) {
            throw std::invalid_argument(std::format("type mismatch"));
        }
        if (IsLogicOperator(node->lexem)) {
            return Tid::Variable_Node("", variable_type::Integer);
        }
        return f_type;
    }
};

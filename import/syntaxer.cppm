module;

#include <cassert>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <ranges>
#include <vector>
#include <span>
#include <set>


#define OPTIMIZING_ASSERT(cond) \
  assert(cond);                 \
  [[assume(cond)]]

export module syntaxer;

import lexer;
import lexem;
import arifm_tree;

import codegen;

import tid;


using namespace std::string_view_literals;
using namespace std::string_literals;

template <>
struct std::formatter<decltype(std::declval<Lexem>().GetPosition())> {
  constexpr auto parse(std::format_parse_context& ctx) {
    return ctx.begin();
  }

  constexpr auto format(const auto& obj, std::format_context& ctx) const {
    return std::format_to(ctx.out(), "{}:{}", obj.line, obj.index);
  }
};


std::string_view ToString(Lex lex) {
  switch (lex) {
    case Lex::kId: {
      return "identifier"sv;
    }
    case Lex::kKeyworkd: {
      return "keyword"sv;
    }
    case Lex::kSeparator: {
      return "separator"sv;
    }
    case Lex::kOperator: {
      return "operator"sv;
    }
    case Lex::kEndLine: {
      return "endline"sv;
    }
    case Lex::kFloatLiter: {
      return "float"sv;
    }
    case Lex::kIntLiter: {
      return "int"sv;
    }
    case Lex::kStringLiter: {
      return "string"sv;
    }
  }
}

export class SyntaxValidator {
 public:
  SyntaxValidator(const std::string& filename) {
    auto lexes_own = Lexer(filename, "token.txt").Scan();
    bool prev_endline = false;
    std::vector<Lexem> lexes_filtered;
    for (auto lex : lexes_own) {
      if (lex.GetType() == Lex::kEndLine) {
        prev_endline = true;
        lexes_filtered.push_back(lex);
      } else {
        if (prev_endline || lex.GetType() != Lex::kSeparator) {
          if (lex.GetType() == Lex::kKeyworkd && (lex.GetData() == "not" || lex.GetData() == "and" || lex.GetData() == "or")) {
            lexes_filtered.emplace_back(Lex::kOperator, std::string(lex.GetData()), lex.GetPosition().line, lex.GetPosition().index);
          } else {
            lexes_filtered.push_back(lex);
          }
        }
        prev_endline = false;
      }
    }
    lexes_ = lexes_filtered;
    Program();
    if (!lexes_.empty()) {
      throw std::invalid_argument(std::format("Unexpected tabulation at {}", lexes_[0].GetPosition()));
    }
  }

 private:

  Tid tid;

  std::span<Lexem> lexes_;
  int in_cycle_ = 0;
  size_t cur_indent_ = 0;
  std::vector<Tid::Function_Node*> tec_func = { nullptr };

  void NewScope() {
    tid.NewScope();
    cur_indent_ += 4;
  }

  void CloseScope() {
    tid.CloseScope();
    cur_indent_ -= 4;
  }

  void Program() {
    Tid::Variable_Node* prev_id = nullptr;
    while (!lexes_.empty()) {
      while (lexes_.at(0).GetType() == Lex::kEndLine) {
        SkipLexem(Lex::kEndLine);
      }
      if (SpacesAmount() != cur_indent_) {
        return;
      }
      if (lexes_.at(0).GetType() == Lex::kSeparator) {
        SkipLexem(Lex::kSeparator);
      }
      if (lexes_.at(0).GetType() == Lex::kEndLine) {
        SkipLexem(Lex::kEndLine);
      } else if (lexes_.at(0).GetType() == Lex::kKeyworkd) {
        if (lexes_.at(0).GetData() == "def") {
          DeclFunc();
        } else if (lexes_.at(0).GetData() == "if") {
          IfElse();
        } else if (lexes_.at(0).GetData() == "while") {
          While();
        } else if (lexes_.at(0).GetData() == "match") {
          MatchCase();
        } else if (lexes_.at(0).GetData() == "break") {
          if (in_cycle_ == 0) {
            throw std::runtime_error(std::format("SyntaxError: break out of cycle at {}", lexes_.at(0).GetData()));
          }
          SkipLexem(Lex::kKeyworkd, "break");
          SkipLexem(Lex::kEndLine);
        } else if (lexes_.at(0).GetData() == "pass") {
          SkipLexem(Lex::kKeyworkd, "pass");
          SkipLexem(Lex::kEndLine);
        } else if (lexes_.at(0).GetData() == "continue") {
          if (in_cycle_ == 0) {
            throw std::runtime_error(std::format("SyntaxError: break out of cycle at {}", lexes_[0].GetPosition()));
          }
          SkipLexem(Lex::kKeyworkd, "continue");
          SkipLexem(Lex::kEndLine);
        } else if (lexes_.at(0).GetData() == "return") {
          if (tec_func.back() == nullptr) {
            throw std::invalid_argument(std::format("SyntaxError: return out of func at {}", lexes_.at(0).GetPosition()));
          }
          SkipLexem(Lex::kKeyworkd, "return");
          auto func_type = Expression().first;
          if (func_type.type != tec_func.back()->return_value.type && tec_func.back()->return_value.type != variable_type::Undefined) {
            throw std::invalid_argument(std::format("type mismatch: cannot be combined return values {} and {}, at {}",
              Tid::ToValueString(func_type.type), Tid::ToValueString(tec_func.back()->return_value.type), lexes_.at(0).GetPosition()));
          }
          if (func_type.type == variable_type::Array && func_type.in_array_type == variable_type::Undefined) {
            throw std::invalid_argument(std::format("the returned array cannot contain an undefined type, at {}",
              lexes_.at(0).GetPosition()));
          }
          tec_func.back()->return_value = func_type;
        } else {
          throw std::invalid_argument(std::format("SyntaxError: expected code, got {}", lexes_.at(0).GetData()));
        }
      } else {
        bool is_func = false;
        if (lexes_.at(0).GetType() == Lex::kId) {
          if (lexes_.at(1).GetData() != "(") {
            prev_id = tid.FindVariable(lexes_.at(0).GetData());
            if (prev_id == nullptr) {
              if (lexes_.at(1).GetData() == "=") {
                tid.InsertVariable(Tid::Variable_Node(lexes_.at(0).GetData()));
                prev_id = tid.FindVariable(lexes_.at(0).GetData());
                SkipLexem(Lex::kId);
                SkipLexem(Lex::kOperator);
              }
            }
          } else {
            if (!tid.FindFunction(lexes_.at(0).GetData())) {
              throw std::invalid_argument(std::format("undeclared function {}, at {}",
                lexes_.at(0).GetData(), lexes_.at(0).GetPosition()));
            }
            is_func = true;
            ArifmTree tree;
            SkipParams(tree);
            SkipLexem(lexes_.at(0).GetType(), ")");
          }
        }
        if (!is_func) {
          auto [type, root] = Expression();
          if (prev_id != nullptr) {
            if (type.type == variable_type::Undefined) {
              throw std::invalid_argument("type mismatch: Undefined Expression");
            }
            if (type != *prev_id && prev_id->type != variable_type::Undefined) {
              throw std::invalid_argument(std::format("type mismatch, at {}", lexes_.at(0).GetPosition()));
            }
            Assignment(prev_id->name, std::move(root)).Evaluate(std::cout, "");
            prev_id->type = type.type;
            prev_id->in_array_type = type.in_array_type;
            prev_id->array_dimensions = type.array_dimensions;
            prev_id = nullptr;
          }
          SkipLexem(Lex::kEndLine);
        }
      }
    }
  }

  void MatchCase() {
    SkipLexem(Lex::kKeyworkd, "match");
    auto type = Expression().first;
    SkipLexem(Lex::kKeyworkd, ":");
    SkipLexem(Lex::kEndLine);
    NewScope();
    while (SpacesAmount() == cur_indent_) {
      SkipLexem(Lex::kSeparator);
      if (lexes_.at(0).GetType() != Lex::kKeyworkd || lexes_.at(0).GetData() != "case") {
        throw std::invalid_argument(std::format("Expected case at {}, got \"{}\"", lexes_[0].GetPosition(), lexes_.at(0).GetData()));
      }
      SkipLexem(Lex::kKeyworkd, "case");
      auto case_type = Expression().first;
      if (case_type != type) {
        throw std::invalid_argument(std::format("Expected type {}, got {} at {}",
          Tid::ToValueString(case_type.type), Tid::ToValueString(type.type), lexes_.at(0).GetPosition()));
      }
      SkipLexem(Lex::kKeyworkd, ":");
      NewScope();
      Program();
      CloseScope();
    }
    CloseScope();
  }

  static variable_type GetType(Lexem const& lex) {
    if (lex.GetType() == Lex::kIntLiter) {
      return variable_type::Integer;
    }
    if (lex.GetType() == Lex::kFloatLiter) {
      return variable_type::Float;
    }
    if (lex.GetType() == Lex::kStringLiter) {
      return variable_type::String;
    }
    return variable_type::Undefined;
  }

  bool CheckTypeCast(variable_type& from, variable_type& to) {
    static std::set nums = {
      variable_type::Float, variable_type::Integer
    };
    if (nums.contains(from) && nums.contains(to)) {
      return true;
    }
    return to == variable_type::String;
  }

  static variable_type TypeFromCastString(const std::string& str) {
    static const std::unordered_map<std::string, variable_type> actions = {
      {"int", variable_type::Integer },
      {"float", variable_type::Float},
      {"str", variable_type::String}
    };
    auto it = actions.find(str);
    if (it == actions.end()) {
      return variable_type::Undefined;
    }
    return it->second;
  }

  Tid::Variable_Node ParsMas() {
    Tid::Variable_Node node;
    node.type = variable_type::Array;
    node.array_dimensions = 1;
    SkipLexem(lexes_.at(0).GetType(), "[");
    while (lexes_.at(0).GetData() != "]") {
      if (lexes_.at(0).GetData() == "[") {
        auto type = ParsMas();
        if (node.in_array_type == variable_type::Undefined) {
          node.in_array_type = type.in_array_type;
          node.array_dimensions = type.array_dimensions + 1;
        } else if (node.in_array_type != type.in_array_type
          || node.array_dimensions != type.array_dimensions + 1) {
          throw std::invalid_argument(std::format("Mas type error at {}", lexes_.at(0).GetPosition()));
        }

      } else {
        auto type = Expression(true).first;
        if (node.in_array_type == variable_type::Undefined) {
          if (type.type == variable_type::Array) {
            node.in_array_type = type.in_array_type;
            node.array_dimensions = type.array_dimensions + 1;
          } else {
            node.in_array_type = type.type;
          }
        } else if (type.type != node.in_array_type) {
          throw std::invalid_argument(std::format("Mas type error at {}", lexes_.at(0).GetPosition()));
        }
      }
      if (lexes_.at(0).GetData() == ",") {
        SkipLexem(lexes_.at(0).GetType(), ",");
      }
    }
    SkipLexem(lexes_.at(0).GetType(), "]");
    return node;
  }

  std::pair<Tid::Variable_Node, ExprPtr> Expression(bool in_func = false) {
    ArifmTree tree;
    int cur_brace_balance = in_func;
    bool prev_operator = true;
    bool prev_dot = false;

    static std::set bin_op = {
      "*"sv, "/"sv, "%"sv, "<"sv, ">"sv, "=="sv, "!="sv, "<="sv, ">="sv, "="sv, "**"sv, "//"sv, "."sv, ","sv, "or"sv, "and"sv
    };
    static std::set un_op = {
      "+"sv, "-"sv, "not"sv
    };
    static std::set open_bracket = {
      "("sv, "["sv, "{"sv
    };
    static std::set close_bracket = {
      ")"sv, "]"sv, "}"sv
    };
    static std::set type_values = {
      Lex::kId, Lex::kFloatLiter, Lex::kStringLiter, Lex::kIntLiter
    };
    static std::set cast_keywords = {
      "int"sv, "float"sv, "str"sv
    };
    bool is_prev_id = false;
    while (lexes_.at(0).GetType() != Lex::kKeyworkd && lexes_.at(0).GetType() != Lex::kEndLine) {
      if (lexes_.at(0).GetData() == "," && in_func) {
        break;
      }
      if (prev_dot && lexes_.at(0).GetType() != Lex::kId) {
        throw std::invalid_argument(std::format("SyntaxError: expected identifier after operator dot at {}", lexes_.at(0).GetPosition()));
      }
      bool is_skip_mas = false;
      bool is_insert = false;
      if (lexes_.at(0).GetType() == Lex::kOperator) {
        if (open_bracket.contains(lexes_.at(0).GetData())) {
          if (lexes_.at(0).GetData() == "[") {
            if (!is_prev_id) {
              auto type = ParsMas();
              tree.Insert(Lexem(Lex::kId, "id",
                lexes_.at(0).GetPosition().line, lexes_.at(0).GetPosition().index), type);
              is_skip_mas = true;
              is_insert = true;
            } else {
              auto type = tree.GetLats();
              while (lexes_.at(0).GetData() == "[") {
                SkipLexem(lexes_.at(0).GetType(), "[");
                auto tp = Expression(true).first;
                if (tp.type != variable_type::Integer) {
                  throw std::invalid_argument(std::format("Not integer index at", lexes_.at(0).GetPosition()));
                }
                SkipLexem(lexes_.at(0).GetType(), "]");
                type.array_dimensions -= 1;
              }
              if (type.array_dimensions < 0) {
                throw std::invalid_argument(std::format("too few measurements at {}", lexes_.at(0).GetPosition()));
              }
              tree.Insert(Lexem(Lex::kId, "id",
                lexes_.at(0).GetPosition().line, lexes_.at(0).GetPosition().index), type);
              is_skip_mas = true;
              is_insert = true;
            }
            prev_operator = false;
          } else {
            prev_operator = true;
            ++cur_brace_balance;
          }
        } else if (close_bracket.contains(lexes_.at(0).GetData())) {
          if (prev_operator) {
            throw std::invalid_argument(std::format("SyntaxError: invalid expression at {}: closing brace after operator", lexes_.at(0).GetPosition()));
          }
          prev_operator = false;
          --cur_brace_balance;
          if (in_func && cur_brace_balance == 0) {
            break;
          }
          if (cur_brace_balance < 0) {
            throw std::invalid_argument(std::format("SyntaxError: extra brace at {}", lexes_.at(0).GetPosition()));
          }
        } else if (un_op.contains(lexes_.at(0).GetData())) {
          prev_operator = true;
        } else if (bin_op.contains(lexes_.at(0).GetData())) {
          if (prev_operator) {
            throw std::invalid_argument(std::format("SyntaxError: invalid expression at {}: two operators in a row", lexes_.at(0).GetPosition()));
          }
          prev_operator = true;
        } else {
          OPTIMIZING_ASSERT(false);
        }
      } else {
        if (!prev_operator) {
          throw std::invalid_argument(std::format("SyntaxError: invalid expression at {}: two expressions in a row", lexes_.at(0).GetPosition()));
        }
        prev_operator = false;
      }
      if (type_values.contains(lexes_.at(0).GetType())) {
        is_prev_id = true;
        if (lexes_.at(0).GetType() == Lex::kId && cast_keywords.contains(lexes_.at(0).GetData())) {
          auto to_cast_type = TypeFromCastString(lexes_.at(0).GetData());

          auto lex = lexes_.at(0);

          SkipLexem(Lex::kId, lexes_.at(0).GetData());
          SkipLexem(Lex::kOperator, "(");
          auto from_cast_type = Expression(true).first;
          if (!CheckTypeCast(from_cast_type.type, to_cast_type)) {
            throw std::invalid_argument(std::format("type mismatch: cannot cast from {} to {}, at {}",
              Tid::ToValueString(from_cast_type.type), Tid::ToValueString(to_cast_type), lexes_.at(0).GetPosition()));
          }
          tree.Insert(lex, Tid::Variable_Node("p", to_cast_type));
        } else if (lexes_.at(0).GetType() == Lex::kId) {
          if (lexes_.at(1).GetData() != "(") {
            auto val = tid.FindVariable(lexes_.at(0).GetData());
            if (!val) {
              throw std::invalid_argument(std::format("undeclared variable {}, at {}",
                                           lexes_.at(0).GetData(), lexes_.at(0).GetPosition()));
            }
            is_insert = true;
            tree.Insert(lexes_.at(0), *val);
          } else{
            auto func = tid.FindFunction(lexes_.at(0).GetData());
            if (!func) {
              throw std::invalid_argument(std::format("undeclared func {}, at {}",
                                           lexes_.at(0).GetData(), lexes_.at(0).GetPosition()));
            }

            is_insert = true;

            SkipParams(tree);
          }
        } else {
          tree.Insert(lexes_.at(0), Tid::Variable_Node("", GetType(lexes_.at(0))));
          is_insert = true;
        }
      } else {
        is_prev_id = false;
      }
      if (!is_insert) {
        tree.Insert(lexes_.at(0), Tid::Variable_Node("", GetType(lexes_.at(0))));
      }
      prev_dot = lexes_.at(0).GetData() == ".";
      if (!is_skip_mas) {
        SkipLexem(lexes_.at(0).GetType());
      }
    }
    if (cur_brace_balance > 0 && !(in_func && cur_brace_balance == 1)) {
      throw std::invalid_argument(std::format("SyntaxError: need extra brace at {}", lexes_.at(0).GetPosition()));
    }
    tree.build();
    return std::pair(tree.check(), tree.BuildAst());
  }

  void SkipParams(ArifmTree& tree) {
    auto func = tid.FindFunction(lexes_.at(0).GetData());
    SkipLexem(lexes_.at(0).GetType());
    SkipLexem(lexes_.at(0).GetType(), "(");
    int ind = 0;
    while (lexes_.at(0).GetType() != Lex::kEndLine &&
      lexes_.at(0).GetType() != Lex::kKeyworkd) {
      if (lexes_.at(0).GetType() == Lex::kOperator && lexes_.at(0).GetData() != "[") {
        break;
      }
      auto type = Expression(true).first;
      if (ind >= func->parameters.size()) {
        throw std::invalid_argument(std::format("expected {} parameters, received {}, at {}",
          std::to_string(func->parameters.size()), ind + 1, lexes_.at(0).GetPosition()));
      }
      if (type != func->parameters[ind]) {
        throw std::invalid_argument(std::format("type mismatch: cannot be combined {} and {}, at {}",
          Tid::ToValueString(type.type), Tid::ToValueString(func->parameters[ind].type), lexes_.at(0).GetPosition()));
      }
      if (lexes_.at(0).GetData() == ",") {
        SkipLexem(lexes_.at(0).GetType());
      }
      ++ind;
    }
    if (ind != func->parameters.size()) {
      throw std::invalid_argument(std::format("expected {} parameters, received {}, at {}",
          std::to_string(func->parameters.size()), ind, lexes_.at(0).GetPosition()));
    }
    tree.Insert(lexes_.at(0), func->return_value);
  }

  void IfElse() {
    SkipLexem(Lex::kKeyworkd, "if");
    auto type = Expression().first;
    if (type.type != variable_type::Integer) {
      throw std::invalid_argument(std::format("the type of expression in the condition is not equal Integer, {}, at {}",
        Tid::ToValueString(type.type), lexes_.at(0).GetPosition()));
    }
    SkipLexem(Lex::kKeyworkd, ":");
    SkipLexem(Lex::kEndLine);
    NewScope();
    Program();
    CloseScope();
    if (!lexes_.empty() && lexes_.at(0).GetType() == Lex::kKeyworkd && lexes_.at(0).GetData() == "else") {
      SkipLexem(Lex::kKeyworkd, "else");
      SkipLexem(Lex::kKeyworkd, ":");
      NewScope();
      Program();
      CloseScope();
    }
  }

  void DeclFunc() {
    SkipLexem(Lex::kKeyworkd, "def");

    if (tid.FindFunction(lexes_.at(0).GetData())) {
      throw std::invalid_argument(std::format("function override {}, at {}",
        lexes_.at(0).GetData(), lexes_.at(0).GetPosition()));
    }

    tid.InsertFunction(Tid::Function_Node(lexes_.at(0).GetData()));
    tec_func.push_back(tid.FindFunction(lexes_.at(0).GetData()));

    NewScope();

    SkipLexem(Lex::kId);
    SkipLexem(Lex::kOperator, "(");
    while (lexes_.at(0).GetType() != Lex::kOperator ||
           lexes_.at(0).GetData() != ")") {
      auto name = lexes_.at(0).GetData();
      SkipLexem(Lex::kId, lexes_.at(0).GetData());
      SkipLexem(Lex::kKeyworkd, ":");
      auto type = Tid::TypeFromString(lexes_.at(0).GetData());
      if (tid.FindVariable(name)) {
        throw std::invalid_argument(std::format("variable redefinition {}, at {}",
          name, lexes_.at(0).GetPosition()));
      }
      SkipLexem(Lex::kId, lexes_.at(0).GetData());
      if (lexes_.at(0).GetData() != "[") {
        tid.InsertVariable(Tid::Variable_Node(name, type));
        tec_func.back()->parameters.emplace_back(name, type);
      } else {
        auto t = ParsMas();
        t.name = name;
        t.in_array_type = type;
        tid.InsertVariable(t);
        tec_func.back()->parameters.emplace_back(t);
      }
      if (lexes_.at(0).GetData() == ",") {
        SkipLexem(Lex::kOperator, ",");
      }
    }
    SkipLexem(Lex::kOperator, ")");
    SkipLexem(Lex::kKeyworkd, ":");
    SkipLexem(Lex::kEndLine);
    Program();
    CloseScope();
    tec_func.pop_back();
  }

  void While() {
    SkipLexem(Lex::kKeyworkd, "while");
    auto type = Expression().first;
    if (type.type != variable_type::Integer) {
      throw std::invalid_argument(std::format("the type of expression in the condition is not equal Integer, {}, at {}",
        Tid::ToValueString(type.type), lexes_.at(0).GetPosition()));
    }
    SkipLexem(Lex::kKeyworkd, ":");
    SkipLexem(Lex::kEndLine);
    NewScope();
    ++in_cycle_;
    Program();
    --in_cycle_;
    CloseScope();
  }

  void SkipLexem(Lex type) {
    OPTIMIZING_ASSERT(type != Lex::kKeyworkd);
    if (type == Lex::kId) {
      if (!tid.FindVariable(lexes_.at(0).GetData()) && !tid.FindFunction(lexes_.at(0).GetData())) {
        throw std::invalid_argument(std::format("uninitialized variable {} at {}",
          lexes_.at(0).GetData(), lexes_.at(0).GetPosition()));
      }
    }
    if (lexes_.at(0).GetType() != type) {
      throw std::invalid_argument(std::format(
          "expected {}, got {} at {}", ToString(type), lexes_[0].GetData(), lexes_[0].GetPosition()));
    }
    lexes_ = lexes_.subspan(1);
  }

  void SkipLexem(Lex type, std::string_view data) {
    if (lexes_.at(0).GetType() != type || lexes_.at(0).GetData() != data) {
      throw std::invalid_argument(std::format(
          "expected {}, got {} at {}", ToString(type), lexes_[0].GetData(), lexes_[0].GetPosition()));
    }
    lexes_ = lexes_.subspan(1);
  }

  size_t SpacesAmount() {
    return lexes_.at(0).GetType() == Lex::kSeparator ? lexes_.at(0).GetData().size() : 0;
  }
};

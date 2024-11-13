module;

#include <cassert>
#include <fstream>
#include <iostream>
#include <ranges>
#include <span>

export module syntaxer;

import lexer;
import lexem;

#define OPTIMIZING_ASSERT(cond) \
  assert(cond);                 \
  [[assume(cond)]]

using namespace std::string_view_literals;

export class SyntaxValidator {
 public:
  SyntaxValidator(std::string_view filename) {
    auto lexes_own = Lexer(std::string(filename)).Scan();
    bool prev_endline = false;
    std::vector<Lexem> lexes_filtered;
    for (auto lex : lexes_own) {
      if (lex.GetType() == Lex::kEndLine) {
        prev_endline = true;
        lexes_filtered.push_back(lex);
      } else {
        if (prev_endline || lex.GetType() != Lex::kSeparator) {
          if (lex.GetType() == Lex::kKeyworkd && (lex.GetData() == "not" || lex.GetData() == "and" || lex.GetData() == "or")) {
            lexes_filtered.emplace_back(Lex::kOperator, std::string(lex.GetData()));
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
      throw std::invalid_argument("Unexpected tabulation");
    }
    std::println("All ok");
  }

 private:
  std::span<Lexem> lexes_;
  int in_cycle_ = 0;
  size_t cur_indent_ = 0;

  void Program() {
    while (!lexes_.empty()) {
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
            throw std::runtime_error("SyntaxError: break out of cycle");
          }
          SkipLexem(Lex::kKeyworkd, "break");
          SkipLexem(Lex::kEndLine);
        } else if (lexes_.at(0).GetData() == "pass") {
          SkipLexem(Lex::kKeyworkd, "pass");
          SkipLexem(Lex::kEndLine);
        } else if (lexes_.at(0).GetData() == "continue") {
          if (in_cycle_ == 0) {
            throw std::runtime_error("SyntaxError: break out of cycle");
          }
          SkipLexem(Lex::kKeyworkd, "continue");
          SkipLexem(Lex::kEndLine);
        } else {
          throw std::invalid_argument(std::format("Am i stupid?: {}", lexes_.at(0).GetData()));
        }
      } else {
        Expression();
        SkipLexem(Lex::kEndLine);
      }
    }
  }

  void MatchCase() {
    SkipLexem(Lex::kKeyworkd, "match");
    Expression();
    SkipLexem(Lex::kKeyworkd, ":");
    cur_indent_ += 4;
    while (lexes_.at(0).GetType() == Lex::kKeyworkd && lexes_.at(0).GetData() == "case") {
      SkipLexem(Lex::kKeyworkd, "case");
      Expression();
      SkipLexem(Lex::kKeyworkd, ":");
      cur_indent_ += 4;
      Program();
      cur_indent_ -= 4;
    }
    cur_indent_ -= 4;
    SkipLexem(Lex::kEndLine);
  }

  void Expression() {
    while (lexes_.at(0).GetType() != Lex::kKeyworkd && lexes_.at(0).GetType() != Lex::kEndLine) {
      SkipLexem(lexes_.at(0).GetType());
    }
  }

  void IfElse() {
    SkipLexem(Lex::kKeyworkd, "if");
    Expression();
    SkipLexem(Lex::kKeyworkd, ":");
    SkipLexem(Lex::kEndLine);
    cur_indent_ += 4;
    Program();
    cur_indent_ -= 4;
    if (lexes_.at(0).GetType() == Lex::kKeyworkd && lexes_.at(0).GetData() == "else") {
      SkipLexem(Lex::kKeyworkd, "else");
      SkipLexem(Lex::kKeyworkd, ":");
      cur_indent_ += 4;
      Program();
      cur_indent_ -= 4;
    }
  }

  void DeclFunc() {
    SkipLexem(Lex::kKeyworkd, "def");
    SkipLexem(Lex::kId);
    SkipLexem(Lex::kOperator, "(");
    while (lexes_.at(0).GetType() != Lex::kOperator ||
           lexes_.at(0).GetData() != ")") {
      SkipParam();
      SkipLexem(Lex::kOperator, ",");
    }
    SkipLexem(Lex::kOperator, ")");
    SkipLexem(Lex::kKeyworkd, ":");
    SkipLexem(Lex::kEndLine);
    cur_indent_ += 4;
    std::println("Caller");
    Program();
    cur_indent_ -= 4;
  }

  void While() {
    SkipLexem(Lex::kKeyworkd, "while");
    Expression();
    SkipLexem(Lex::kKeyworkd, ":");
    SkipLexem(Lex::kEndLine);
    cur_indent_ += 4;
    ++in_cycle_;
    Program();
    --in_cycle_;
    cur_indent_ -= 4;
  }

  // void SkipWhiteSpaces() {
  //
  // }

  void SkipParam() {
    SkipLexem(Lex::kId);
    SkipLexem(Lex::kKeyworkd, ":");
    SkipLexem(Lex::kId);
  }

  void SkipLexem(Lex type) {
    OPTIMIZING_ASSERT(type != Lex::kKeyworkd);
    if (lexes_.at(0).GetType() != type) {
      throw std::invalid_argument(std::format(
          "expected {}, got {}", LexToString(type), lexes_[0].GetData()));
    }
    lexes_ = lexes_.subspan(1);
  }

  void SkipLexem(Lex type, std::string_view data) {
    if (lexes_.at(0).GetType() != type || lexes_.at(0).GetData() != data) {
      throw std::invalid_argument(std::format(
          "expected {}, got {}", LexToString(type), lexes_[0].GetData()));
    }
    lexes_ = lexes_.subspan(1);
  }

  size_t SpacesAmount() {
    return lexes_.at(0).GetType() == Lex::kSeparator ? lexes_.at(0).GetData().size() : 0;
  }
};

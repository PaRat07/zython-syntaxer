module;

#include <experimental/iterator>
#include <algorithm>
#include <exception>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <print>
#include <ranges>
#include <span>
#include <sstream>
#include <stack>
#include <string_view>
#include <vector>

export module ast_builder;

import lexem;

template<size_t Sz>
struct StringLiteral {
  consteval StringLiteral(char src[Sz]) {
    std::ranges::copy(src, data);
  }

  char data[Sz];
};

std::string GetUniqueId() {
  static size_t cur_ind = 0;
  return std::format("{}uniq", cur_ind++);
}

struct TypeI {
  virtual auto TypeId() const -> std::size_t = 0;
  virtual auto Typename() const -> std::string = 0;
  virtual ~TypeI() = default;
};

using TypePtr = std::unique_ptr<TypeI>;

struct Void : TypeI {
  static const TypePtr kPtr;
  static constexpr size_t id = 2;

  virtual auto TypeId() const -> std::size_t override { return id; }

  virtual auto Typename() const -> std::string override { return "void"; }
};

const TypePtr Void::kPtr = std::make_unique<Void>();

struct Integer : TypeI {
  static const TypePtr kPtr;
  static constexpr size_t id = 1;

  virtual auto TypeId() const -> std::size_t override { return id; }

  virtual auto Typename() const -> std::string override { return "i32"; }
};

const TypePtr Integer::kPtr = std::make_unique<Integer>();

struct Number : TypeI {
  static const TypePtr kPtr;
  static constexpr size_t id = 2;
  virtual auto TypeId() const -> std::size_t override { return id; }

  virtual auto Typename() const -> std::string override { return "float"; }
};

const TypePtr Number::kPtr = std::make_unique<Number>();

struct ExpressionI {
  virtual auto GetResultType() const -> const TypePtr& = 0;
  virtual ~ExpressionI() = default;
  virtual void Evaluate(std::ostream&, std::string_view to_reg) const = 0;

  std::string continue_label;
  std::string break_label;
};

using ExprPtr = std::unique_ptr<ExpressionI>;

struct IntegerLiteral : ExpressionI {
  int value;
  auto GetResultType() const -> const TypePtr& override {
    return Integer::kPtr;
  }
  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    std::println(out, "{} = add i32 0, {}", to_reg, value);
  }
};

struct FloatLiteral : ExpressionI {
  float value;
  auto GetResultType() const -> const TypePtr& override {
    return Integer::kPtr;
  }
  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    std::println(out, "{} = fadd float 0, {}", to_reg, value);
  }
};

struct VariableDecl final : ExpressionI {
  std::string name;
  TypePtr type;
  virtual auto GetResultType() const -> const TypePtr& override { return type; }
  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    std::println(out, "{} = alloca {} {}", to_reg, type->Typename(), name);
  }
};

struct VariableUse final : ExpressionI {
  std::string name;
  TypePtr type;
  virtual auto GetResultType() const -> const TypePtr& override { return type; }
  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    std::println(out, "{} = load {}* {}", to_reg, type->Typename(), name);
  }
};

struct BinaryOp : ExpressionI {
  BinaryOp(ExprPtr l, ExprPtr r) : left(std::move(l)), right(std::move(r)) {}

  ExprPtr left, right;
};

struct Subtract final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> const TypePtr& override {
    return left->GetResultType();
  }

  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueId();
    auto rbuf_name = GetUniqueId();
    left->Evaluate(out, lbuf_name);
    right->Evaluate(out, rbuf_name);
    std::println(out, "{} = {} {} {} {}", to_reg,
        (left->GetResultType()->Typename() == "i32" ? "sub" : "fsub"),
        left->GetResultType()->Typename(), lbuf_name, rbuf_name);
  }
};

struct Add final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> const TypePtr& override {
    return left->GetResultType();
  }
  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueId();
    auto rbuf_name = GetUniqueId();
    left->Evaluate(out, lbuf_name);
    right->Evaluate(out, rbuf_name);
    out << std::format(
        "{} = {} {} {} {}\n", to_reg,
        (left->GetResultType()->Typename() == "i32" ? "add" : "fadd"),
        left->GetResultType()->Typename(), lbuf_name, rbuf_name);
  }
};

struct Divide final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> const TypePtr& override {
    return left->GetResultType();
  }

  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueId();
    auto rbuf_name = GetUniqueId();
    if (left->GetResultType()->TypeId() == Number::id) {
      left->Evaluate(out, lbuf_name);
      right->Evaluate(out, rbuf_name);
    } else {
      auto intlbuf_name = GetUniqueId();
      auto intrbuf_name = GetUniqueId();

      left->Evaluate(out, intlbuf_name);
      right->Evaluate(out, intrbuf_name);

      std::println(out, "{} = sitofp i32 {} to float", lbuf_name, intlbuf_name);
      std::println(out, "{} = sitofp i32 {} to float", lbuf_name, intlbuf_name);
    }
    std::print(out, "{} = {} {} {} {}\n", to_reg, "fsub",
               left->GetResultType()->Typename(), lbuf_name, rbuf_name);
  }
};

struct DividAndRound final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> const TypePtr& override {
    return left->GetResultType();
  }

  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueId();
    auto rbuf_name = GetUniqueId();
    left->Evaluate(out, lbuf_name);
    right->Evaluate(out, rbuf_name);
    if (left->GetResultType()->TypeId() == Integer::id) {
      std::println(out, "{} = sdiv i32 {}, {}", to_reg, lbuf_name, rbuf_name);
    } else {
      auto ansbuf_name = GetUniqueId();
      std::println(out, "{} = {} {} {} {}", to_reg, "fdiv",
                   left->GetResultType()->Typename(), lbuf_name, rbuf_name);
      std::print(out, "{} = fptosi float {} to i32", to_reg, ansbuf_name);
    }
  }
};

struct Multiply final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> const TypePtr& override {
    return left->GetResultType();
  }
  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueId();
    auto rbuf_name = GetUniqueId();
    left->Evaluate(out, lbuf_name);
    right->Evaluate(out, rbuf_name);
    out << std::format(
        "{} = {} {} {} {}\n", to_reg,
        (left->GetResultType()->Typename() == "i32" ? "mul" : "fmul"),
        left->GetResultType()->Typename(), lbuf_name, rbuf_name);
  }
};

struct Break : ExpressionI {
  auto GetResultType() const -> const TypePtr& override { return Void::kPtr; }

  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    std::println(out, "br label {}", break_label);
  }
};

template<StringLiteral CmpType>
struct Comparison final : BinaryOp {
  auto GetResultType() const -> const TypePtr& override {
    return Integer::kPtr;
  }

  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    auto left_buf_name = GetUniqueId();
    auto right_buf_name = GetUniqueId();
    left->Evaluate(out, left_buf_name);
    right->Evaluate(out, right_buf_name);
    auto res_buf_name = GetUniqueId();
    std::println(out, "{} = icmp {} {} {}, {}", res_buf_name, CmpType.data, left->GetResultType()->Typename(), left_buf_name, right_buf_name);
    std::println(out, "{} = sext i1 {} to i32", to_reg, res_buf_name);
  }
};

struct ReturnSttmnt : ExpressionI {
  ExprPtr value;

  auto GetResultType() const -> const TypePtr& override { return Void::kPtr; }

  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    auto ret_buf = GetUniqueId();
    value->Evaluate(out, ret_buf);
    std::println(out, "ret {} {}", value->GetResultType()->Typename(), ret_buf);
  }
};

struct FunctionDecl final : ExpressionI {
  std::string name;
  TypePtr return_type;
  std::vector<ExprPtr> exprs;
  std::vector<std::pair<std::string, TypePtr>> args;

  void Evaluate(std::ostream& out, std::string_view) const override {
    std::println(out, "define dso_local {} {}({:n}){{", return_type->Typename(),
                 name,
                 args | std::views::transform([](auto&& p) {
                   return std::format("{} {}", p.second->Typename(), p.first);
                 }));
    for (auto&& i : exprs) {
      i->Evaluate(out, GetUniqueId());
    }
    out << "}\n";
  }
  auto GetResultType() const -> const TypePtr& override {
    return Void::kPtr;
  }
};

std::map<std::string, FunctionDecl> func_table;

struct FunctionInv final : ExpressionI {
  std::vector<ExprPtr> args;
  std::string func_name;

  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    std::vector<std::string> reg_names(args.size());
    for (auto &i : reg_names) {
      i = GetUniqueId();
    }
    for (const auto &[reg, expr] : std::views::zip(reg_names, args)) {
      expr->Evaluate(out, reg);
    }
    if (func_table[func_name].return_type->Typename() == "void") {
      std::println(out, "call void {}({:n})", func_name, reg_names);
    } else {
      std::println(out, "{} = call {} {}({:n})", to_reg,
                   func_table[func_name].return_type->Typename(), func_name, reg_names);
    }
  }

  auto GetResultType() const -> const TypePtr& override {
    return func_table[func_name].return_type;
  }
};

struct Assignment : ExpressionI {
  std::string var_name;
  ExprPtr value;

  auto GetResultType() const -> const TypePtr& override {
    return value->GetResultType();
  }

  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    auto buf_name = GetUniqueId();
    value->Evaluate(out, buf_name);
    std::println(out, "store {} {}, {}* {}", value->GetResultType()->Typename(),
                 buf_name, value->GetResultType()->Typename(), var_name);
    std::println(out, "{} = load {}, {}* {}", to_reg,
                 value->GetResultType()->Typename(),
                 value->GetResultType()->Typename(), var_name);
  }
};

struct Cycle : ExpressionI {
  ExprPtr cond;
  std::vector<ExprPtr> body;

  auto GetResultType() const -> const TypePtr& override { return Void::kPtr; }

  void Evaluate(std::ostream& out, std::string_view) const override {
    auto again_name = GetUniqueId();
    std::println(out, "{}:", again_name);
    auto res_name = GetUniqueId();
    cond->Evaluate(out, res_name);
    auto cond_name = GetUniqueId();
    std::println(out, "{} = icmp ne i32 0, {}", cond_name, res_name);
    auto body_label_name = GetUniqueId();
    auto break_label_name = GetUniqueId();
    std::println(out, "br i1 {}, label {}, label {}", cond_name, body_label_name, break_label_name);
    std::println(out, "{}:", body_label_name);
    auto continue_label_name = GetUniqueId();
    for (auto &&i : body) {
      i->break_label = break_label_name;
      i->continue_label = continue_label_name;
      i->Evaluate(out, GetUniqueId());
    }
    std::println(out, "{}:", continue_label_name);
    std::println(out, "br label {}", again_name);
    std::println(out, "{}:", break_label_name);
  }
};

enum class IdType { kFuncion, kVariable };

// FIXME idk
std::map<std::string, std::stack<ExprPtr>> tid;

void Translate(std::ostream& out, std::span<Lexem> lexemes) {
  for (auto&& i : lexemes) {
    switch (i.GetType()) {
      case Lex::kKeyworkd: {
        if (i.GetData() == "def") {
        }
      }
    }
  }
}

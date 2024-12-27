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

export module codegen;

import lexem;

std::string GetUniqueRegister() {
  static size_t cur_ind = 0;
  return std::format("%{}reg", cur_ind++);
}

std::string GetUniqueLabel() {
  static size_t cur_ind = 0;
  return std::format("{}lbl", cur_ind++);
}

export struct TypeI {
  virtual auto TypeId() const -> std::size_t = 0;
  virtual auto Typename() const -> std::string = 0;
  virtual ~TypeI() = default;
};

export using TypePtr = std::unique_ptr<TypeI>;

export struct Void : TypeI {
  static const TypePtr kPtr;
  static constexpr size_t id = 2;

  virtual auto TypeId() const -> std::size_t override { return id; }

  virtual auto Typename() const -> std::string override { return "void"; }
};

const TypePtr Void::kPtr = std::make_unique<Void>();

export struct Integer : TypeI {
  static const TypePtr kPtr;
  static constexpr size_t id = 1;

  virtual auto TypeId() const -> std::size_t override { return id; }

  virtual auto Typename() const -> std::string override { return "i32"; }
};

const TypePtr Integer::kPtr = std::make_unique<Integer>();

export struct Number : TypeI {
  static const TypePtr kPtr;
  static constexpr size_t id = 2;
  virtual auto TypeId() const -> std::size_t override { return id; }

  virtual auto Typename() const -> std::string override { return "float"; }
};

const TypePtr Number::kPtr = std::make_unique<Number>();

export struct ExpressionI {
  virtual auto GetResultType() const -> const TypePtr& = 0;
  virtual ~ExpressionI() = default;
  virtual void Evaluate(std::ostream&, std::string_view to_reg) const = 0;

  std::string continue_label;
  std::string break_label;
};

export using ExprPtr = std::unique_ptr<ExpressionI>;

export struct IntegerLiteral : ExpressionI {
  explicit IntegerLiteral(int value) : value(value) {}
  int value;
  auto GetResultType() const -> const TypePtr& override {
    return Integer::kPtr;
  }
  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    std::println(out, "{} = add i32 0, {}", to_reg, value);
  }
};

export struct FloatLiteral : ExpressionI {
  explicit FloatLiteral(float value) : value(value) {}
  float value;
  auto GetResultType() const -> const TypePtr& override {
    return Integer::kPtr;
  }
  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    std::println(out, "{} = fadd float 0, {}", to_reg, value);
  }
};

export struct VariableDecl final : ExpressionI {
  VariableDecl(std::string name, TypePtr type)
      : name(std::move(name)), type(std::move(type)) {}
  std::string name;
  TypePtr type;
  virtual auto GetResultType() const -> const TypePtr& override { return type; }
  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    std::println(out, "{} = alloca {} {}", to_reg, type->Typename(), name);
  }
};

export struct VariableUse final : ExpressionI {
  VariableUse(std::string name, TypePtr type)
      : name(std::move(name)), type(std::move(type)) {}
  std::string name;
  TypePtr type;
  virtual auto GetResultType() const -> const TypePtr& override { return type; }
  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    std::println(out, "{} = load {}* {}", to_reg, type->Typename(), name);
  }
};

export struct BinaryOp : ExpressionI {
  BinaryOp(ExprPtr l, ExprPtr r) : left(std::move(l)), right(std::move(r)) {}

  ExprPtr left, right;
};

export struct Subtract final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> const TypePtr& override {
    return left->GetResultType();
  }

  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueRegister();
    auto rbuf_name = GetUniqueRegister();
    left->Evaluate(out, lbuf_name);
    right->Evaluate(out, rbuf_name);
    std::println(out, "{} = {} {} {} {}", to_reg,
        (left->GetResultType()->Typename() == "i32" ? "sub" : "fsub"),
        left->GetResultType()->Typename(), lbuf_name, rbuf_name);
  }
};

export struct Add final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> const TypePtr& override {
    return left->GetResultType();
  }
  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueRegister();
    auto rbuf_name = GetUniqueRegister();
    left->Evaluate(out, lbuf_name);
    right->Evaluate(out, rbuf_name);
    out << std::format(
        "{} = {} {} {} {}\n", to_reg,
        (left->GetResultType()->Typename() == "i32" ? "add" : "fadd"),
        left->GetResultType()->Typename(), lbuf_name, rbuf_name);
  }
};

export struct Divide final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> const TypePtr& override {
    return left->GetResultType();
  }

  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueRegister();
    auto rbuf_name = GetUniqueRegister();
    if (left->GetResultType()->TypeId() == Number::id) {
      left->Evaluate(out, lbuf_name);
      right->Evaluate(out, rbuf_name);
    } else {
      auto intlbuf_name = GetUniqueRegister();
      auto intrbuf_name = GetUniqueRegister();

      left->Evaluate(out, intlbuf_name);
      right->Evaluate(out, intrbuf_name);

      std::println(out, "{} = sitofp i32 {} to float", lbuf_name, intlbuf_name);
      std::println(out, "{} = sitofp i32 {} to float", lbuf_name, intlbuf_name);
    }
    std::print(out, "{} = {} {} {} {}\n", to_reg, "fsub",
               left->GetResultType()->Typename(), lbuf_name, rbuf_name);
  }
};

export struct DividAndRound final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> const TypePtr& override {
    return left->GetResultType();
  }

  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueRegister();
    auto rbuf_name = GetUniqueRegister();
    left->Evaluate(out, lbuf_name);
    right->Evaluate(out, rbuf_name);
    if (left->GetResultType()->TypeId() == Integer::id) {
      std::println(out, "{} = sdiv i32 {}, {}", to_reg, lbuf_name, rbuf_name);
    } else {
      auto ansbuf_name = GetUniqueRegister();
      std::println(out, "{} = {} {} {} {}", to_reg, "fdiv",
                   left->GetResultType()->Typename(), lbuf_name, rbuf_name);
      std::print(out, "{} = fptosi float {} to i32", to_reg, ansbuf_name);
    }
  }
};

export struct Multiply final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> const TypePtr& override {
    return left->GetResultType();
  }
  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueRegister();
    auto rbuf_name = GetUniqueRegister();
    left->Evaluate(out, lbuf_name);
    right->Evaluate(out, rbuf_name);
    out << std::format(
        "{} = {} {} {} {}\n", to_reg,
        (left->GetResultType()->Typename() == "i32" ? "mul" : "fmul"),
        left->GetResultType()->Typename(), lbuf_name, rbuf_name);
  }
};

export struct Break : ExpressionI {
  auto GetResultType() const -> const TypePtr& override { return Void::kPtr; }

  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    std::println(out, "br label {}", break_label);
  }
};

export struct Equal final : BinaryOp {
  using BinaryOp::BinaryOp;

  auto GetResultType() const -> const TypePtr& override {
    return Integer::kPtr;
  }

  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    auto left_buf_name = GetUniqueRegister();
    auto right_buf_name = GetUniqueRegister();
    left->Evaluate(out, left_buf_name);
    right->Evaluate(out, right_buf_name);
    auto res_buf_name = GetUniqueRegister();
    std::println(out, "{} = {} {} {} {}, {}",
                 res_buf_name,
                 (left->GetResultType()->TypeId() == Integer::id ? "icmp" : "fcmp"),
                 (left->GetResultType()->TypeId() == Integer::id ? "eq" : "oeq"),
                 left->GetResultType()->Typename(),
                 left_buf_name,
                 right_buf_name);
    std::println(out, "{} = sext i1 {} to i32", to_reg, res_buf_name);
  }
};

export struct NotEqual final : BinaryOp {
  using BinaryOp::BinaryOp;

  auto GetResultType() const -> const TypePtr& override {
    return Integer::kPtr;
  }

  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    auto left_buf_name = GetUniqueRegister();
    auto right_buf_name = GetUniqueRegister();
    left->Evaluate(out, left_buf_name);
    right->Evaluate(out, right_buf_name);
    auto res_buf_name = GetUniqueRegister();
    std::println(out, "{} = {} {} {} {}, {}",
                 res_buf_name,
                 (left->GetResultType()->TypeId() == Integer::id ? "icmp" : "fcmp"),
                 (left->GetResultType()->TypeId() == Integer::id ? "neq" : "one"),
                 left->GetResultType()->Typename(),
                 left_buf_name,
                 right_buf_name);
    std::println(out, "{} = sext i1 {} to i32", to_reg, res_buf_name);
  }
};

export struct Greater final : BinaryOp {
  using BinaryOp::BinaryOp;
  auto GetResultType() const -> const TypePtr& override {
    return Integer::kPtr;
  }

  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    auto left_buf_name = GetUniqueRegister();
    auto right_buf_name = GetUniqueRegister();
    left->Evaluate(out, left_buf_name);
    right->Evaluate(out, right_buf_name);
    auto res_buf_name = GetUniqueRegister();
    std::println(out, "{} = {} {} {} {}, {}",
                 res_buf_name,
                 (left->GetResultType()->TypeId() == Integer::id ? "icmp" : "fcmp"),
                 (left->GetResultType()->TypeId() == Integer::id ? "sgt" : "ogt"),
                 left->GetResultType()->Typename(),
                 left_buf_name,
                 right_buf_name);
    std::println(out, "{} = sext i1 {} to i32", to_reg, res_buf_name);
  }
};

export struct GreaterOrEqual final : BinaryOp {
  using BinaryOp::BinaryOp;

  auto GetResultType() const -> const TypePtr& override {
    return Integer::kPtr;
  }

  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    auto left_buf_name = GetUniqueRegister();
    auto right_buf_name = GetUniqueRegister();
    left->Evaluate(out, left_buf_name);
    right->Evaluate(out, right_buf_name);
    auto res_buf_name = GetUniqueRegister();
    std::println(out, "{} = {} {} {} {}, {}",
                 res_buf_name,
                 (left->GetResultType()->TypeId() == Integer::id ? "icmp" : "fcmp"),
                 (left->GetResultType()->TypeId() == Integer::id ? "sge" : "oge"),
                 left->GetResultType()->Typename(),
                 left_buf_name,
                 right_buf_name);
    std::println(out, "{} = sext i1 {} to i32", to_reg, res_buf_name);
  }
};

export struct Less final : BinaryOp {
  using BinaryOp::BinaryOp;

  auto GetResultType() const -> const TypePtr& override {
    return Integer::kPtr;
  }

  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    auto left_buf_name = GetUniqueRegister();
    auto right_buf_name = GetUniqueRegister();
    left->Evaluate(out, left_buf_name);
    right->Evaluate(out, right_buf_name);
    auto res_buf_name = GetUniqueRegister();
    std::println(out, "{} = {} {} {} {}, {}",
                 res_buf_name,
                 (left->GetResultType()->TypeId() == Integer::id ? "icmp" : "fcmp"),
                 (left->GetResultType()->TypeId() == Integer::id ? "slt" : "olt"),
                 left->GetResultType()->Typename(),
                 left_buf_name,
                 right_buf_name);
    std::println(out, "{} = sext i1 {} to i32", to_reg, res_buf_name);
  }
};

export struct LessOrEqual final : BinaryOp {
  using BinaryOp::BinaryOp;

  auto GetResultType() const -> const TypePtr& override {
    return Integer::kPtr;
  }

  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    auto left_buf_name = GetUniqueRegister();
    auto right_buf_name = GetUniqueRegister();
    left->Evaluate(out, left_buf_name);
    right->Evaluate(out, right_buf_name);
    auto res_buf_name = GetUniqueRegister();
    std::println(out, "{} = {} {} {} {}, {}",
                 res_buf_name,
                 (left->GetResultType()->TypeId() == Integer::id ? "icmp" : "fcmp"),
                 (left->GetResultType()->TypeId() == Integer::id ? "sle" : "ole"),
                 left->GetResultType()->Typename(),
                 left_buf_name,
                 right_buf_name);
    std::println(out, "{} = sext i1 {} to i32", to_reg, res_buf_name);
  }
};


export struct ReturnSttmnt : ExpressionI {
  explicit ReturnSttmnt(ExprPtr value) : value(std::move(value)) {}
  ExprPtr value;

  auto GetResultType() const -> const TypePtr& override { return Void::kPtr; }

  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    auto ret_buf = GetUniqueRegister();
    value->Evaluate(out, ret_buf);
    std::println(out, "ret {} {}", value->GetResultType()->Typename(), ret_buf);
  }
};

export struct FunctionDecl final : ExpressionI {
  FunctionDecl(std::string name, TypePtr return_type,
               std::vector<ExprPtr> exprs,
               std::vector<std::pair<std::string, TypePtr>> args)
      : name(std::move(name)),
        return_type(std::move(return_type)),
        exprs(std::move(exprs)),
        args(std::move(args)) {}
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
      i->Evaluate(out, GetUniqueRegister());
    }
    out << "}\n";
  }
  auto GetResultType() const -> const TypePtr& override {
    return Void::kPtr;
  }
};

export std::map<std::string, FunctionDecl> func_table;

export struct FunctionInv final : ExpressionI {
  FunctionInv(std::vector<ExprPtr> args, std::string func_name)
      : args(std::move(args)), func_name(std::move(func_name)) {}
  std::vector<ExprPtr> args;
  std::string func_name;

  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    std::vector<std::string> reg_names(args.size());
    for (auto &i : reg_names) {
      i = GetUniqueRegister();
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

export struct Assignment : ExpressionI {
  Assignment(std::string var_name, ExprPtr value)
      : var_name(std::move(var_name)), value(std::move(value)) {}
  std::string var_name;
  ExprPtr value;

  auto GetResultType() const -> const TypePtr& override {
    return value->GetResultType();
  }

  void Evaluate(std::ostream& out, std::string_view to_reg) const override {
    auto buf_name = GetUniqueRegister();
    value->Evaluate(out, buf_name);
    std::println(out, "store {} {}, {}* {}", value->GetResultType()->Typename(),
                 buf_name, value->GetResultType()->Typename(), var_name);
    std::println(out, "{} = load {}, {}* {}", to_reg,
                 value->GetResultType()->Typename(),
                 value->GetResultType()->Typename(), var_name);
  }
};

export struct Cycle : ExpressionI {
  Cycle(ExprPtr cond_val, std::vector<ExprPtr> body_val)
    : cond(std::move(cond_val)),
      body(std::move(body_val)) {}

  ExprPtr cond;
  std::vector<ExprPtr> body;

  auto GetResultType() const -> const TypePtr& override { return Void::kPtr; }

  void Evaluate(std::ostream& out, std::string_view) const override {
    auto again_name = GetUniqueLabel();
    std::println(out, "{}:", again_name);
    auto res_name = GetUniqueRegister();
    cond->Evaluate(out, res_name);
    auto cond_name = GetUniqueRegister();
    std::println(out, "{} = icmp ne i32 0, {}", cond_name, res_name);
    auto body_label_name = GetUniqueLabel();
    auto break_label_name = GetUniqueLabel();
    std::println(out, "br i1 {}, label {}, label {}", cond_name, body_label_name, break_label_name);
    std::println(out, "{}:", body_label_name);
    auto continue_label_name = GetUniqueLabel();
    for (auto &&i : body) {
      i->break_label = break_label_name;
      i->continue_label = continue_label_name;
      i->Evaluate(out, GetUniqueRegister());
    }
    std::println(out, "{}:", continue_label_name);
    std::println(out, "br label {}", again_name);
    std::println(out, "{}:", break_label_name);
  }
};

export enum class IdType { kFuncion, kVariable };

// FIXME idk
export std::map<std::string, std::stack<ExprPtr>> tid;

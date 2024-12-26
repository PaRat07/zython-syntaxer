module;

#include <format>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>
#include <stack>
#include <exception>
#include <algorithm>
#include <ostream>
#include <ranges>
#include <print>
#include <experimental/iterator>

export module ast_builder;

import lexem;


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
  static constexpr size_t id = 2;

  virtual auto TypeId() const -> std::size_t override {
    return id;
  }

  virtual auto Typename() const -> std::string override {
    return "void";
  }
};

struct Integer : TypeI {
  static constexpr size_t id = 1;

  virtual auto TypeId() const -> std::size_t override {
    return id;
  }

  virtual auto Typename() const -> std::string override {
    return "float";
  }
};

struct Number : TypeI {
  static constexpr size_t id = 2;
  virtual auto TypeId() const -> std::size_t override {
    return id;
  }

  virtual auto Typename() const -> std::string override {
    return "i32";
  }
};

struct ExpressionI {
  struct Getter {
    const ExpressionI *data;
    std::string_view store_to;
  };

  virtual auto GetResultType() const -> const TypePtr& = 0;
  virtual ~ExpressionI() = default;
  virtual void Evaluate(std::ostream&, std::string_view to_reg) const = 0;

  Getter Evaluate() const {
    return { this };
  }

  struct {
    uint64_t line, index;
  } pos;
};

using ExprPtr = std::unique_ptr<ExpressionI>;

std::ostream &operator<<(std::ostream &out, ExpressionI::Getter expr) {
  expr.data->Evaluate(out, expr.store_to);
  return out;
}

// FIXME idk
std::map<std::string, std::stack<ExprPtr>> tid;


struct Variable : ExpressionI {
  std::string name;
  TypePtr type;
  virtual auto GetResultType() const -> const TypePtr& override {
    return type;
  }
};

struct BinaryOp : ExpressionI {
  BinaryOp(ExprPtr l, ExprPtr r)
    : left(std::move(l)),
      right(std::move(r)) {
    if (l->GetResultType()->TypeId() != r->GetResultType()->TypeId()) {
      throw std::logic_error(std::format("Operands have incompatible type({} and {})", l->GetResultType()->Typename(), r->GetResultType()->Typename()));
    }
  }

  ExprPtr left, right;
};


struct Subtract final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> const TypePtr& override {
    return left->GetResultType();
  }

  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueId();
    auto rbuf_name = GetUniqueId();
    left->Evaluate(out, lbuf_name);
    right->Evaluate(out, rbuf_name);
    out << std::format("{} = {} {} {} {}\n",
      to_reg,
      (left->GetResultType()->Typename() == "i32" ? "sub" : "fsub"),
      left->GetResultType()->Typename(),
      lbuf_name,
      rbuf_name);
  }

  void Set(std::ostream&, std::string_view from_reg) const override {
    throw std::logic_error("Can't assign to rvalue");
  }
};

struct Add final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> const TypePtr& override {
    return left->GetResultType();
  }
  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueId();
    auto rbuf_name = GetUniqueId();
    left->Evaluate(out, lbuf_name);
    right->Evaluate(out, rbuf_name);
    out << std::format("{} = {} {} {} {}\n",
      to_reg,
      (left->GetResultType()->Typename() == "i32" ? "add" : "fsub"),
      left->GetResultType()->Typename(),
      lbuf_name,
      rbuf_name);
  }

  void Set(std::ostream&, std::string_view from_reg) const override {
    throw std::logic_error("Can't assign to rvalue");
  }
};

struct Divide final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> const TypePtr& override {
    return left->GetResultType();
  }

  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
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
    std::print(out, "{} = {} {} {} {}\n",
      to_reg,
      "fsub",
      left->GetResultType()->Typename(),
      lbuf_name,
      rbuf_name);
  }

  void Set(std::ostream&, std::string_view from_reg) const override {
    throw std::logic_error("Can't assign to rvalue");
  }
};

struct DividAndRound final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> const TypePtr& override {
    return left->GetResultType();
  }

  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueId();
    auto rbuf_name = GetUniqueId();
    left->Evaluate(out, lbuf_name);
    right->Evaluate(out, rbuf_name);
    if (left->GetResultType()->TypeId() == Integer::id) {
      std::println(out, "{} = sdiv i32 {}, {}", to_reg, lbuf_name, rbuf_name);
    } else {
      auto ansbuf_name = GetUniqueId();
      std::println(out, "{} = {} {} {} {}",
        to_reg,
        "fdiv",
        left->GetResultType()->Typename(),
        lbuf_name,
        rbuf_name);
      std::print(out, "{} = fptosi float {} to i32", to_reg, ansbuf_name);
    }
  }
};

struct Multiply final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> const TypePtr& override {
    return left->GetResultType();
  }
  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueId();
    auto rbuf_name = GetUniqueId();
    left->Evaluate(out, lbuf_name);
    right->Evaluate(out, rbuf_name);
    out << std::format("{} = {} {} {} {}\n",
      to_reg,
      (left->GetResultType()->Typename() == "i32" ? "mul" : "fmul"),
      left->GetResultType()->Typename(),
      lbuf_name,
      rbuf_name);
  }
};


struct StatementI {
  virtual void Get(std::ostream &out) const = 0;
  virtual ~StatementI() = default;
};

struct ReturnSttmnt : StatementI {
  ExprPtr value;
  void Get(std::ostream &out) const override {
    out << "ret " << value->GetResultType()->Typename() << " " << value->Evaluate() << "\n";
  }
};

struct FunctionDecl final : ExpressionI {
  TypePtr return_type;
  std::vector<ExprPtr> exprs;
  std::vector<std::pair<std::string, TypePtr>> args;

  void Set(std::ostream& out, std::string_view set_to) const override {
    out << "define dso_local " << return_type->Typename() << " @" << set_to
        << "(";
    for (bool first = true; auto&& [name, type] : args) {
      if (first) {
        first = false;
      } else {
        out << ", ";
      }
      out << std::format("{} %{}", type->Typename(), name);
    }
    out << ") {\n";
    for (auto&& i : exprs) {
      out << i->Evaluate() << "\n";
    }
    out << "}\n";
  }

  auto GetResultType() const -> const TypePtr& override {
    return return_type;
  }
  void Evaluate(std::ostream&, std::string_view to_reg) const override {
    throw std::logic_error("Can't get value from function declaration");
  }
};

std::map<std::string, FunctionDecl> func_table;

struct FunctionInv final : ExpressionI {
  std::vector<ExprPtr> args;
  std::string func_name;

  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    if (func_table[func_name].GetResultType()->Typename() == "void") {
      std::print(out, "call void {}({:n})", func_name, args | std::views::transform([] (const ExprPtr &expr) {
        return expr->Evaluate();
      }));
    } else {
      std::print(out, "{} = {} call void {}({:n})",
        to_reg,
        func_table[func_name].GetResultType()->Typename(),
        func_name,
        args
          | std::views::transform([] (const ExprPtr &expr) {
            return expr->Evaluate();
          })
      );
    }
  }

  auto GetResultType() const -> const TypePtr& override {
    return func_table[func_name].GetResultType();
  }

  void Set(std::ostream &out, std::string_view from_reg) const override {
    throw std::logic_error("Can't assign to rvalue(function invocation)");
  }
};

struct Assignment : ExpressionI {
  std::string var_name;
  ExprPtr value;

  auto GetResultType() const -> const TypePtr& override {
    return value->GetResultType();
  }

  void Evaluate(std::ostream &out, std::string_view to_reg) const override {
    auto buf_name = GetUniqueId();
    value->Evaluate(out, buf_name);
    std::println(out, "store {} {}, {}* {}", value->GetResultType()->Typename(), buf_name, value->GetResultType(), var_name);
    std::println(out, "{} = load {}, {}* %{}", to_reg, value->GetResultType()->Typename(), value->GetResultType()->Typename(), var_name);
  }
};

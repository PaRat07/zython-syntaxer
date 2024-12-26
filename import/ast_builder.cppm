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

  struct Setter {
    const ExpressionI *data;
    std::string_view load_from;
  };

  virtual auto GetResultType() const -> const TypePtr& = 0;
  virtual ~ExpressionI() = default;
  virtual void Get(std::ostream&, std::string_view to_reg) const = 0;
  virtual void Set(std::ostream&, std::string_view from_reg) const = 0;

  Getter Get() const {
    return { this };
  }
  Setter Set() const {
    return { this };
  }

  struct {
    uint64_t line, index;
  } pos;
};

using ExprPtr = std::unique_ptr<ExpressionI>;

std::ostream &operator<<(std::ostream &out, ExpressionI::Getter expr) {
  expr.data->Get(out, expr.store_to);
  return out;
}

std::ostream &operator<<(std::ostream &out, ExpressionI::Setter expr) {
  expr.data->Set(out, expr.load_from);
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

  void Get(std::ostream &out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueId();
    auto rbuf_name = GetUniqueId();
    left->Get(out, lbuf_name);
    right->Get(out, rbuf_name);
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
  void Get(std::ostream &out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueId();
    auto rbuf_name = GetUniqueId();
    left->Get(out, lbuf_name);
    right->Get(out, rbuf_name);
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

  void Get(std::ostream &out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueId();
    auto rbuf_name = GetUniqueId();
    if (left->GetResultType()->TypeId() == Number::id) {
      left->Get(out, lbuf_name);
      right->Get(out, rbuf_name);
    } else {
      auto intlbuf_name = GetUniqueId();
      auto intrbuf_name = GetUniqueId();

      left->Get(out, intlbuf_name);
      right->Get(out, intrbuf_name);

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

  void Get(std::ostream &out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueId();
    auto rbuf_name = GetUniqueId();
    left->Get(out, lbuf_name);
    right->Get(out, rbuf_name);
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

  void Set(std::ostream&, std::string_view from_reg) const override {
    throw std::logic_error("Can't assign to rvalue");
  }
};

struct Multiply final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> const TypePtr& override {
    return left->GetResultType();
  }
  void Get(std::ostream &out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueId();
    auto rbuf_name = GetUniqueId();
    left->Get(out, lbuf_name);
    right->Get(out, rbuf_name);
    out << std::format("{} = {} {} {} {}\n",
      to_reg,
      (left->GetResultType()->Typename() == "i32" ? "mul" : "fmul"),
      left->GetResultType()->Typename(),
      lbuf_name,
      rbuf_name);
  }

  void Set(std::ostream&, std::string_view from_reg) const override {
    throw std::logic_error("Can't assign to rvalue");
  }
};


struct StatementI {
  virtual void Get(std::ostream &out) const = 0;
  virtual ~StatementI() = default;
};

struct ReturnSttmnt : StatementI {
  ExprPtr value;
  void Get(std::ostream &out) const override {
    out << "ret " << value->GetResultType()->Typename() << " " << value->Get() << "\n";
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
      out << i->Get() << "\n";
    }
    out << "}\n";
  }

  auto GetResultType() const -> const TypePtr& override {
    return return_type;
  }
  void Get(std::ostream&, std::string_view to_reg) const override {
    throw std::logic_error("Can't get value from function declaration");
  }
};

std::map<std::string, FunctionDecl> func_table;

struct FunctionInv final : ExpressionI {
  std::vector<ExprPtr> args;
  std::string func_name;

  void Get(std::ostream &out, std::string_view to_reg) const override {
    if (func_table[func_name].GetResultType()->Typename() == "void") {
      std::print(out, "call void {}({:n})", func_name, args | std::views::transform([] (const ExprPtr &expr) {
        return expr->Get();
      }));
    } else {
      std::print(out, "{} = {} call void {}({:n})",
        to_reg,
        func_table[func_name].GetResultType()->Typename(),
        func_name,
        args
          | std::views::transform([] (const ExprPtr &expr) {
            return expr->Get();
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

};

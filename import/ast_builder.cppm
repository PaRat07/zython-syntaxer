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

template<typename U, typename V>
static const std::unique_ptr<U> kPtrToType = std::make_unique<V>();

struct Void : TypeI {
  virtual auto TypeId() const -> std::size_t override {
    return 0;
  }

  virtual auto Typename() const -> std::string override {
    return "void";
  }
};

struct Integer : TypeI {
  virtual auto TypeId() const -> std::size_t override {
    return 1;
  }

  virtual auto Typename() const -> std::string override {
    return "float";
  }
};

struct Number : TypeI {
  virtual auto TypeId() const -> std::size_t override {
    return 2;
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

  virtual auto GetResultType() const -> const std::unique_ptr<TypeI>& = 0;
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
std::map<std::string, std::stack<std::unique_ptr<ExpressionI>>> tid;


// struct Variable :  {
//   std::string name;
//   std::unique_ptr<TypeI> type;
//   virtual auto GetResultType() const -> const std::unique_ptr<TypeI>& override {
//     return type;
//   }
// };

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

  virtual auto GetResultType() const -> std::unique_ptr<TypeI> const& override {
    return left->GetResultType();
  }
  void Get(std::ostream &out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueId();
    auto rbuf_name = GetUniqueId();
    left->Get(out, lbuf_name);
    right->Get(out, rbuf_name);
    out << std::format("{} = {} {} {} {}\n",
      to_reg,
      (left->GetResultType()->Typename() == "i32" ? "add" : "fadd"),
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

  virtual auto GetResultType() const -> std::unique_ptr<TypeI> const& override {
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

struct Divide final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> std::unique_ptr<TypeI> const& override {
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

struct DividAndRound final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> std::unique_ptr<TypeI> const& override {
    return left->GetResultType();
  }
  void Get(std::ostream &out, std::string_view to_reg) const override {
    auto lbuf_name = GetUniqueId();
    auto rbuf_name = GetUniqueId();
    left->Get(out, lbuf_name);
    right->Get(out, rbuf_name);
    out << std::format("{} = {} {} {} {}\n",
      to_reg,
      (left->GetResultType()->Typename() == "i32" ? "sdiv" : "fdiv"),
      left->GetResultType()->Typename(),
      lbuf_name,
      rbuf_name);
  }

  void Set(std::ostream&, std::string_view from_reg) const override {
    throw std::logic_error("Can't assign to rvalue");
  }
};

struct Multiply final : BinaryOp {
  using BinaryOp::BinaryOp;

  virtual auto GetResultType() const -> std::unique_ptr<TypeI> const& override {
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
  std::unique_ptr<ExpressionI> value;
  void Get(std::ostream &out, std::string_view) const override {
    out << "ret " << value->GetResultType()->Typename() << " " << value->Get() << "\n";
  }

  void Set(std::ostream&, std::string_view) const override {
    throw std::logic_error("Cant assign to return expression");
  }
};

struct FunctionDecl : ExpressionI {
  std::string name;
  std::unique_ptr<TypeI> return_type;
  std::vector<std::unique_ptr<ExpressionI>> exprs;
  std::vector<std::pair<std::string, std::unique_ptr<TypeI>>> args;

  void Set(std::ostream& out) const override {
    out << "define dso_local " << return_type->Typename() << " @" << name
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

  auto GetResultType() const -> std::unique_ptr<TypeI> const& override {
    return return_type;
  }

  auto GetError() const -> std::optional<std::string> override {
    return std::nullopt;
  }

  void Get(std::ostream&) const override {
    return
  }
};

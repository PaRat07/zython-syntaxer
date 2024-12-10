module;

#include <memory>
#include <vector>
#include <string>

export module tid;

import bor;

export enum class variable_type {
  Integer,
  Float,
  String,
  Array,
};

export class Tid {
 public:

  Tid() : root(std::make_unique<Node>()) {}

  struct Variable_Node {
    std::string name;
    variable_type type;
  };

  struct Function_Node {
    std::string name;
    std::vector<Variable_Node> parameters;
  };

  void NewScope() {
    auto Scope = std::make_unique<Node>();
    Scope->parent = root.get();
    root = std::move(Scope);
  }

  void CloseScope() {
    root = std::make_unique<Node>(root->parent);
  }

  const Variable_Node* FindVariable(const std::string& name) {
    Node* tec_root = root.get();
    while (tec_root && !tec_root->data_variables.Find(name)) {
      tec_root = tec_root->parent;
    }
    if (!tec_root) {
      return nullptr;
    }
    return &tec_root->data_variables.GetData(name);
  }

  const Function_Node* FindFunction(const std::string& name) {
    Node* tec_root = root.get();
    while (tec_root && !tec_root->data_functions.Find(name)) {
      tec_root = tec_root->parent;
    }
    if (!tec_root) {
      return nullptr;
    }
    return &tec_root->data_functions.GetData(name);
  }

 private:
  struct Node {
    Node* parent;
    Bor<Variable_Node> data_variables;
    Bor<Function_Node> data_functions;
  };
  std::unique_ptr<Node> root;
};

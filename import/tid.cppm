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
  Char
};

export class Tid {
 public:

  Tid() : root(nullptr) {}

  struct Variable_Node {
    std::string name;
    variable_type type;
    int array_dimensions = 0;
  };

  struct Function_Node {
    std::string name;
    Variable_Node return_value;
    std::vector<Variable_Node> parameters;
  };

  void NewScope() {
    auto Scope = new Node();
    Scope->parent = root;
    root = Scope;
  }

  void CloseScope() {
    auto tmp = root;
    root = root->parent;
    delete tmp;
  }

  const Variable_Node* FindVariable(const std::string& name) {
    Node* tec_root = root;
    while (tec_root && !tec_root->data_variables.Find(name)) {
      tec_root = tec_root->parent;
    }
    if (!tec_root) {
      return nullptr;
    }
    return &tec_root->data_variables.GetData(name);
  }

  const Function_Node* FindFunction(const std::string& name) {
    Node* tec_root = root;
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

  Node* root;
};

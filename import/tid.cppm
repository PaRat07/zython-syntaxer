module;

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

export module tid;

import bor;

using namespace std::string_view_literals;
using namespace std::string_literals;

export enum class variable_type {
  Integer,
  Float,
  String,
  Array,
  Char,
  Undefined
};

export class Tid {
 public:

  static variable_type TypeFromString(const std::string& str) {
    static const std::unordered_map<std::string, variable_type> actions = {
      {"int", variable_type::Integer },
      {"float", variable_type::Float},
      {"string", variable_type::String}
    };
    auto it = actions.find(str);
    if (it == actions.end()) {
      return variable_type::Undefined;
    }
    return it->second;
  }

  static std::string_view ToValueString(const variable_type& type) {
    switch (type) {
      case variable_type::Integer: {
        return "Integer"sv;
      }
      case variable_type::Float: {
        return "Float"sv;
      }
      case variable_type::String: {
        return "String"sv;
      }
      case variable_type::Array: {
        return "Array"sv;
      }
      case variable_type::Char: {
        return "Char"sv;
      }
      case variable_type::Undefined: {
        return "Undefined"sv;
      }
    }
    return "Undefined"sv;
  }

  Tid() : root(new Node()) {}
  ~Tid() {
    delete root;
  }

  struct Variable_Node {
    Variable_Node() = default;
    explicit Variable_Node(const std::string& s) : name(s) {}
    explicit Variable_Node(const std::string& s, variable_type type) :
    name(s), type(type) {}
    std::string name;
    variable_type type = variable_type::Undefined;
    variable_type in_array_type = variable_type::Undefined;
    int array_dimensions = 0;
  };

  struct Function_Node {
    Function_Node() = default;
    explicit Function_Node(const std::string& s) : name(s) {}
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

  void InsertVariable(const Variable_Node& variable) {
    root->data_variables.Insert(variable.name, variable);
  }

  void InsertFunction(const Function_Node& function) {
    root->data_functions.Insert(function.name, function);
  }

  Variable_Node* FindVariable(const std::string& name) {
    Node* tec_root = root;
    while (tec_root && !tec_root->data_variables.Find(name)) {
      tec_root = tec_root->parent;
    }
    if (!tec_root) {
      return nullptr;
    }
    return &tec_root->data_variables.GetData(name);
  }

  Function_Node* FindFunction(const std::string& name) {
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

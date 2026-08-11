#pragma once
// STW-ENGINE: minimaler JSON-Parser (nur was der glTF-Loader braucht, keine Deps)
#include <map>
#include <string>
#include <vector>

namespace stw {

struct Json {
  enum class T { Null, Bool, Num, Str, Arr, Obj };
  T t = T::Null;
  bool b = false;
  double num = 0;
  std::string str;
  std::vector<Json> arr;
  std::map<std::string, Json> obj;

  const Json* get(const std::string& k) const {
    auto it = obj.find(k);
    return it == obj.end() ? nullptr : &it->second;
  }
  double numOr(double d = 0) const { return t == T::Num ? num : d; }
};

bool ParseJson(const std::string& text, Json& out);

}  // namespace stw

#include "json_mini.h"

#include <cctype>
#include <cstdlib>

namespace stw {

namespace {

struct P {
  const std::string& s;
  size_t i = 0;
  explicit P(const std::string& src) : s(src) {}

  void ws() {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
  }
  bool eof() const { return i >= s.size(); }
  char peek() const { return i < s.size() ? s[i] : '\0'; }

  bool parseValue(Json& v) {
    ws();
    if (eof()) return false;
    char c = peek();
    if (c == '{') return parseObj(v);
    if (c == '[') return parseArr(v);
    if (c == '"') { v.t = Json::T::Str; return parseStr(v.str); }
    if (c == 't') { v.t = Json::T::Bool; v.b = true; i += 4; return true; }
    if (c == 'f') { v.t = Json::T::Bool; v.b = false; i += 5; return true; }
    if (c == 'n') { v.t = Json::T::Null; i += 4; return true; }
    // Zahl
    char* end = nullptr;
    v.num = std::strtod(s.c_str() + i, &end);
    if (end == s.c_str() + i) return false;
    i = static_cast<size_t>(end - s.c_str());
    v.t = Json::T::Num;
    return true;
  }

  bool parseStr(std::string& out) {
    if (peek() != '"') return false;
    i++;
    out.clear();
    while (i < s.size() && s[i] != '"') {
      char c = s[i++];
      if (c == '\\' && i < s.size()) {
        char e = s[i++];
        switch (e) {
          case 'n': out += '\n'; break;
          case 't': out += '\t'; break;
          case 'r': out += '\r'; break;
          case 'b': out += '\b'; break;
          case 'f': out += '\f'; break;
          case 'u': {  // \uXXXX: ASCII-Pfade reichen, Rest als '?'
            if (i + 4 <= s.size()) {
              unsigned code = std::strtoul(s.substr(i, 4).c_str(), nullptr, 16);
              i += 4;
              out += (code < 128 ? static_cast<char>(code) : '?');
            }
            break;
          }
          default: out += e; break;
        }
      } else {
        out += c;
      }
    }
    if (i >= s.size()) return false;
    i++;  // schließendes "
    return true;
  }

  bool parseArr(Json& v) {
    v.t = Json::T::Arr;
    i++;  // [
    ws();
    if (peek() == ']') { i++; return true; }
    while (true) {
      Json item;
      if (!parseValue(item)) return false;
      v.arr.push_back(std::move(item));
      ws();
      if (peek() == ',') { i++; continue; }
      if (peek() == ']') { i++; return true; }
      return false;
    }
  }

  bool parseObj(Json& v) {
    v.t = Json::T::Obj;
    i++;  // {
    ws();
    if (peek() == '}') { i++; return true; }
    while (true) {
      ws();
      std::string key;
      if (!parseStr(key)) return false;
      ws();
      if (peek() != ':') return false;
      i++;
      Json val;
      if (!parseValue(val)) return false;
      v.obj[key] = std::move(val);
      ws();
      if (peek() == ',') { i++; continue; }
      if (peek() == '}') { i++; return true; }
      return false;
    }
  }
};

}  // namespace

bool ParseJson(const std::string& text, Json& out) {
  P p(text);
  return p.parseValue(out);
}

}  // namespace stw

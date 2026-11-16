// Minimal host stand-ins for the Arduino APIs used by config.cpp (tests only).
#pragma once
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
class String {
 public:
  std::string s;
  String() {}
  String(const char *c) : s(c) {}
  String(const std::string &x) : s(x) {}
  unsigned length() const { return s.size(); }
  char operator[](unsigned i) const { return i < s.size() ? s[i] : 0; }
  void trim() { size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
                s = a == std::string::npos ? "" : s.substr(a, b - a + 1); }
  bool startsWith(const char *p) const { return s.rfind(p, 0) == 0; }
  String substring(unsigned a) const { return a < s.size() ? s.substr(a) : ""; }
  const char *c_str() const { return s.c_str(); }
};

#include "HdrLoader.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace stw {
namespace {
struct Rgbe { unsigned char r, g, b, e; };
inline void RgbeToFloat(const Rgbe& c, float* out) {
    if (c.e == 0) { out[0] = out[1] = out[2] = 0.f; return; }
    float f = std::ldexp(1.0f, int(c.e) - (128 + 8));
    out[0] = (c.r + 0.5f) * f;
    out[1] = (c.g + 0.5f) * f;
    out[2] = (c.b + 0.5f) * f;
}
}  // namespace

bool LoadHdr(const std::string& path, std::vector<float>& out, int& w, int& h) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::string line;
    std::getline(f, line);
    if (line.find("#?") != 0) return false;
    bool gotRes = false;
    while (std::getline(f, line)) {
        if (line.empty()) break;  // Header-Ende
        if (line[0] == '#') continue;
        std::sscanf(line.c_str(), "-Y %d +X %d", &h, &w);
        gotRes = true;
    }
    if (!gotRes || w <= 0 || h <= 0) return false;
    out.resize(size_t(w) * h * 3);
    for (int y = 0; y < h; y++) {
        unsigned char hdr[4];
        f.read(reinterpret_cast<char*>(hdr), 4);
        bool rle = f && hdr[0] == 2 && hdr[1] == 2 && ((hdr[2] << 8 | hdr[3]) == w);
        std::vector<Rgbe> row(w);
        if (rle) {
            for (int ch = 0; ch < 4; ch++) {
                int x = 0;
                while (x < w) {
                    unsigned char cc[2];
                    f.read(reinterpret_cast<char*>(cc), 2);
                    int count = cc[0];
                    if (count > 128) {  // run
                        count &= 127;
                        for (int i = 0; i < count && x < w; i++, x++) {
                            reinterpret_cast<unsigned char*>(row.data())[x * 4 + ch] = cc[1];
                        }
                    } else {
                        reinterpret_cast<unsigned char*>(row.data())[x * 4 + ch] = cc[1];
                        x++;
                        for (int i = 1; i < count && x < w; i++, x++) {
                            unsigned char v;
                            f.read(reinterpret_cast<char*>(&v), 1);
                            reinterpret_cast<unsigned char*>(row.data())[x * 4 + ch] = v;
                        }
                    }
                }
            }
        } else {
            f.seekg(-4, std::ios::cur);
            f.read(reinterpret_cast<char*>(row.data()), w * 4);
        }
        for (int x = 0; x < w; x++) {
            float rgb[3];
            RgbeToFloat(row[x], rgb);
            out[(size_t(y) * w + x) * 3 + 0] = rgb[0];
            out[(size_t(y) * w + x) * 3 + 1] = rgb[1];
            out[(size_t(y) * w + x) * 3 + 2] = rgb[2];
        }
    }
    return f.good() || f.eof();
}
}  // namespace stw

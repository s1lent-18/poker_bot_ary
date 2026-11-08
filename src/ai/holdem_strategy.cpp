#include "ai/holdem_strategy.h"

#include <iomanip>
#include <utility>

namespace poker {

void HoldemStrategy::save(std::ostream& os) const {
    os << "HOLDEM_CFR 1\n";
    os << "cfg " << cfg.stack << ' ' << cfg.small_blind << ' ' << cfg.big_blind << ' '
       << cfg.num_raise_fractions;
    for (double f : cfg.raise_fractions) os << ' ' << f;
    os << '\n';
    bucketer.save(os);
    os << "entries " << table.size() << '\n';
    os << std::setprecision(7);
    for (const auto& kv : table) {
        os << kv.first << ' ' << kv.second.size();
        for (float p : kv.second) os << ' ' << p;
        os << '\n';
    }
}

HoldemStrategy HoldemStrategy::load(std::istream& is) {
    std::string magic, tag;
    int version = 0;
    if (!(is >> magic >> version) || magic != "HOLDEM_CFR" || version != 1)
        throw std::runtime_error("not a Hold'em strategy file");

    HoldemStrategy out;
    if (!(is >> tag) || tag != "cfg") throw std::runtime_error("missing cfg line");
    GameConfig& c = out.cfg;
    if (!(is >> c.stack >> c.small_blind >> c.big_blind >> c.num_raise_fractions))
        throw std::runtime_error("bad cfg line");
    for (double& f : c.raise_fractions)
        if (!(is >> f)) throw std::runtime_error("bad cfg line");

    out.bucketer = Bucketer::load(is);

    size_t n = 0;
    if (!(is >> tag >> n) || tag != "entries") throw std::runtime_error("missing entries line");
    out.table.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        std::string key;
        size_t m = 0;
        if (!(is >> key >> m) || m == 0 || m > 5) throw std::runtime_error("bad strategy entry");
        std::vector<float> v(m);
        for (float& p : v)
            if (!(is >> p)) throw std::runtime_error("bad strategy entry");
        out.table.emplace(std::move(key), std::move(v));
    }
    return out;
}

}  // namespace poker
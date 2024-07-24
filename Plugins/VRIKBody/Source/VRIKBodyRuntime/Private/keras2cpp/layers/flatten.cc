// (c) gosha20777 (github.com/gosha20777), 2019, MIT License

#include "keras2cpp/layers/flatten.h"

namespace keras2cpp{
    namespace layers{
        Tensor Flatten::operator()(const Tensor& in) const noexcept {
            return Tensor(in).flatten();
        }
    }
}
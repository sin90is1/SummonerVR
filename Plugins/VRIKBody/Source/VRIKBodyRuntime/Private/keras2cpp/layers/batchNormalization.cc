// (c) gosha20777 (github.com/gosha20777), 2019, MIT License

#include "keras2cpp/layers/batchNormalization.h"

namespace keras2cpp{
    namespace layers{
        BatchNormalization::BatchNormalization(Stream& file)
        : weights_(file), biases_(file) {}
        Tensor BatchNormalization::operator()(const Tensor& in) const noexcept {
            kassert(in.ndim());
            return in.fma(weights_, biases_);
        }
    }
}
// (c) gosha20777 (github.com/gosha20777), 2019, MIT License

#include "keras2cpp/layers/embedding.h"
#include "Runtime/Launch/Resources/Version.h"

namespace keras2cpp{
    namespace layers{
        Embedding::Embedding(Stream& file) : weights_(file, 2) {}

        Tensor Embedding::operator()(const Tensor& in) const noexcept {
            size_t out_i = in.dims_[0];
            size_t out_j = weights_.dims_[1];

#if ENGINE_MAJOR_VERSION > 4
            auto out = Tensor::empty(out_i, out_j);
#else
            auto out = Tensor::empty<size_t>({out_i, out_j});
#endif

            for (const auto& it : in.data_) {
                auto first = weights_.begin() + cast(it * out_j);
                auto last = weights_.begin() + cast(it * out_j + out_j);
                out.data_.insert(out.end(), first, last);
            }
            return out;
        }
    }
}
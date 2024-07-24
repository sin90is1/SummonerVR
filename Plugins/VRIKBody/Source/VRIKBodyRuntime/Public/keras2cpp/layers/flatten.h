// (c) gosha20777 (github.com/gosha20777), 2019, MIT License

#pragma once
#include "../baseLayer.h"
namespace keras2cpp{
    namespace layers{
        class Flatten final : public Layer<Flatten> {
        public:
            using Layer<Flatten>::Layer;
            Tensor operator()(const Tensor& in) const noexcept override;
        };
    }
}
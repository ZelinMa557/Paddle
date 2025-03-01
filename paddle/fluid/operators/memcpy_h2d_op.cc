/* Copyright (c) 2021 PaddlePaddle Authors. All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/fluid/operators/memcpy_h2d_op.h"

#include <string>

#include "paddle/fluid/framework/infershape_utils.h"
#include "paddle/fluid/framework/op_registry.h"
#include "paddle/phi/core/infermeta_utils.h"
#include "paddle/phi/infermeta/unary.h"

namespace paddle::framework {
class OpDesc;
class InferShapeContext;
template <typename T>
class EmptyGradOpMaker;
}  // namespace paddle::framework
namespace paddle::imperative {
class OpBase;
}  // namespace paddle::imperative

namespace paddle::operators {

class MemcpyH2DOp : public framework::OperatorWithKernel {
 public:
  using framework::OperatorWithKernel::OperatorWithKernel;

 protected:
  phi::KernelKey GetKernelTypeForVar(
      const std::string &var_name,
      const phi::DenseTensor &tensor,
      const phi::KernelKey &expected_kernel_type) const override {
    return phi::KernelKey(phi::Backend::ALL_BACKEND,
                          tensor.layout(),
                          expected_kernel_type.dtype());
  }

  phi::KernelKey GetExpectedKernelType(
      const framework::ExecutionContext &ctx) const override {
    return phi::KernelKey(OperatorWithKernel::IndicateVarDataType(ctx, "X"),
                          ctx.GetPlace());
  }
};

class MemcpyH2DInferVarType : public framework::VarTypeInference {
 public:
  void operator()(framework::InferVarTypeContext *ctx) const override {
    ctx->SyncTypeAndDataType("X", "Out");
  }
};

class MemcpyH2DKernel {
 public:
  void operator()(const framework::ExecutionContext &ctx) const {
    auto *x = ctx.InputVar("X");
    if (x == nullptr) {
      return;
    }
    PADDLE_ENFORCE_EQ(
        ctx.HasOutput("Out"),
        true,
        phi::errors::NotFound("Output(Out) of memcpy_d2h_op is not found."));
    auto *out = ctx.OutputVar("Out");
    // Get dev_ctx from ExecutionContext, it's H2D stream
    auto &dev_ctx = ctx.device_context();
    auto dst_place_type = ctx.Attr<int>("dst_place_type");
    framework::VisitVarType(*x, MemcpyH2DFunctor(out, dev_ctx, dst_place_type));
  }
};

class MemcpyH2DOpProtoMaker : public framework::OpProtoAndCheckerMaker {
 public:
  void Make() override {
    AddInput("X", "(phi::DenseTensor) The input variable ");
    AddOutput("Out",
              "(phi::DenseTensor) The type of output "
              "is the same as input X.");
    AddAttr<int>("dst_place_type",
                 "Determine the dst place of tensor copy. "
                 "By Now it support:"
                 "0. CUDAPinnedPlace/CPU <->CUDAPlace"
                 "1. CPU <->XPUPlace"
                 "2. CPU <->IPUPlace"
                 "Other place type is Unimplemented and will cause ERROR.");
    AddComment(R"DOC(
    MemcpyD2H Operator.
    By now, it ONLY supports the memcopy between CUDAPinnedPlace/CPU <-> CUDAPlace.
    You would have to update it if you want other more capacities.
Out = X,  when type in [phi::DenseTensor]
raise error if the type is not listed above.
)DOC");
  }
};

}  // namespace paddle::operators

namespace ops = paddle::operators;

DECLARE_INFER_SHAPE_FUNCTOR(memcpy_h2d,
                            MemcpyH2DInferShapeFunctor,
                            PD_INFER_META(phi::UnchangedInferMeta));
REGISTER_OPERATOR(
    memcpy_h2d,
    ops::MemcpyH2DOp,
    ops::MemcpyH2DOpProtoMaker,
    ops::MemcpyH2DInferVarType,
    paddle::framework::EmptyGradOpMaker<paddle::framework::OpDesc>,
    paddle::framework::EmptyGradOpMaker<paddle::imperative::OpBase>,
    MemcpyH2DInferShapeFunctor);

#ifdef PADDLE_WITH_IPU
REGISTER_OP_IPU_KERNEL_FUNCTOR(memcpy_h2d,
                               float,
                               ops::MemcpyH2DKernel,
                               double,
                               ops::MemcpyH2DKernel,
                               int8_t,
                               ops::MemcpyH2DKernel,
                               uint8_t,
                               ops::MemcpyH2DKernel,
                               int,
                               ops::MemcpyH2DKernel,
                               int64_t,
                               ops::MemcpyH2DKernel,
                               bool,
                               ops::MemcpyH2DKernel,
                               phi::dtype::bfloat16,
                               ops::MemcpyH2DKernel,
                               paddle::platform::complex<float>,
                               ops::MemcpyH2DKernel,
                               paddle::platform::complex<double>,
                               ops::MemcpyH2DKernel,
                               phi::dtype::float16,
                               ops::MemcpyH2DKernel,
                               int16_t,
                               ops::MemcpyH2DKernel);
#endif

//
// Copyright (c) 2017 XiaoMi All rights reserved.
//

#include "mace/ops/concat.h"
#include "gmock/gmock.h"
#include "mace/ops/ops_test_util.h"

using namespace mace;

class ConcatOpTest : public OpsTestBase {};

TEST_F(ConcatOpTest, CPUSimpleHorizon) {
  // Construct graph
  auto &net = test_net();
  OpDefBuilder("Concat", "ConcatTest")
      .Input("Input0")
      .Input("Input1")
      .Input("Axis")
      .Output("Output")
      .Finalize(net.NewOperatorDef());

  std::vector<index_t> input_shape = {4, 4};
  std::vector<float> input0;
  GenerateRandomRealTypeData(input_shape, input0);
  std::vector<float> input1;
  GenerateRandomRealTypeData(input_shape, input1);
  // Add inputs
  net.AddInputFromArray<DeviceType::CPU, float>("Input0", input_shape, input0);
  net.AddInputFromArray<DeviceType::CPU, float>("Input1", input_shape, input1);
  net.AddInputFromArray<DeviceType::CPU, int>("Axis", {}, {0});

  // Run
  net.RunOp();

  // Check
  auto output = net.GetOutput("Output");

  std::vector<index_t> expected_shape = {8, 4};
  EXPECT_THAT(output->shape(), ::testing::ContainerEq(expected_shape));

  const float *output_ptr = output->data<float>();
  for (auto f : input0) {
    ASSERT_EQ(f, *output_ptr++);
  }
  for (auto f : input1) {
    ASSERT_EQ(f, *output_ptr++);
  }
}

TEST_F(ConcatOpTest, CPUSimpleVertical) {
  // Construct graph
  auto &net = test_net();
  OpDefBuilder("Concat", "ConcatTest")
      .Input("Input0")
      .Input("Input1")
      .Input("Axis")
      .Output("Output")
      .Finalize(net.NewOperatorDef());

  std::vector<index_t> input_shape = {4, 4};
  std::vector<float> input0;
  GenerateRandomRealTypeData(input_shape, input0);
  std::vector<float> input1;
  GenerateRandomRealTypeData(input_shape, input1);
  // Add inputs
  net.AddInputFromArray<DeviceType::CPU, float>("Input0", input_shape, input0);
  net.AddInputFromArray<DeviceType::CPU, float>("Input1", input_shape, input1);
  net.AddInputFromArray<DeviceType::CPU, int>("Axis", {}, {1});

  // Run
  net.RunOp();

  // Check
  auto output = net.GetOutput("Output");

  std::vector<index_t> expected_shape = {4, 8};
  EXPECT_THAT(output->shape(), ::testing::ContainerEq(expected_shape));

  const float *output_ptr = output->data<float>();
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      ASSERT_EQ(input0[i * 4 + j], *output_ptr++);
    }
    for (int j = 0; j < 4; ++j) {
      ASSERT_EQ(input1[i * 4 + j], *output_ptr++);
    }
  }
}

TEST_F(ConcatOpTest, CPURandom) {
  srand(time(nullptr));
  int dim = 5;
  int num_inputs = 2 + rand() % 10;
  int axis = rand() % dim;
  // Construct graph
  auto &net = test_net();
  auto builder = OpDefBuilder("Concat", "ConcatTest");
  for (int i = 0; i < num_inputs; ++i) {
    builder = builder.Input(("Input" + ToString(i)).c_str());
  }
  builder.Input("Axis").Output("Output").Finalize(net.NewOperatorDef());

  std::vector<index_t> shape_data;
  GenerateRandomIntTypeData<index_t>({dim}, shape_data, 1, dim);
  std::vector<std::vector<index_t>> input_shapes(num_inputs, shape_data);
  std::vector<std::vector<float>> inputs(num_inputs, std::vector<float>());
  std::vector<float *> input_ptrs(num_inputs, nullptr);
  index_t concat_axis_size = 0;
  for (int i = 0; i < num_inputs; ++i) {
    input_shapes[i][axis] = 1 + rand() % dim;
    concat_axis_size += input_shapes[i][axis];
    GenerateRandomRealTypeData(input_shapes[i], inputs[i]);
    input_ptrs[i] = inputs[i].data();
    net.AddInputFromArray<DeviceType::CPU, float>(("Input" + ToString(i)).c_str(),
                                 input_shapes[i], inputs[i]);
  }
  net.AddInputFromArray<DeviceType::CPU, int>("Axis", {}, {axis});

  // Run
  net.RunOp();

  // Check
  auto output = net.GetOutput("Output");

  std::vector<index_t> expected_shape = input_shapes[0];
  expected_shape[axis] = concat_axis_size;
  EXPECT_THAT(output->shape(), ::testing::ContainerEq(expected_shape));

  const float *output_ptr = output->data<float>();
  while (output_ptr != (output->data<float>() + output->size())) {
    for (int i = 0; i < num_inputs; ++i) {
      index_t num_elements =
          std::accumulate(input_shapes[i].begin() + axis, input_shapes[i].end(),
                          1, std::multiplies<index_t>());
      for (int j = 0; j < num_elements; ++j) {
        EXPECT_EQ(*input_ptrs[i]++, *output_ptr++);
      }
    }
  }
}

template<typename T>
void OpenclRandomTest(const std::vector<std::vector<index_t>> &shapes,
                      const int axis) {
  srand(time(nullptr));
  int num_inputs = 2;
  int concat_axis_size = 0;
  // Construct graph
  OpsTestNet net;
  for (int i = 0; i < num_inputs; ++i) {
    const std::string input_name = ("Input" + ToString(i)).c_str();
    const std::string image_name = ("InputImage" + ToString(i)).c_str();
    concat_axis_size += shapes[i][axis];
    net.AddRandomInput<DeviceType::OPENCL, float>(input_name,
                                                  shapes[i]);
    BufferToImage<DeviceType::OPENCL, T>(net, input_name, image_name, kernels::BufferType::IN_OUT);
  }
  net.AddInputFromArray<DeviceType::OPENCL, int>("Axis", {}, {axis});

  auto builder = OpDefBuilder("Concat", "ConcatTest");
  for (int i = 0; i < num_inputs; ++i) {
    const std::string image_name = ("InputImage" + ToString(i)).c_str();
    builder = builder.Input(image_name);
  }
  builder.Input("Axis")
      .Output("OutputImage")
      .AddIntArg("T", static_cast<int>(DataTypeToEnum<T>::value))
      .Finalize(net.NewOperatorDef());

  // Run
  net.RunOp(DeviceType::OPENCL);

  ImageToBuffer<DeviceType::OPENCL, float>(net, "OutputImage", "Output", kernels::BufferType::IN_OUT);

  // Check
  auto output = net.GetOutput("Output");

  std::vector<index_t> expected_shape = shapes[0];
  expected_shape[axis] = concat_axis_size;
  EXPECT_THAT(output->shape(), ::testing::ContainerEq(expected_shape));

  Tensor::MappingGuard output_mapper(output);
  const float *output_ptr = output->data<float>();
  int k = 0;
  while (output_ptr != (output->data<float>() + output->size())) {
    for (int i = 0; i < num_inputs; ++i) {
      index_t num_elements =
          std::accumulate(shapes[i].begin() + axis, shapes[i].end(),
                          1, std::multiplies<index_t>());

      const std::string input_name = ("Input" + ToString(i)).c_str();
      const Tensor *input_tensor = net.GetTensor(input_name.data());
      Tensor::MappingGuard input_guard(input_tensor);
      const float *input_ptr = input_tensor->data<float>() + k * num_elements;
      for (int j = 0; j < num_elements; ++j) {
        EXPECT_NEAR(*(input_ptr + j), *output_ptr++, 1e-2) << "With index: " << i << ", " << j;
      }
    }
    k++;
  }
}

TEST_F(ConcatOpTest, OPENCLAligned) {
  OpenclRandomTest<float>({
                              {3, 32, 32, 32},
                              {3, 32, 32, 64}
                          },
                          3);
}

TEST_F(ConcatOpTest, OPENCLHalfAligned) {
  OpenclRandomTest<half>({
                              {3, 32, 32, 32},
                              {3, 32, 32, 64}
                          },
                          3);
}

TEST_F(ConcatOpTest, OPENCLUnAligned) {
  OpenclRandomTest<float>({
                              {3, 32, 32, 13},
                              {3, 32, 32, 17}
                          },
                          3);
}

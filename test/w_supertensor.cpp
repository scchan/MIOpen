/*******************************************************************************
 *
 * MIT License
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/
#include <array>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <miopen/convolution.hpp>
#include <miopen/miopen.h>
#include <miopen/tensor.hpp>
#include <miopen/handle.hpp>
#include <miopen/tensor_ops.hpp>
#include <miopen/allocator.hpp>
#include <utility>

#include "driver.hpp"
#include "test.hpp"

struct superTensorTest : test_driver
{
    miopenRNNDescriptor_t rnnDesc;

    int num_layer;
    int wei_hh;
    int batch_size;

    miopenRNNMode_t mode;
    miopenRNNBiasMode_t biasMode;
    miopenRNNDirectionMode_t directionMode;
    miopenRNNInputMode_t inMode;
    miopenRNNAlgo_t algo = miopenRNNdefault;
    miopenDataType_t dataType;

    int seqLen;
    int in_size;

    miopenTensorDescriptor_t inputTensor;
    miopenTensorDescriptor_t weightTensor;
    miopenTensorDescriptor_t paramTensor;
    miopenTensorDescriptor_t biasTensor;

    miopen::Allocator::ManageDataPtr wei_dev;

    superTensorTest()
    {
        miopenCreateRNNDescriptor(&rnnDesc);

        dataType = miopenFloat;

        std::vector<int> get_seqLen                   = {1, 2, 4};
        std::vector<int> get_batch_size               = {2, 4, 8};
        std::vector<int> get_num_layer                = {4, 8, 16};
        std::vector<int> get_in_size                  = {2, 8, 16};
        std::vector<int> get_wei_hh                   = {4, 8, 16};
        std::vector<miopenRNNMode_t> get_mode         = {miopenRNNRELU, miopenLSTM, miopenGRU};
        std::vector<miopenRNNBiasMode_t> get_biasMode = {miopenRNNwithBias, miopenRNNNoBias};
        std::vector<miopenRNNDirectionMode_t> get_directionMode = {miopenRNNunidirection,
                                                                   miopenRNNbidirection};
        std::vector<miopenRNNInputMode_t> get_inMode = {miopenRNNskip, miopenRNNlinear};

        add(seqLen, "seqLen", generate_data(get_seqLen));
        add(in_size, "in_size", generate_data(get_in_size));
        add(batch_size, "batch_size", generate_data(get_batch_size));
        add(num_layer, "num_layer", generate_data(get_num_layer));
        add(wei_hh, "wei_hh", generate_data(get_wei_hh));
        add(mode, "mode", generate_data(get_mode));
        add(biasMode, "biasMode", generate_data(get_biasMode));
        add(directionMode, "directionMode", generate_data(get_directionMode));
        add(inMode, "inMode", generate_data(get_inMode));
        // add(algo, "algo", generate_data(get_algo));

        miopenCreateTensorDescriptor(&inputTensor);
        miopenCreateTensorDescriptor(&weightTensor);
        miopenCreateTensorDescriptor(&paramTensor);
        miopenCreateTensorDescriptor(&biasTensor);
    }

    std::vector<float> fill_w_tensor()
    {
        auto&& handle = get_handle();
        size_t wei_sz = 0;
        miopenGetRNNParamsSize(&handle, rnnDesc, inputTensor, &wei_sz, miopenFloat);
        wei_sz = wei_sz / sizeof(miopenFloat);
        std::vector<float> wei_h(wei_sz, 0);

        int offset = 0;

        int num_HiddenLayer = (mode == miopenRNNRELU) ? 1 : (mode == miopenGRU ? 3 : 4);

        if(directionMode == miopenRNNbidirection)
        {
            for(int k = 0; k < num_layer * 4; k++)
            {
                for(int j = 0; j < num_HiddenLayer; j++)
                {
                    int layer   = k % 2 + (k / 4) * 2;
                    int layerId = (k % 4 > 1) ? j + num_HiddenLayer : j;

                    if((inMode == miopenRNNskip) && (layer < 2) && (layerId >= num_HiddenLayer))
                    {
                        break;
                    }

                    size_t paramSize = 0;
                    miopenGetRNNLayerParamSize(
                        &handle, rnnDesc, layer, inputTensor, layerId, &paramSize);

                    paramSize /= sizeof(miopenFloat);

                    // printf("param layer: %d Id: %d offset: %d size: %d\n", layer, layerId,
                    // offset, paramSize);

                    for(int i = 0; i < paramSize; i++)
                    {
                        wei_h[offset + i] = layer * 10 + layerId;
                    }

                    offset += paramSize;
                }
            }

            if(biasMode == miopenRNNwithBias)
            {
                for(int k = 0; k < num_layer * 4; k++)
                {
                    for(int j = 0; j < num_HiddenLayer; j++)
                    {

                        int layer   = k % 2 + (k / 4) * 2;
                        int layerID = (k % 4 > 1) ? j + num_HiddenLayer : j;

                        if((inMode == miopenRNNskip) && (layer < 2) && (layerID >= num_HiddenLayer))
                        {
                            break;
                        }

                        size_t biasSize = 0;
                        miopenGetRNNLayerBiasSize(&handle, rnnDesc, layer, layerID, &biasSize);

                        // printf("bias layer: %d layerId: %d offset: %d size: %d\n", layer,
                        // layerID, offset, biasSize);
                        biasSize /= sizeof(float);

                        for(int i = 0; i < biasSize; i++)
                        {
                            wei_h[offset + i] = layer * 10 + layerID;
                        }
                        offset += biasSize;
                    }
                }
            }
        }
        else
        {
            for(int k = 0; k < num_layer; k++)
            {
                int skip = (inMode == miopenRNNskip && k < 1) ? 1 : 2;

                for(int j = 0; j < num_HiddenLayer * skip; j++)
                {
                    size_t paramSize = 0;
                    miopenGetRNNLayerParamSize(&handle, rnnDesc, k, inputTensor, j, &paramSize);

                    paramSize /= sizeof(miopenFloat);

                    // printf("param layer: %d Id: %d offset: %d size: %d\n", k, j, offset,
                    // paramSize);

                    for(int i = 0; i < paramSize; i++)
                    {
                        wei_h[offset + i] = k * 10 + j;
                    }

                    offset += paramSize;
                }
            }

            if(biasMode == miopenRNNwithBias)
            {
                for(int layer = 0; layer < num_layer; layer++)
                {
                    int skip = (inMode == miopenRNNskip && layer < 1) ? 1 : 2;

                    for(int layerID = 0; layerID < num_HiddenLayer * skip; layerID++)
                    {

                        size_t biasSize = 0;
                        miopenGetRNNLayerBiasSize(&handle, rnnDesc, layer, layerID, &biasSize);

                        // printf("bias layer: %d layerId: %d offset: %d size: %d\n", layer,
                        // layerID, offset, biasSize);
                        biasSize /= sizeof(float);

                        for(int i = 0; i < biasSize; i++)
                        {
                            wei_h[offset + i] = layer * 10 + layerID;
                        }
                        offset += biasSize;
                    }
                }
            }
        }

        return wei_h;
    }

    std::vector<float> setRNNLayer()
    {
        auto&& handle       = get_handle();
        int num_HiddenLayer = (mode == miopenRNNRELU) ? 1 : (mode == miopenGRU ? 3 : 4);
        int bi              = (directionMode == miopenRNNbidirection) ? 2 : 1;

        for(int layer = 0; layer < num_layer * bi; layer++)
        {

            int skip = 2;
            if(inMode == miopenRNNskip && layer < bi)
            {
                skip = 1;
            }

            for(int layerID = 0; layerID < num_HiddenLayer * skip; layerID++)
            {

                size_t paramSize = 0;
                miopenGetRNNLayerParamSize(
                    &handle, rnnDesc, layer, inputTensor, layerID, &paramSize);

                auto param_dev_out = handle.Create(paramSize);

                miopenGetRNNLayerParam(&handle,
                                       rnnDesc,
                                       layer,
                                       inputTensor,
                                       weightTensor,
                                       wei_dev.get(),
                                       layerID,
                                       paramTensor,
                                       nullptr);

                paramSize /= sizeof(miopenFloat);
                std::vector<float> param_h_in(paramSize, layer * 10 + layerID);
                auto param_dev_in = handle.Write(param_h_in);

                miopenSetRNNLayerParam(&handle,
                                       rnnDesc,
                                       layer,
                                       inputTensor,
                                       weightTensor,
                                       wei_dev.get(),
                                       layerID,
                                       paramTensor,
                                       param_dev_in.get());

                if(biasMode == miopenRNNwithBias)
                {
                    size_t biasSize = 0;

                    miopenGetRNNLayerBiasSize(&handle, rnnDesc, layer, layerID, &biasSize);

                    auto bias_dev_out = handle.Create(biasSize);

                    miopenGetRNNLayerBias(&handle,
                                          rnnDesc,
                                          layer,
                                          inputTensor,
                                          weightTensor,
                                          wei_dev.get(),
                                          layerID,
                                          biasTensor,
                                          nullptr);

                    biasSize /= sizeof(float);
                    std::vector<float> bias_h_in(biasSize, layer * 10 + layerID);
                    auto bias_dev_in = handle.Write(bias_h_in);

                    miopenSetRNNLayerBias(&handle,
                                          rnnDesc,
                                          layer,
                                          inputTensor,
                                          weightTensor,
                                          wei_dev.get(),
                                          layerID,
                                          biasTensor,
                                          bias_dev_in.get());
                }
            }
        }

        size_t wei_sz = 0;
        miopenGetRNNParamsSize(&handle, rnnDesc, inputTensor, &wei_sz, miopenFloat);
        wei_sz = wei_sz / sizeof(miopenFloat);
        return handle.Read<float>(wei_dev, wei_sz);
    }

    void getRNNLayer()
    {
        auto&& handle       = get_handle();
        int num_HiddenLayer = (mode == miopenRNNRELU) ? 1 : (mode == miopenGRU ? 3 : 4);
        int bi              = (directionMode == miopenRNNbidirection) ? 2 : 1;

        for(int layer = 0; layer < num_layer * bi; layer++)
        {

            int skip = 2;
            if(inMode == miopenRNNskip && layer < bi)
            {
                skip = 1;
            }

            for(int layerID = 0; layerID < num_HiddenLayer * skip; layerID++)
            {

                size_t paramSize = 0;
                miopenGetRNNLayerParamSize(
                    &handle, rnnDesc, layer, inputTensor, layerID, &paramSize);

                auto param_dev_out = handle.Create(paramSize);

                miopenGetRNNLayerParam(&handle,
                                       rnnDesc,
                                       layer,
                                       inputTensor,
                                       weightTensor,
                                       wei_dev.get(),
                                       layerID,
                                       paramTensor,
                                       param_dev_out.get());

                paramSize /= sizeof(miopenFloat);

                auto param_h_out = handle.Read<float>(param_dev_out, paramSize);

                for(int i = 0; i < param_h_out.size(); i++)
                {

                    EXPECT(static_cast<int>(param_h_out[i]) == 10 * layer + layerID);
                    // if(static_cast<int>(param_h_out[i]) != 10 * layer + layerID)
                    //{
                    // fprintf(stderr,
                    //"param mismatch at %d : d %f != h %d\n",
                    // i,
                    // param_h_out[i],
                    // 10 * layer + layerID);
                    // exit(1);
                    //}
                }

                if(biasMode == miopenRNNwithBias)
                {

                    size_t biasSize = 0;

                    miopenGetRNNLayerBiasSize(&handle, rnnDesc, layer, layerID, &biasSize);

                    auto bias_dev_out = handle.Create(biasSize);

                    miopenGetRNNLayerBias(&handle,
                                          rnnDesc,
                                          layer,
                                          inputTensor,
                                          weightTensor,
                                          wei_dev.get(),
                                          layerID,
                                          biasTensor,
                                          bias_dev_out.get());

                    biasSize /= sizeof(float);

                    auto bias_h_out = handle.Read<float>(bias_dev_out, biasSize);

                    for(int i = 0; i < bias_h_out.size(); i++)
                    {
                        EXPECT(static_cast<int>(bias_h_out[i]) == 10 * layer + layerID);
                        // if(static_cast<int>(bias_h_out[i]) != 10 * layer + layerID)
                        //{
                        // fprintf(stderr,
                        //"bias mismatch at %d : d %d != %d\n",
                        // i,
                        // static_cast<int>(bias_h_out[i]),
                        // 10 * layer + layerID);
                        // exit(1);
                        //}
                    }
                }
            }
        }
    }

    void run()
    {
        miopenSetRNNDescriptor(
            rnnDesc, wei_hh, num_layer, inMode, directionMode, mode, biasMode, algo, dataType);

        size_t in_sz  = 0;
        size_t wei_sz = 0;

        auto&& handle = get_handle();

        std::array<int, 2> in_lens = {{batch_size, in_size}};
        miopenSetTensorDescriptor(inputTensor, dataType, 2, in_lens.data(), nullptr);
        miopenSetTensorDescriptor(weightTensor, dataType, 2, in_lens.data(), nullptr);

        miopenGetRNNInputTensorSize(&handle, rnnDesc, seqLen, &inputTensor, &in_sz);

        miopenGetRNNParamsSize(&handle, rnnDesc, inputTensor, &wei_sz, miopenFloat);

        // wei_sz = wei_sz / sizeof(miopenFloat);
        // std::vector<float> wei_h(wei_sz, 0);
        wei_dev = handle.Create(wei_sz);

        auto wei_h   = fill_w_tensor();
        auto wei_set = setRNNLayer();

        EXPECT(wei_h.size() == wei_set.size());

        for(int i = 0; i < wei_h.size(); i++)
        {
            EXPECT(static_cast<int>(wei_h[i]) == static_cast<int>(wei_set[i]));
        }

        getRNNLayer();
    }
};

int main(int argc, const char* argv[]) { test_drive<superTensorTest>(argc, argv); }

//============================================================================================================
//
//
//                  Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once


#include <atomic>
#include <cassert>
#include <volk/volk.h>

namespace VulkanTraits
{
    template<typename T>
    struct DestroyFn;

    template<>
    struct DestroyFn<VkFramebuffer>
    {
        static void Call(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator)
        {
            vkDestroyFramebuffer(device, framebuffer, pAllocator);
        }
    };

    template<>
    struct DestroyFn<VkImageView>
    {
        static void Call(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator)
        {
            vkDestroyImageView(device, imageView, pAllocator);
        }
    };

    template<>
    struct DestroyFn<VkImage>
    {
        static void Call(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator)
        {
            vkDestroyImage(device, image, pAllocator);
        }
    };

    template<>
    struct DestroyFn<VkBuffer>
    {
        static void Call(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator)
        {
            vkDestroyBuffer(device, buffer, pAllocator);
        }
    };

    template<>
    struct DestroyFn<VkShaderModule>
    {
        static void Call(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator)
        {
            vkDestroyShaderModule(device, shaderModule, pAllocator);
        }
    };

    template<>
    struct DestroyFn<VkPipeline>
    {
        static void Call(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator)
        {
            vkDestroyPipeline(device, pipeline, pAllocator);
        }
    };

    template<>
    struct DestroyFn<VkPipelineLayout>
    {
        static void Call(VkDevice device, VkPipelineLayout layout, const VkAllocationCallbacks* pAllocator)
        {
            vkDestroyPipelineLayout(device, layout, pAllocator);
        }
    };

    template<>
    struct DestroyFn<VkRenderPass>
    {
        static void Call(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator)
        {
            vkDestroyRenderPass(device, renderPass, pAllocator);
        }
    };

    template<>
    struct DestroyFn<VkDescriptorSetLayout>
    {
        static void Call(VkDevice device, VkDescriptorSetLayout layout, const VkAllocationCallbacks* pAllocator)
        {
            vkDestroyDescriptorSetLayout(device, layout, pAllocator);
        }
    };

    template<>
    struct DestroyFn<VkDescriptorPool>
    {
        static void Call(VkDevice device, VkDescriptorPool pool, const VkAllocationCallbacks* pAllocator)
        {
            vkDestroyDescriptorPool(device, pool, pAllocator);
        }
    };

    template<>
    struct DestroyFn<VkSampler>
    {
        static void Call(VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator)
        {
            vkDestroySampler(device, sampler, pAllocator);
        }
    };

    template<>
    struct DestroyFn<VkDeviceMemory>
    {
        static void Call(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator)
        {
            vkFreeMemory(device, memory, pAllocator);
        }
    };

    template<>
    struct DestroyFn<VkFence>
    {
        static void Call(VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator)
        {
            vkDestroyFence(device, fence, pAllocator);
        }
    };

    template<>
    struct DestroyFn<VkSemaphore>
    {
        static void Call(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator)
        {
            vkDestroySemaphore(device, semaphore, pAllocator);
        }
    };

    template<>
    struct DestroyFn<VkCommandPool>
    {
        static void Call(VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator)
        {
            vkDestroyCommandPool(device, commandPool, pAllocator);
        }
    };

    template<>
    struct DestroyFn<VkSwapchainKHR>
    {
        static void Call(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator)
        {
            vkDestroySwapchainKHR(device, swapchain, pAllocator);
        }
    };

} // VulkanTraits
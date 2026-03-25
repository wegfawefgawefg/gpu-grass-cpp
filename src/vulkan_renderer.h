#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan.h>

#include "demo_types.h"
#include "vulkan_helpers.h"

struct VulkanRenderer
{
    VulkanRenderer() = default;
    ~VulkanRenderer();

    void Initialize(SDL_Window* window, std::span<const GrassBladeGpu> blades);
    void Shutdown();
    void Resize(
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        std::uint32_t renderWidth,
        std::uint32_t renderHeight
    );

    void ProcessImGuiEvent(const SDL_Event& event);
    void BeginImGuiFrame();
    void Render(const FrameState& frame, std::span<const std::uint32_t> overlayPixels);

    [[nodiscard]] bool WantsMouseCapture() const;
    [[nodiscard]] bool WantsKeyboardCapture() const;

    void CreateInstance();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateDevice();
    void CreateCommandObjects();
    void CreateSyncObjects();
    void CreateStaticBuffers(std::span<const GrassBladeGpu> blades);
    void CreateDescriptorObjects();
    void CreateSwapchain(std::uint32_t width, std::uint32_t height);
    void CreateSceneTargets(std::uint32_t width, std::uint32_t height);
    void CreateRenderPasses();
    void CreatePipelines();
    void CreateFramebuffers();
    void CreateSampler();
    void CreateImGuiObjects();
    void UploadImGuiFonts();
    void DestroySwapchain();
    void DestroyPipelines();
    void DestroyImGuiObjects();
    void UpdateDescriptorSets();
    void RecordCommandBuffer(std::uint32_t imageIndex);
    void SubmitImmediate(const std::function<void(VkCommandBuffer)>& recorder);
    std::uint32_t ChooseQueueFamily() const;
    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) const;
    VkFormat ChooseDepthFormat() const;

    SDL_Window* m_window = nullptr;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    std::uint32_t m_queueFamilyIndex = 0;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapchainExtent = {};
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    std::vector<VkFramebuffer> m_presentFramebuffers;
    std::vector<bool> m_swapchainImagePrimed;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence m_frameFence = VK_NULL_HANDLE;

    BufferResource m_sceneUniformBuffer;
    BufferResource m_presentUniformBuffer;
    BufferResource m_bladeBuffer;
    BufferResource m_repulsorBuffer;
    BufferResource m_overlayBuffer;
    VkDeviceSize m_bladeBufferBytes = 0;
    VkDeviceSize m_repulsorBufferBytes = 0;
    VkDeviceSize m_overlayBufferBytes = 0;

    ImageResource m_sceneColorTarget;
    ImageResource m_sceneDepthTarget;
    VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;
    VkFramebuffer m_sceneFramebuffer = VK_NULL_HANDLE;
    VkSampler m_sceneSampler = VK_NULL_HANDLE;
    bool m_sceneColorPrimed = false;
    bool m_sceneDepthPrimed = false;

    VkDescriptorSetLayout m_sceneDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_presentDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_sceneDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorPool m_presentDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_sceneDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet m_presentDescriptorSet = VK_NULL_HANDLE;

    VkRenderPass m_sceneRenderPass = VK_NULL_HANDLE;
    VkRenderPass m_presentRenderPass = VK_NULL_HANDLE;
    VkPipelineLayout m_scenePipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_presentPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_groundPipeline = VK_NULL_HANDLE;
    VkPipeline m_grassPipeline = VK_NULL_HANDLE;
    VkPipeline m_markerPipeline = VK_NULL_HANDLE;
    VkPipeline m_presentPipeline = VK_NULL_HANDLE;

    VkDescriptorPool m_imguiDescriptorPool = VK_NULL_HANDLE;
    bool m_imguiInitialized = false;

    std::uint32_t m_windowWidth = 0;
    std::uint32_t m_windowHeight = 0;
    std::uint32_t m_renderWidth = 0;
    std::uint32_t m_renderHeight = 0;
};

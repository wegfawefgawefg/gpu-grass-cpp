#include "vulkan_renderer.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

#include <SDL3/SDL_vulkan.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>

namespace
{
constexpr const char* kGrassVertShaderPath = GPU_GRASS_VERT_SHADER_PATH;
constexpr const char* kGrassFragShaderPath = GPU_GRASS_FRAG_SHADER_PATH;
constexpr const char* kGroundVertShaderPath = GPU_GROUND_VERT_SHADER_PATH;
constexpr const char* kGroundFragShaderPath = GPU_GROUND_FRAG_SHADER_PATH;
constexpr const char* kMarkerVertShaderPath = GPU_MARKER_VERT_SHADER_PATH;
constexpr const char* kMarkerFragShaderPath = GPU_MARKER_FRAG_SHADER_PATH;
constexpr const char* kPresentVertShaderPath = GPU_PRESENT_VERT_SHADER_PATH;
constexpr const char* kPresentFragShaderPath = GPU_PRESENT_FRAG_SHADER_PATH;

VkShaderModule CreateShaderModule(VkDevice device, std::string_view path)
{
    const std::vector<std::byte> shaderBytes = ReadBinaryFile(path);
    VkShaderModuleCreateInfo moduleInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaderBytes.size(),
        .pCode = reinterpret_cast<const std::uint32_t*>(shaderBytes.data()),
    };

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    CheckVk(vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule), "vkCreateShaderModule");
    return shaderModule;
}

void TransitionImage(
    VkCommandBuffer commandBuffer,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkAccessFlags srcAccessMask,
    VkAccessFlags dstAccessMask,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    VkImageAspectFlags aspectMask
)
{
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange =
            {
                .aspectMask = aspectMask,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask,
        dstStageMask,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );
}
} // namespace

VulkanRenderer::~VulkanRenderer()
{
    Shutdown();
}

void VulkanRenderer::Initialize(SDL_Window* window, std::span<const GrassBladeGpu> blades)
{
    m_window = window;

    CreateInstance();
    CreateSurface();
    PickPhysicalDevice();
    CreateDevice();
    CreateCommandObjects();
    CreateSyncObjects();
    CreateStaticBuffers(blades);
    CreateDescriptorObjects();
    CreateSampler();

    int pixelWidth = 0;
    int pixelHeight = 0;
    if (!SDL_GetWindowSizeInPixels(m_window, &pixelWidth, &pixelHeight))
    {
        throw std::runtime_error("SDL_GetWindowSizeInPixels failed");
    }

    m_windowWidth = static_cast<std::uint32_t>(std::max(pixelWidth, 1));
    m_windowHeight = static_cast<std::uint32_t>(std::max(pixelHeight, 1));
    m_renderWidth = m_windowWidth;
    m_renderHeight = m_windowHeight;

    CreateSwapchain(m_windowWidth, m_windowHeight);
    m_depthFormat = ChooseDepthFormat();
    CreateSceneTargets(m_renderWidth, m_renderHeight);
    CreateRenderPasses();
    CreatePipelines();
    CreateFramebuffers();
    UpdateDescriptorSets();
    CreateImGuiObjects();
    UploadImGuiFonts();
}

void VulkanRenderer::Shutdown()
{
    if (m_device == VK_NULL_HANDLE)
    {
        return;
    }

    vkDeviceWaitIdle(m_device);

    DestroyImGuiObjects();
    DestroySwapchain();
    DestroyPipelines();

    if (m_sceneDescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_device, m_sceneDescriptorPool, nullptr);
        m_sceneDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_presentDescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_device, m_presentDescriptorPool, nullptr);
        m_presentDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_sceneDescriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_device, m_sceneDescriptorSetLayout, nullptr);
        m_sceneDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_presentDescriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_device, m_presentDescriptorSetLayout, nullptr);
        m_presentDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_sceneSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(m_device, m_sceneSampler, nullptr);
        m_sceneSampler = VK_NULL_HANDLE;
    }

    DestroyBuffer(m_device, m_overlayBuffer);
    DestroyBuffer(m_device, m_repulsorBuffer);
    DestroyBuffer(m_device, m_bladeBuffer);
    DestroyBuffer(m_device, m_presentUniformBuffer);
    DestroyBuffer(m_device, m_sceneUniformBuffer);

    if (m_frameFence != VK_NULL_HANDLE)
    {
        vkDestroyFence(m_device, m_frameFence, nullptr);
        m_frameFence = VK_NULL_HANDLE;
    }
    if (m_renderFinishedSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphore, nullptr);
        m_renderFinishedSemaphore = VK_NULL_HANDLE;
    }
    if (m_imageAvailableSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_device, m_imageAvailableSemaphore, nullptr);
        m_imageAvailableSemaphore = VK_NULL_HANDLE;
    }
    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    vkDestroyDevice(m_device, nullptr);
    m_device = VK_NULL_HANDLE;

    if (m_surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::Resize(
    std::uint32_t windowWidth,
    std::uint32_t windowHeight,
    std::uint32_t renderWidth,
    std::uint32_t renderHeight
)
{
    if (windowWidth == 0 || windowHeight == 0 || renderWidth == 0 || renderHeight == 0)
    {
        return;
    }

    m_windowWidth = windowWidth;
    m_windowHeight = windowHeight;
    m_renderWidth = renderWidth;
    m_renderHeight = renderHeight;

    vkDeviceWaitIdle(m_device);
    DestroySwapchain();
    DestroyPipelines();
    CreateSwapchain(windowWidth, windowHeight);
    CreateSceneTargets(renderWidth, renderHeight);
    CreateRenderPasses();
    CreatePipelines();
    CreateFramebuffers();
    UpdateDescriptorSets();

    if (m_imguiInitialized)
    {
        ImGui_ImplVulkan_SetMinImageCount(static_cast<std::uint32_t>(m_swapchainImages.size()));
    }
}

void VulkanRenderer::ProcessImGuiEvent(const SDL_Event& event)
{
    if (m_imguiInitialized)
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
    }
}

void VulkanRenderer::BeginImGuiFrame()
{
    if (!m_imguiInitialized)
    {
        return;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void VulkanRenderer::Render(const FrameState& frame, std::span<const std::uint32_t> overlayPixels)
{
    if (m_swapchain == VK_NULL_HANDLE || m_sceneFramebuffer == VK_NULL_HANDLE)
    {
        return;
    }

    if (overlayPixels.size() != kOverlayPixelCount)
    {
        throw std::runtime_error("Overlay pixel buffer size mismatch");
    }

    std::memcpy(m_sceneUniformBuffer.mapped, &frame.scene, sizeof(frame.scene));
    std::memcpy(m_presentUniformBuffer.mapped, &frame.present, sizeof(frame.present));
    std::memcpy(m_repulsorBuffer.mapped, frame.repulsors.data(), sizeof(RepulsorGpu) * kMaxRepulsors);
    std::memcpy(
        m_overlayBuffer.mapped,
        overlayPixels.data(),
        sizeof(std::uint32_t) * overlayPixels.size()
    );

    CheckVk(vkWaitForFences(m_device, 1, &m_frameFence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
    CheckVk(vkResetFences(m_device, 1, &m_frameFence), "vkResetFences");

    std::uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        m_device,
        m_swapchain,
        UINT64_MAX,
        m_imageAvailableSemaphore,
        VK_NULL_HANDLE,
        &imageIndex
    );
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        Resize(m_windowWidth, m_windowHeight, m_renderWidth, m_renderHeight);
        return;
    }
    CheckVk(acquireResult, "vkAcquireNextImageKHR");

    CheckVk(vkResetCommandBuffer(m_commandBuffer, 0), "vkResetCommandBuffer");
    RecordCommandBuffer(imageIndex);

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &m_imageAvailableSemaphore,
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = 1,
        .pCommandBuffers = &m_commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &m_renderFinishedSemaphore,
    };
    CheckVk(vkQueueSubmit(m_queue, 1, &submitInfo, m_frameFence), "vkQueueSubmit");

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &m_renderFinishedSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &imageIndex,
    };
    const VkResult presentResult = vkQueuePresentKHR(m_queue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
    {
        Resize(m_windowWidth, m_windowHeight, m_renderWidth, m_renderHeight);
        return;
    }
    CheckVk(presentResult, "vkQueuePresentKHR");
}

bool VulkanRenderer::WantsMouseCapture() const
{
    return m_imguiInitialized && ImGui::GetIO().WantCaptureMouse;
}

bool VulkanRenderer::WantsKeyboardCapture() const
{
    return m_imguiInitialized && ImGui::GetIO().WantCaptureKeyboard;
}

void VulkanRenderer::CreateInstance()
{
    Uint32 extensionCount = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (extensions == nullptr)
    {
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");
    }

    VkApplicationInfo applicationInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "gpu-grass-cpp",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "none",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &applicationInfo,
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames = extensions,
    };
    CheckVk(vkCreateInstance(&createInfo, nullptr, &m_instance), "vkCreateInstance");
}

void VulkanRenderer::CreateSurface()
{
    if (!SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface))
    {
        throw std::runtime_error("SDL_Vulkan_CreateSurface failed");
    }
}

void VulkanRenderer::PickPhysicalDevice()
{
    std::uint32_t deviceCount = 0;
    CheckVk(vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    CheckVk(
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data()),
        "vkEnumeratePhysicalDevices"
    );

    for (VkPhysicalDevice device : devices)
    {
        m_physicalDevice = device;
        try
        {
            m_queueFamilyIndex = ChooseQueueFamily();
            return;
        }
        catch (const std::exception&)
        {
        }
    }

    throw std::runtime_error("No compatible Vulkan device found");
}

void VulkanRenderer::CreateDevice()
{
    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = m_queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    const std::array deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    };
    CheckVk(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device), "vkCreateDevice");
    vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);
}

void VulkanRenderer::CreateCommandObjects()
{
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_queueFamilyIndex,
    };
    CheckVk(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool), "vkCreateCommandPool");

    VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    CheckVk(
        vkAllocateCommandBuffers(m_device, &allocateInfo, &m_commandBuffer),
        "vkAllocateCommandBuffers"
    );
}

void VulkanRenderer::CreateSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    CheckVk(
        vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphore),
        "vkCreateSemaphore"
    );
    CheckVk(
        vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphore),
        "vkCreateSemaphore"
    );

    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    CheckVk(vkCreateFence(m_device, &fenceInfo, nullptr, &m_frameFence), "vkCreateFence");
}

void VulkanRenderer::CreateStaticBuffers(std::span<const GrassBladeGpu> blades)
{
    m_bladeBufferBytes = std::max<VkDeviceSize>(blades.size_bytes(), sizeof(GrassBladeGpu));
    m_repulsorBufferBytes = sizeof(RepulsorGpu) * kMaxRepulsors;
    m_overlayBufferBytes = sizeof(std::uint32_t) * kOverlayPixelCount;

    m_sceneUniformBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(SceneUniforms),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_presentUniformBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(PresentUniforms),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_bladeBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        m_bladeBufferBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_repulsorBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        m_repulsorBufferBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_overlayBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        m_overlayBufferBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );

    std::memset(m_bladeBuffer.mapped, 0, static_cast<std::size_t>(m_bladeBufferBytes));
    std::memset(m_repulsorBuffer.mapped, 0, static_cast<std::size_t>(m_repulsorBufferBytes));
    std::memset(m_overlayBuffer.mapped, 0, static_cast<std::size_t>(m_overlayBufferBytes));
    std::memcpy(m_bladeBuffer.mapped, blades.data(), blades.size_bytes());
}

void VulkanRenderer::CreateDescriptorObjects()
{
    const std::array sceneBindings = {
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    VkDescriptorSetLayoutCreateInfo sceneLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<std::uint32_t>(sceneBindings.size()),
        .pBindings = sceneBindings.data(),
    };
    CheckVk(
        vkCreateDescriptorSetLayout(m_device, &sceneLayoutInfo, nullptr, &m_sceneDescriptorSetLayout),
        "vkCreateDescriptorSetLayout"
    );

    const std::array scenePoolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
    };
    VkDescriptorPoolCreateInfo scenePoolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = static_cast<std::uint32_t>(scenePoolSizes.size()),
        .pPoolSizes = scenePoolSizes.data(),
    };
    CheckVk(vkCreateDescriptorPool(m_device, &scenePoolInfo, nullptr, &m_sceneDescriptorPool), "vkCreateDescriptorPool");

    VkDescriptorSetAllocateInfo sceneAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_sceneDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_sceneDescriptorSetLayout,
    };
    CheckVk(vkAllocateDescriptorSets(m_device, &sceneAllocateInfo, &m_sceneDescriptorSet), "vkAllocateDescriptorSets");

    const std::array presentBindings = {
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    VkDescriptorSetLayoutCreateInfo presentLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<std::uint32_t>(presentBindings.size()),
        .pBindings = presentBindings.data(),
    };
    CheckVk(
        vkCreateDescriptorSetLayout(
            m_device,
            &presentLayoutInfo,
            nullptr,
            &m_presentDescriptorSetLayout
        ),
        "vkCreateDescriptorSetLayout"
    );

    const std::array presentPoolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
    };
    VkDescriptorPoolCreateInfo presentPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = static_cast<std::uint32_t>(presentPoolSizes.size()),
        .pPoolSizes = presentPoolSizes.data(),
    };
    CheckVk(
        vkCreateDescriptorPool(m_device, &presentPoolInfo, nullptr, &m_presentDescriptorPool),
        "vkCreateDescriptorPool"
    );

    VkDescriptorSetAllocateInfo presentAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_presentDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_presentDescriptorSetLayout,
    };
    CheckVk(
        vkAllocateDescriptorSets(m_device, &presentAllocateInfo, &m_presentDescriptorSet),
        "vkAllocateDescriptorSets"
    );
}

void VulkanRenderer::CreateSwapchain(std::uint32_t width, std::uint32_t height)
{
    VkSurfaceCapabilitiesKHR capabilities;
    CheckVk(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities),
        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"
    );

    std::uint32_t formatCount = 0;
    CheckVk(
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr),
        "vkGetPhysicalDeviceSurfaceFormatsKHR"
    );
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    CheckVk(
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data()),
        "vkGetPhysicalDeviceSurfaceFormatsKHR"
    );

    std::uint32_t presentModeCount = 0;
    CheckVk(
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, nullptr),
        "vkGetPhysicalDeviceSurfacePresentModesKHR"
    );
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    CheckVk(
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            m_physicalDevice,
            m_surface,
            &presentModeCount,
            presentModes.data()
        ),
        "vkGetPhysicalDeviceSurfacePresentModesKHR"
    );

    const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(formats);
    const VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);
    m_swapchainExtent.width =
        std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    m_swapchainExtent.height =
        std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    std::uint32_t imageCount = std::max(2u, capabilities.minImageCount);
    if (capabilities.maxImageCount > 0)
    {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = m_surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = m_swapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
    };
    CheckVk(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain), "vkCreateSwapchainKHR");

    m_swapchainFormat = surfaceFormat.format;

    std::uint32_t swapchainImageCount = 0;
    CheckVk(vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchainImageCount, nullptr), "vkGetSwapchainImagesKHR");
    m_swapchainImages.resize(swapchainImageCount);
    CheckVk(
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchainImageCount, m_swapchainImages.data()),
        "vkGetSwapchainImagesKHR"
    );

    m_swapchainImageViews.resize(swapchainImageCount);
    m_swapchainImagePrimed.assign(swapchainImageCount, false);
    for (std::size_t index = 0; index < m_swapchainImages.size(); ++index)
    {
        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = m_swapchainImages[index],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = m_swapchainFormat,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };
        CheckVk(
            vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapchainImageViews[index]),
            "vkCreateImageView"
        );
    }
}

void VulkanRenderer::CreateSceneTargets(std::uint32_t width, std::uint32_t height)
{
    m_sceneColorTarget = CreateImage2D(
        m_physicalDevice,
        m_device,
        width,
        height,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );
    m_sceneDepthTarget = CreateImage2D(
        m_physicalDevice,
        m_device,
        width,
        height,
        m_depthFormat,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );
    m_sceneColorPrimed = false;
    m_sceneDepthPrimed = false;
}

void VulkanRenderer::CreateRenderPasses()
{
    VkAttachmentDescription sceneColorAttachment = {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentDescription sceneDepthAttachment = {
        .format = m_depthFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentReference colorReference = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentReference depthReference = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription sceneSubpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorReference,
        .pDepthStencilAttachment = &depthReference,
    };
    const std::array sceneAttachments = {sceneColorAttachment, sceneDepthAttachment};
    VkRenderPassCreateInfo sceneRenderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<std::uint32_t>(sceneAttachments.size()),
        .pAttachments = sceneAttachments.data(),
        .subpassCount = 1,
        .pSubpasses = &sceneSubpass,
    };
    CheckVk(vkCreateRenderPass(m_device, &sceneRenderPassInfo, nullptr, &m_sceneRenderPass), "vkCreateRenderPass");

    VkAttachmentDescription presentAttachment = {
        .format = m_swapchainFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentReference presentColorReference = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription presentSubpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &presentColorReference,
    };
    VkRenderPassCreateInfo presentRenderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &presentAttachment,
        .subpassCount = 1,
        .pSubpasses = &presentSubpass,
    };
    CheckVk(
        vkCreateRenderPass(m_device, &presentRenderPassInfo, nullptr, &m_presentRenderPass),
        "vkCreateRenderPass"
    );
}

void VulkanRenderer::CreatePipelines()
{
    const VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    const VkPipelineViewportStateCreateInfo viewportInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    const VkPipelineMultisampleStateCreateInfo multisampleInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    const VkPipelineColorBlendAttachmentState blendAttachment = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    const VkPipelineColorBlendStateCreateInfo blendInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment,
    };
    const std::array dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamicInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data(),
    };
    const VkPipelineDepthStencilStateCreateInfo depthInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
    };

    VkPipelineLayoutCreateInfo sceneLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &m_sceneDescriptorSetLayout,
    };
    CheckVk(vkCreatePipelineLayout(m_device, &sceneLayoutInfo, nullptr, &m_scenePipelineLayout), "vkCreatePipelineLayout");

    auto createScenePipeline = [&](std::string_view vertPath,
                                   std::string_view fragPath,
                                   VkCullModeFlags cullMode,
                                   VkPipeline* pipeline) {
        const VkShaderModule vertModule = CreateShaderModule(m_device, vertPath);
        const VkShaderModule fragModule = CreateShaderModule(m_device, fragPath);
        const std::array stages = {
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertModule,
                .pName = "main",
            },
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragModule,
                .pName = "main",
            },
        };
        const VkPipelineRasterizationStateCreateInfo rasterInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = cullMode,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.0f,
        };
        VkGraphicsPipelineCreateInfo pipelineInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = static_cast<std::uint32_t>(stages.size()),
            .pStages = stages.data(),
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssemblyInfo,
            .pViewportState = &viewportInfo,
            .pRasterizationState = &rasterInfo,
            .pMultisampleState = &multisampleInfo,
            .pDepthStencilState = &depthInfo,
            .pColorBlendState = &blendInfo,
            .pDynamicState = &dynamicInfo,
            .layout = m_scenePipelineLayout,
            .renderPass = m_sceneRenderPass,
            .subpass = 0,
        };
        CheckVk(
            vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, pipeline),
            "vkCreateGraphicsPipelines"
        );
        vkDestroyShaderModule(m_device, fragModule, nullptr);
        vkDestroyShaderModule(m_device, vertModule, nullptr);
    };

    createScenePipeline(
        kGroundVertShaderPath,
        kGroundFragShaderPath,
        VK_CULL_MODE_BACK_BIT,
        &m_groundPipeline
    );
    createScenePipeline(kGrassVertShaderPath, kGrassFragShaderPath, VK_CULL_MODE_NONE, &m_grassPipeline);
    createScenePipeline(
        kMarkerVertShaderPath,
        kMarkerFragShaderPath,
        VK_CULL_MODE_BACK_BIT,
        &m_markerPipeline
    );

    VkPipelineLayoutCreateInfo presentLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &m_presentDescriptorSetLayout,
    };
    CheckVk(
        vkCreatePipelineLayout(m_device, &presentLayoutInfo, nullptr, &m_presentPipelineLayout),
        "vkCreatePipelineLayout"
    );

    const VkShaderModule presentVertModule = CreateShaderModule(m_device, kPresentVertShaderPath);
    const VkShaderModule presentFragModule = CreateShaderModule(m_device, kPresentFragShaderPath);
    const std::array presentStages = {
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = presentVertModule,
            .pName = "main",
        },
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = presentFragModule,
            .pName = "main",
        },
    };
    const VkPipelineRasterizationStateCreateInfo presentRasterInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
    };
    const VkPipelineDepthStencilStateCreateInfo presentDepthInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };
    VkGraphicsPipelineCreateInfo presentPipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = static_cast<std::uint32_t>(presentStages.size()),
        .pStages = presentStages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssemblyInfo,
        .pViewportState = &viewportInfo,
        .pRasterizationState = &presentRasterInfo,
        .pMultisampleState = &multisampleInfo,
        .pDepthStencilState = &presentDepthInfo,
        .pColorBlendState = &blendInfo,
        .pDynamicState = &dynamicInfo,
        .layout = m_presentPipelineLayout,
        .renderPass = m_presentRenderPass,
        .subpass = 0,
    };
    CheckVk(
        vkCreateGraphicsPipelines(
            m_device,
            VK_NULL_HANDLE,
            1,
            &presentPipelineInfo,
            nullptr,
            &m_presentPipeline
        ),
        "vkCreateGraphicsPipelines"
    );
    vkDestroyShaderModule(m_device, presentFragModule, nullptr);
    vkDestroyShaderModule(m_device, presentVertModule, nullptr);
}

void VulkanRenderer::CreateFramebuffers()
{
    const std::array sceneAttachments = {m_sceneColorTarget.view, m_sceneDepthTarget.view};
    VkFramebufferCreateInfo sceneFramebufferInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = m_sceneRenderPass,
        .attachmentCount = static_cast<std::uint32_t>(sceneAttachments.size()),
        .pAttachments = sceneAttachments.data(),
        .width = m_renderWidth,
        .height = m_renderHeight,
        .layers = 1,
    };
    CheckVk(vkCreateFramebuffer(m_device, &sceneFramebufferInfo, nullptr, &m_sceneFramebuffer), "vkCreateFramebuffer");

    m_presentFramebuffers.resize(m_swapchainImageViews.size());
    for (std::size_t index = 0; index < m_swapchainImageViews.size(); ++index)
    {
        VkImageView attachment = m_swapchainImageViews[index];
        VkFramebufferCreateInfo presentFramebufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = m_presentRenderPass,
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .width = m_swapchainExtent.width,
            .height = m_swapchainExtent.height,
            .layers = 1,
        };
        CheckVk(
            vkCreateFramebuffer(m_device, &presentFramebufferInfo, nullptr, &m_presentFramebuffers[index]),
            "vkCreateFramebuffer"
        );
    }
}

void VulkanRenderer::CreateSampler()
{
    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .minLod = 0.0f,
        .maxLod = 0.0f,
    };
    CheckVk(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sceneSampler), "vkCreateSampler");
}

void VulkanRenderer::CreateImGuiObjects()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForVulkan(m_window))
    {
        throw std::runtime_error("ImGui_ImplSDL3_InitForVulkan failed");
    }

    const std::array poolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, 16},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 16},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 16},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 16},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 16},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 16},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 16},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 16},
    };
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 64,
        .poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };
    CheckVk(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_imguiDescriptorPool), "vkCreateDescriptorPool");

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = m_instance;
    initInfo.PhysicalDevice = m_physicalDevice;
    initInfo.Device = m_device;
    initInfo.QueueFamily = m_queueFamilyIndex;
    initInfo.Queue = m_queue;
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = m_imguiDescriptorPool;
    initInfo.RenderPass = m_presentRenderPass;
    initInfo.Subpass = 0;
    initInfo.MinImageCount = static_cast<std::uint32_t>(m_swapchainImages.size());
    initInfo.ImageCount = static_cast<std::uint32_t>(m_swapchainImages.size());
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = nullptr;
    if (!ImGui_ImplVulkan_Init(&initInfo))
    {
        throw std::runtime_error("ImGui_ImplVulkan_Init failed");
    }

    m_imguiInitialized = true;
}

void VulkanRenderer::UploadImGuiFonts()
{
    if (!ImGui_ImplVulkan_CreateFontsTexture())
    {
        throw std::runtime_error("ImGui_ImplVulkan_CreateFontsTexture failed");
    }
}

void VulkanRenderer::DestroySwapchain()
{
    if (m_sceneFramebuffer != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(m_device, m_sceneFramebuffer, nullptr);
        m_sceneFramebuffer = VK_NULL_HANDLE;
    }

    for (VkFramebuffer framebuffer : m_presentFramebuffers)
    {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
    m_presentFramebuffers.clear();

    DestroyImage(m_device, m_sceneDepthTarget);
    DestroyImage(m_device, m_sceneColorTarget);
    m_sceneColorPrimed = false;
    m_sceneDepthPrimed = false;

    for (VkImageView view : m_swapchainImageViews)
    {
        vkDestroyImageView(m_device, view, nullptr);
    }
    m_swapchainImageViews.clear();
    m_swapchainImages.clear();
    m_swapchainImagePrimed.clear();

    if (m_swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::DestroyPipelines()
{
    if (m_groundPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_groundPipeline, nullptr);
        m_groundPipeline = VK_NULL_HANDLE;
    }
    if (m_grassPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_grassPipeline, nullptr);
        m_grassPipeline = VK_NULL_HANDLE;
    }
    if (m_markerPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_markerPipeline, nullptr);
        m_markerPipeline = VK_NULL_HANDLE;
    }
    if (m_presentPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_presentPipeline, nullptr);
        m_presentPipeline = VK_NULL_HANDLE;
    }
    if (m_scenePipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_device, m_scenePipelineLayout, nullptr);
        m_scenePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_presentPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_device, m_presentPipelineLayout, nullptr);
        m_presentPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_sceneRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_device, m_sceneRenderPass, nullptr);
        m_sceneRenderPass = VK_NULL_HANDLE;
    }
    if (m_presentRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_device, m_presentRenderPass, nullptr);
        m_presentRenderPass = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::DestroyImGuiObjects()
{
    if (!m_imguiInitialized)
    {
        return;
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    m_imguiInitialized = false;

    if (m_imguiDescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_device, m_imguiDescriptorPool, nullptr);
        m_imguiDescriptorPool = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::UpdateDescriptorSets()
{
    VkDescriptorBufferInfo sceneUniformInfo = {
        .buffer = m_sceneUniformBuffer.buffer,
        .offset = 0,
        .range = sizeof(SceneUniforms),
    };
    VkDescriptorBufferInfo bladeInfo = {
        .buffer = m_bladeBuffer.buffer,
        .offset = 0,
        .range = m_bladeBufferBytes,
    };
    VkDescriptorBufferInfo repulsorInfo = {
        .buffer = m_repulsorBuffer.buffer,
        .offset = 0,
        .range = m_repulsorBufferBytes,
    };
    const std::array sceneWrites = {
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_sceneDescriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &sceneUniformInfo,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_sceneDescriptorSet,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &bladeInfo,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_sceneDescriptorSet,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &repulsorInfo,
        },
    };
    vkUpdateDescriptorSets(
        m_device,
        static_cast<std::uint32_t>(sceneWrites.size()),
        sceneWrites.data(),
        0,
        nullptr
    );

    VkDescriptorImageInfo sceneColorInfo = {
        .sampler = m_sceneSampler,
        .imageView = m_sceneColorTarget.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkDescriptorBufferInfo presentUniformInfo = {
        .buffer = m_presentUniformBuffer.buffer,
        .offset = 0,
        .range = sizeof(PresentUniforms),
    };
    VkDescriptorBufferInfo overlayInfo = {
        .buffer = m_overlayBuffer.buffer,
        .offset = 0,
        .range = m_overlayBufferBytes,
    };
    const std::array presentWrites = {
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_presentDescriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &sceneColorInfo,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_presentDescriptorSet,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &presentUniformInfo,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_presentDescriptorSet,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &overlayInfo,
        },
    };
    vkUpdateDescriptorSets(
        m_device,
        static_cast<std::uint32_t>(presentWrites.size()),
        presentWrites.data(),
        0,
        nullptr
    );
}

void VulkanRenderer::RecordCommandBuffer(std::uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    CheckVk(vkBeginCommandBuffer(m_commandBuffer, &beginInfo), "vkBeginCommandBuffer");

    const VkImageLayout sceneColorOldLayout =
        m_sceneColorPrimed ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    const VkImageLayout sceneDepthOldLayout = m_sceneDepthPrimed
                                                  ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                                  : VK_IMAGE_LAYOUT_UNDEFINED;
    const VkImageLayout swapchainOldLayout = m_swapchainImagePrimed[imageIndex]
                                                 ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                                                 : VK_IMAGE_LAYOUT_UNDEFINED;

    TransitionImage(
        m_commandBuffer,
        m_sceneColorTarget.image,
        sceneColorOldLayout,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        m_sceneColorPrimed ? VK_ACCESS_SHADER_READ_BIT : 0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        m_sceneColorPrimed ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );
    TransitionImage(
        m_commandBuffer,
        m_sceneDepthTarget.image,
        sceneDepthOldLayout,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        0,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );
    TransitionImage(
        m_commandBuffer,
        m_swapchainImages[imageIndex],
        swapchainOldLayout,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    const std::array<VkClearValue, 2> sceneClearValues = {
        VkClearValue{.color = {{0.54f, 0.71f, 0.93f, 1.0f}}},
        VkClearValue{.depthStencil = {1.0f, 0}},
    };
    VkRenderPassBeginInfo scenePassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = m_sceneRenderPass,
        .framebuffer = m_sceneFramebuffer,
        .renderArea = {.offset = {0, 0}, .extent = {m_renderWidth, m_renderHeight}},
        .clearValueCount = static_cast<std::uint32_t>(sceneClearValues.size()),
        .pClearValues = sceneClearValues.data(),
    };
    vkCmdBeginRenderPass(m_commandBuffer, &scenePassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport sceneViewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(m_renderWidth),
        .height = static_cast<float>(m_renderHeight),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D sceneScissor = {.offset = {0, 0}, .extent = {m_renderWidth, m_renderHeight}};
    vkCmdSetViewport(m_commandBuffer, 0, 1, &sceneViewport);
    vkCmdSetScissor(m_commandBuffer, 0, 1, &sceneScissor);

    vkCmdBindDescriptorSets(
        m_commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_scenePipelineLayout,
        0,
        1,
        &m_sceneDescriptorSet,
        0,
        nullptr
    );

    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_groundPipeline);
    vkCmdDraw(m_commandBuffer, 6, 1, 0, 0);

    const SceneUniforms* sceneUniforms = static_cast<const SceneUniforms*>(m_sceneUniformBuffer.mapped);
    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_grassPipeline);
    vkCmdDraw(m_commandBuffer, kVerticesPerBlade, sceneUniforms->counts.x, 0, 0);

    if (sceneUniforms->counts.y > 0)
    {
        vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_markerPipeline);
        vkCmdDraw(m_commandBuffer, 24, sceneUniforms->counts.y, 0, 0);
    }

    vkCmdEndRenderPass(m_commandBuffer);

    TransitionImage(
        m_commandBuffer,
        m_sceneColorTarget.image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    const VkClearValue presentClearValue = {.color = {{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo presentPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = m_presentRenderPass,
        .framebuffer = m_presentFramebuffers[imageIndex],
        .renderArea = {.offset = {0, 0}, .extent = m_swapchainExtent},
        .clearValueCount = 1,
        .pClearValues = &presentClearValue,
    };
    vkCmdBeginRenderPass(m_commandBuffer, &presentPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport presentViewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(m_swapchainExtent.width),
        .height = static_cast<float>(m_swapchainExtent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D presentScissor = {.offset = {0, 0}, .extent = m_swapchainExtent};
    vkCmdSetViewport(m_commandBuffer, 0, 1, &presentViewport);
    vkCmdSetScissor(m_commandBuffer, 0, 1, &presentScissor);

    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_presentPipeline);
    vkCmdBindDescriptorSets(
        m_commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_presentPipelineLayout,
        0,
        1,
        &m_presentDescriptorSet,
        0,
        nullptr
    );
    vkCmdDraw(m_commandBuffer, 3, 1, 0, 0);

    if (m_imguiInitialized)
    {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffer);
    }

    vkCmdEndRenderPass(m_commandBuffer);

    TransitionImage(
        m_commandBuffer,
        m_swapchainImages[imageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        0,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    CheckVk(vkEndCommandBuffer(m_commandBuffer), "vkEndCommandBuffer");
    m_sceneColorPrimed = true;
    m_sceneDepthPrimed = true;
    m_swapchainImagePrimed[imageIndex] = true;
}

void VulkanRenderer::SubmitImmediate(const std::function<void(VkCommandBuffer)>& recorder)
{
    VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    CheckVk(
        vkAllocateCommandBuffers(m_device, &allocateInfo, &commandBuffer),
        "vkAllocateCommandBuffers"
    );

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    CheckVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer");
    recorder(commandBuffer);
    CheckVk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
    };
    CheckVk(vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit");
    CheckVk(vkQueueWaitIdle(m_queue), "vkQueueWaitIdle");
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
}

std::uint32_t VulkanRenderer::ChooseQueueFamily() const
{
    std::uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &familyCount, nullptr);

    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &familyCount, families.data());

    for (std::uint32_t index = 0; index < familyCount; ++index)
    {
        VkBool32 supportsPresent = VK_FALSE;
        CheckVk(
            vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, index, m_surface, &supportsPresent),
            "vkGetPhysicalDeviceSurfaceSupportKHR"
        );

        const VkQueueFlags flags = families[index].queueFlags;
        if ((flags & VK_QUEUE_GRAPHICS_BIT) != 0 && supportsPresent == VK_TRUE)
        {
            return index;
        }
    }

    throw std::runtime_error("No graphics+present queue family found");
}

VkSurfaceFormatKHR VulkanRenderer::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
{
    for (const VkSurfaceFormatKHR& format : formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM)
        {
            return format;
        }
    }

    return formats.front();
}

VkPresentModeKHR VulkanRenderer::ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) const
{
    for (const VkPresentModeKHR mode : presentModes)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkFormat VulkanRenderer::ChooseDepthFormat() const
{
    const std::array candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (VkFormat format : candidates)
    {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &properties);
        if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
        {
            return format;
        }
    }

    throw std::runtime_error("No compatible depth format found");
}

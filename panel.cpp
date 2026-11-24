#include "../sdk/brick/panel.h"
#include <algorithm>
#include <stdexcept>

std::vector<const char*> Panel::instanceExtensions() {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) throw std::runtime_error(SDL_GetError());
    window = SDL_CreateWindow("Rally Cross", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920,1080,
                              SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) throw std::runtime_error(SDL_GetError());
    unsigned count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window,&count,nullptr)) throw std::runtime_error(SDL_GetError());
    std::vector<const char*> extensions(count);
    if (!SDL_Vulkan_GetInstanceExtensions(window,&count,extensions.data())) throw std::runtime_error(SDL_GetError());
    SDL_ShowCursor(SDL_DISABLE);
    return extensions;
}

bool Panel::init(vk::Instance inst, vk::PhysicalDevice gpu, vk::Device dev, uint32_t family, bool native, std::string& error, uint32_t smallWidth) {
    device = dev;
    instance = inst;
    VkSurfaceKHR handle{};
    if (!SDL_Vulkan_CreateSurface(window,instance,&handle)) {error=SDL_GetError();return false;}
    surface=handle;
    if (!gpu.getSurfaceSupportKHR(family,surface)) {error="Queue cannot present on this display";return false;}
    vkQueue=dev.getQueue(family,0);
    const auto caps=gpu.getSurfaceCapabilitiesKHR(surface);
    auto formats=gpu.getSurfaceFormatsKHR(surface);
    auto preferred=std::find_if(formats.begin(),formats.end(),[](auto f){return f.format==format();});
    const auto targetFormat=preferred==formats.end()?formats.front():*preferred;
    int w,h;SDL_Vulkan_GetDrawableSize(window,&w,&h);
    output=caps.currentExtent.width==UINT32_MAX ? vk::Extent2D(w,h) : caps.currentExtent;
    if (!(caps.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferDst)) {error="Surface cannot receive a blit";return false;}
    uint32_t count=caps.minImageCount+1;
    if(caps.maxImageCount) count=std::min(count,caps.maxImageCount);
    auto alpha=vk::CompositeAlphaFlagBitsKHR::eOpaque;
    if (!(caps.supportedCompositeAlpha & alpha)) alpha=static_cast<vk::CompositeAlphaFlagBitsKHR>(uint32_t(caps.supportedCompositeAlpha) & -uint32_t(caps.supportedCompositeAlpha));
    swapchain=dev.createSwapchainKHRUnique(vk::SwapchainCreateInfoKHR({},surface,count,targetFormat.format,targetFormat.colorSpace,
               output,1,vk::ImageUsageFlagBits::eTransferDst,vk::SharingMode::eExclusive,{},caps.currentTransform,alpha,vk::PresentModeKHR::eFifo,true));
    targets=dev.getSwapchainImagesKHR(*swapchain);
    var.xres=1024; var.yres=768;
    mode.xres=native?1024:smallWidth;mode.yres=mode.xres*3/4;
    for (unsigned i=0;i<Pages;++i) {
        auto im=dev.createImageUnique(vk::ImageCreateInfo({},vk::ImageType::e2D,format(),vk::Extent3D(mode.xres,mode.yres,1),1,1,
            vk::SampleCountFlagBits::e1,vk::ImageTiling::eOptimal,vk::ImageUsageFlagBits::eColorAttachment|vk::ImageUsageFlagBits::eTransferSrc|vk::ImageUsageFlagBits::eTransferDst));
        auto req=dev.getImageMemoryRequirements(*im);
        unsigned type=0;auto props=gpu.getMemoryProperties();
        while(type<props.memoryTypeCount && (!(req.memoryTypeBits & (1u<<type)) || !(props.memoryTypes[type].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal))) ++type;
        if(type==props.memoryTypeCount) throw std::runtime_error("No GPU memory for the frame");
        pageMemory.push_back(dev.allocateMemoryUnique(vk::MemoryAllocateInfo(req.size,type)));
        dev.bindImageMemory(*im,*pageMemory.back(),0);
        views.push_back(dev.createImageViewUnique(vk::ImageViewCreateInfo({},*im,vk::ImageViewType::e2D,format(),{},vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor,0,1,0,1))));
        images.push_back(std::move(im));
    }
    copyPool=dev.createCommandPoolUnique(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer,family));
    copyCommands=dev.allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(*copyPool,vk::CommandBufferLevel::ePrimary,1));
    available=dev.createSemaphoreUnique({}); rendered=dev.createSemaphoreUnique({});
    fprintf(stderr,"Ally Vulkan: %ux%u -> %ux%u\n",mode.xres,mode.yres,output.width,output.height);
    return true;
}
void Panel::queue(uint32_t page,vk::Fence) {
    auto acquired=device.acquireNextImageKHR(*swapchain,UINT64_MAX,*available,{});
    auto dst=targets.at(acquired.value); auto cmd=*copyCommands[0];
    cmd.reset();cmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    const vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor,0,1,0,1);
    auto barrier=[&](vk::Image img,vk::ImageLayout from,vk::ImageLayout to,vk::AccessFlags src,vk::AccessFlags dest){
        vk::ImageMemoryBarrier b(src,dest,from,to,VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED,img,range);
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,vk::PipelineStageFlagBits::eAllCommands,{},nullptr,nullptr,b);
    };
    auto transfer=vk::ImageLayout::eTransferDstOptimal, general=vk::ImageLayout::eGeneral;
    barrier(dst,vk::ImageLayout::eUndefined,transfer,{},vk::AccessFlagBits::eTransferWrite);
    cmd.clearColorImage(dst,transfer,vk::ClearColorValue(std::array<float,4>{0,0,0,1}),range);
    barrier(dst,transfer,transfer,vk::AccessFlagBits::eTransferWrite,vk::AccessFlagBits::eTransferWrite);
    barrier(*images[page],general,general,vk::AccessFlagBits::eColorAttachmentWrite,vk::AccessFlagBits::eTransferRead);
    int width=std::min(output.width,output.height*mode.xres/mode.yres), height=width*mode.yres/mode.xres;
    int left=(int(output.width)-width)/2, top=(int(output.height)-height)/2;
    vk::ImageSubresourceLayers layer(vk::ImageAspectFlagBits::eColor,0,0,1);
    vk::ImageBlit region(layer, {vk::Offset3D(),vk::Offset3D(mode.xres,mode.yres,1)},layer,
                          {vk::Offset3D(left,top,0),vk::Offset3D(left+width,top+height,1)});
    cmd.blitImage(*images[page],general,dst,transfer,region,vk::Filter::eLinear);
    barrier(dst,transfer,vk::ImageLayout::ePresentSrcKHR,vk::AccessFlagBits::eTransferWrite,{});
    cmd.end();
    vk::PipelineStageFlags wait=vk::PipelineStageFlagBits::eTransfer;
    vkQueue.submit(vk::SubmitInfo(*available,wait,cmd,*rendered));
    (void)vkQueue.presentKHR(vk::PresentInfoKHR(*rendered,*swapchain,acquired.value));
    vkQueue.waitIdle(); ++shownCount;
}
void Panel::term(){
    if(device){device.waitIdle();views.clear();images.clear();pageMemory.clear();copyCommands.clear();copyPool.reset();
        available.reset();rendered.reset();swapchain.reset();if(surface)instance.destroySurfaceKHR(surface);surface=nullptr;device=nullptr;}
    if(window){SDL_DestroyWindow(window);window=nullptr;}
}
bool Panel::matchRate(){refresh=59.94f;return true;}
void Panel::waitWritable(uint32_t){}
void Panel::waitDrawn(uint32_t){}
void Panel::drain(){if(device)vkQueue.waitIdle();}
bool Panel::readShown(std::vector<uint8_t>&,int&,int&){return false;}
bool Panel::beginPictures(int&,int&){return false;}
void Panel::showPicture(const uint32_t*){}
void Panel::endPictures(After){}
void Panel::setCorners(std::vector<Corner>){}
uint64_t Panel::framesShown(){return shownCount;}

/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#pragma once

/// \file
/// Declaration of Diligent::DeviceContextVkImpl class

#include <unordered_map>
#include <bitset>

#include "EngineVkImplTraits.hpp"

#include "DeviceContextVk.h"
#include "DeviceContextNextGenBase.hpp"

// Vk object implementations are required by DeviceContextBase
#include "BufferVkImpl.hpp"
#include "TextureVkImpl.hpp"
#include "PipelineStateVkImpl.hpp"
#include "QueryVkImpl.hpp"
#include "FramebufferVkImpl.hpp"
#include "RenderPassVkImpl.hpp"
#include "BottomLevelASVkImpl.hpp"
#include "TopLevelASVkImpl.hpp"
#include "ShaderBindingTableVkImpl.hpp"
#include "ShaderResourceBindingVkImpl.hpp"

#include "GenerateMipsVkHelper.hpp"
#include "PipelineLayoutVk.hpp"
#include "VulkanUtilities/VulkanCommandBufferPool.hpp"
#include "VulkanUtilities/VulkanCommandBuffer.hpp"
#include "VulkanUploadHeap.hpp"
#include "VulkanDynamicHeap.hpp"
#include "ResourceReleaseQueue.hpp"
#include "DescriptorPoolManager.hpp"
#include "HashUtils.hpp"
#include "ManagedVulkanObject.hpp"
#include "QueryManagerVk.hpp"

namespace Diligent
{

/// Device context implementation in Vulkan backend.
class DeviceContextVkImpl final : public DeviceContextNextGenBase<EngineVkImplTraits>
{
public:
    using TDeviceContextBase = DeviceContextNextGenBase<EngineVkImplTraits>;

    DeviceContextVkImpl(IReferenceCounters*                   pRefCounters,
                        RenderDeviceVkImpl*                   pDevice,
                        bool                                  bIsDeferred,
                        const EngineVkCreateInfo&             EngineCI,
                        Uint32                                ContextId,
                        Uint32                                CommandQueueId,
                        std::shared_ptr<GenerateMipsVkHelper> GenerateMipsHelper);
    ~DeviceContextVkImpl();

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_DeviceContextVk, TDeviceContextBase)

    /// Implementation of IDeviceContext::SetPipelineState() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE SetPipelineState(IPipelineState* pPipelineState) override final;

    /// Implementation of IDeviceContext::TransitionShaderResources() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE TransitionShaderResources(IPipelineState*         pPipelineState,
                                                              IShaderResourceBinding* pShaderResourceBinding) override final;

    /// Implementation of IDeviceContext::CommitShaderResources() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE CommitShaderResources(IShaderResourceBinding*        pShaderResourceBinding,
                                                          RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) override final;

    /// Implementation of IDeviceContext::SetStencilRef() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE SetStencilRef(Uint32 StencilRef) override final;

    /// Implementation of IDeviceContext::SetBlendFactors() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE SetBlendFactors(const float* pBlendFactors = nullptr) override final;

    /// Implementation of IDeviceContext::SetVertexBuffers() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE SetVertexBuffers(Uint32                         StartSlot,
                                                     Uint32                         NumBuffersSet,
                                                     IBuffer**                      ppBuffers,
                                                     Uint32*                        pOffsets,
                                                     RESOURCE_STATE_TRANSITION_MODE StateTransitionMode,
                                                     SET_VERTEX_BUFFERS_FLAGS       Flags) override final;

    /// Implementation of IDeviceContext::InvalidateState() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE InvalidateState() override final;

    /// Implementation of IDeviceContext::SetIndexBuffer() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE SetIndexBuffer(IBuffer*                       pIndexBuffer,
                                                   Uint32                         ByteOffset,
                                                   RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) override final;

    /// Implementation of IDeviceContext::SetViewports() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE SetViewports(Uint32          NumViewports,
                                                 const Viewport* pViewports,
                                                 Uint32          RTWidth,
                                                 Uint32          RTHeight) override final;

    /// Implementation of IDeviceContext::SetScissorRects() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE SetScissorRects(Uint32      NumRects,
                                                    const Rect* pRects,
                                                    Uint32      RTWidth,
                                                    Uint32      RTHeight) override final;

    /// Implementation of IDeviceContext::SetRenderTargets() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE SetRenderTargets(Uint32                         NumRenderTargets,
                                                     ITextureView*                  ppRenderTargets[],
                                                     ITextureView*                  pDepthStencil,
                                                     RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) override final;

    /// Implementation of IDeviceContext::BeginRenderPass() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE BeginRenderPass(const BeginRenderPassAttribs& Attribs) override final;

    /// Implementation of IDeviceContext::NextSubpass() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE NextSubpass() override final;

    /// Implementation of IDeviceContext::EndRenderPass() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE EndRenderPass() override final;

    // clang-format off
    /// Implementation of IDeviceContext::Draw() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE Draw               (const DrawAttribs& Attribs) override final;
    /// Implementation of IDeviceContext::DrawIndexed() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE DrawIndexed        (const DrawIndexedAttribs& Attribs) override final;
    /// Implementation of IDeviceContext::DrawIndirect() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE DrawIndirect       (const DrawIndirectAttribs& Attribs, IBuffer* pAttribsBuffer) override final;
    /// Implementation of IDeviceContext::DrawIndexedIndirect() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE DrawIndexedIndirect(const DrawIndexedIndirectAttribs& Attribs, IBuffer* pAttribsBuffer) override final;
    /// Implementation of IDeviceContext::DrawMesh() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE DrawMesh           (const DrawMeshAttribs& Attribs) override final;
    /// Implementation of IDeviceContext::DrawMeshIndirect() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE DrawMeshIndirect   (const DrawMeshIndirectAttribs& Attribs, IBuffer* pAttribsBuffer) override final;

    /// Implementation of IDeviceContext::DispatchCompute() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE DispatchCompute        (const DispatchComputeAttribs& Attribs) override final;
    /// Implementation of IDeviceContext::DispatchComputeIndirect() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE DispatchComputeIndirect(const DispatchComputeIndirectAttribs& Attribs, IBuffer* pAttribsBuffer) override final;
    // clang-format on

    /// Implementation of IDeviceContext::ClearDepthStencil() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE ClearDepthStencil(ITextureView*                  pView,
                                                      CLEAR_DEPTH_STENCIL_FLAGS      ClearFlags,
                                                      float                          fDepth,
                                                      Uint8                          Stencil,
                                                      RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) override final;

    /// Implementation of IDeviceContext::ClearRenderTarget() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE ClearRenderTarget(ITextureView*                  pView,
                                                      const float*                   RGBA,
                                                      RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) override final;

    /// Implementation of IDeviceContext::UpdateBuffer() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE UpdateBuffer(IBuffer*                       pBuffer,
                                                 Uint32                         Offset,
                                                 Uint32                         Size,
                                                 const void*                    pData,
                                                 RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) override final;

    /// Implementation of IDeviceContext::CopyBuffer() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE CopyBuffer(IBuffer*                       pSrcBuffer,
                                               Uint32                         SrcOffset,
                                               RESOURCE_STATE_TRANSITION_MODE SrcBufferTransitionMode,
                                               IBuffer*                       pDstBuffer,
                                               Uint32                         DstOffset,
                                               Uint32                         Size,
                                               RESOURCE_STATE_TRANSITION_MODE DstBufferTransitionMode) override final;

    /// Implementation of IDeviceContext::MapBuffer() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE MapBuffer(IBuffer*  pBuffer,
                                              MAP_TYPE  MapType,
                                              MAP_FLAGS MapFlags,
                                              PVoid&    pMappedData) override final;

    /// Implementation of IDeviceContext::UnmapBuffer() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE UnmapBuffer(IBuffer* pBuffer, MAP_TYPE MapType) override final;

    /// Implementation of IDeviceContext::UpdateTexture() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE UpdateTexture(ITexture*                      pTexture,
                                                  Uint32                         MipLevel,
                                                  Uint32                         Slice,
                                                  const Box&                     DstBox,
                                                  const TextureSubResData&       SubresData,
                                                  RESOURCE_STATE_TRANSITION_MODE SrcBufferStateTransitionMode,
                                                  RESOURCE_STATE_TRANSITION_MODE TextureStateTransitionModee) override final;

    /// Implementation of IDeviceContext::CopyTexture() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE CopyTexture(const CopyTextureAttribs& CopyAttribs) override final;

    /// Implementation of IDeviceContext::MapTextureSubresource() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE MapTextureSubresource(ITexture*                 pTexture,
                                                          Uint32                    MipLevel,
                                                          Uint32                    ArraySlice,
                                                          MAP_TYPE                  MapType,
                                                          MAP_FLAGS                 MapFlags,
                                                          const Box*                pMapRegion,
                                                          MappedTextureSubresource& MappedData) override final;

    /// Implementation of IDeviceContext::UnmapTextureSubresource() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE UnmapTextureSubresource(ITexture* pTexture, Uint32 MipLevel, Uint32 ArraySlice) override final;

    /// Implementation of IDeviceContext::FinishCommandList() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE FinishCommandList(class ICommandList** ppCommandList) override final;

    /// Implementation of IDeviceContext::ExecuteCommandLists() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE ExecuteCommandLists(Uint32               NumCommandLists,
                                                        ICommandList* const* ppCommandLists) override final;

    /// Implementation of IDeviceContext::SignalFence() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE SignalFence(IFence* pFence, Uint64 Value) override final;

    /// Implementation of IDeviceContext::WaitForFence() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE WaitForFence(IFence* pFence, Uint64 Value, bool FlushContext) override final;

    /// Implementation of IDeviceContext::WaitForIdle() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE WaitForIdle() override final;

    /// Implementation of IDeviceContext::BeginQuery() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE BeginQuery(IQuery* pQuery) override final;

    /// Implementation of IDeviceContext::EndQuery() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE EndQuery(IQuery* pQuery) override final;

    /// Implementation of IDeviceContext::Flush() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE Flush() override final;

    /// Implementation of IDeviceContext::BuildBLAS() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE BuildBLAS(const BuildBLASAttribs& Attribs) override final;

    /// Implementation of IDeviceContext::BuildTLAS() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE BuildTLAS(const BuildTLASAttribs& Attribs) override final;

    /// Implementation of IDeviceContext::CopyBLAS() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE CopyBLAS(const CopyBLASAttribs& Attribs) override final;

    /// Implementation of IDeviceContext::CopyTLAS() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE CopyTLAS(const CopyTLASAttribs& Attribs) override final;

    /// Implementation of IDeviceContext::WriteBLASCompactedSize() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE WriteBLASCompactedSize(const WriteBLASCompactedSizeAttribs& Attribs) override final;

    /// Implementation of IDeviceContext::WriteTLASCompactedSize() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE WriteTLASCompactedSize(const WriteTLASCompactedSizeAttribs& Attribs) override final;

    /// Implementation of IDeviceContext::TraceRays() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE TraceRays(const TraceRaysAttribs& Attribs) override final;

    // Transitions texture subresources from OldState to NewState, and optionally updates
    // internal texture state.
    // If OldState == RESOURCE_STATE_UNKNOWN, internal texture state is used as old state.
    void TransitionTextureState(TextureVkImpl&           TextureVk,
                                RESOURCE_STATE           OldState,
                                RESOURCE_STATE           NewState,
                                bool                     UpdateTextureState,
                                VkImageSubresourceRange* pSubresRange = nullptr);

    void TransitionImageLayout(TextureVkImpl&                 TextureVk,
                               VkImageLayout                  OldLayout,
                               VkImageLayout                  NewLayout,
                               const VkImageSubresourceRange& SubresRange);

    /// Implementation of IDeviceContextVk::TransitionImageLayout().
    virtual void DILIGENT_CALL_TYPE TransitionImageLayout(ITexture* pTexture, VkImageLayout NewLayout) override final;


    // Transitions buffer state from OldState to NewState, and optionally updates
    // internal buffer state.
    // If OldState == RESOURCE_STATE_UNKNOWN, internal buffer state is used as old state.
    void TransitionBufferState(BufferVkImpl&  BufferVk,
                               RESOURCE_STATE OldState,
                               RESOURCE_STATE NewState,
                               bool           UpdateBufferState);

    /// Implementation of IDeviceContextVk::BufferMemoryBarrier().
    virtual void DILIGENT_CALL_TYPE BufferMemoryBarrier(IBuffer* pBuffer, VkAccessFlags NewAccessFlags) override final;


    // Transitions BLAS state from OldState to NewState, and optionally updates internal state.
    // If OldState == RESOURCE_STATE_UNKNOWN, internal BLAS state is used as old state.
    void TransitionBLASState(BottomLevelASVkImpl& BLAS,
                             RESOURCE_STATE       OldState,
                             RESOURCE_STATE       NewState,
                             bool                 UpdateInternalState);

    // Transitions TLAS state from OldState to NewState, and optionally updates internal state.
    // If OldState == RESOURCE_STATE_UNKNOWN, internal TLAS state is used as old state.
    void TransitionTLASState(TopLevelASVkImpl& TLAS,
                             RESOURCE_STATE    OldState,
                             RESOURCE_STATE    NewState,
                             bool              UpdateInternalState);

    void AddWaitSemaphore(ManagedSemaphore* pWaitSemaphore, VkPipelineStageFlags WaitDstStageMask)
    {
        VERIFY_EXPR(pWaitSemaphore != nullptr);
        m_WaitSemaphores.emplace_back(pWaitSemaphore);
        m_VkWaitSemaphores.push_back(pWaitSemaphore->Get());
        m_WaitDstStageMasks.push_back(WaitDstStageMask);
    }
    void AddSignalSemaphore(ManagedSemaphore* pSignalSemaphore)
    {
        VERIFY_EXPR(pSignalSemaphore != nullptr);
        m_SignalSemaphores.emplace_back(pSignalSemaphore);
        m_VkSignalSemaphores.push_back(pSignalSemaphore->Get());
    }

    void UpdateBufferRegion(BufferVkImpl*                  pBuffVk,
                            Uint64                         DstOffset,
                            Uint64                         NumBytes,
                            VkBuffer                       vkSrcBuffer,
                            Uint64                         SrcOffset,
                            RESOURCE_STATE_TRANSITION_MODE TransitionMode);

    void CopyTextureRegion(TextureVkImpl*                 pSrcTexture,
                           RESOURCE_STATE_TRANSITION_MODE SrcTextureTransitionMode,
                           TextureVkImpl*                 pDstTexture,
                           RESOURCE_STATE_TRANSITION_MODE DstTextureTransitionMode,
                           const VkImageCopy&             CopyRegion);

    void UpdateTextureRegion(const void*                    pSrcData,
                             Uint32                         SrcStride,
                             Uint32                         SrcDepthStride,
                             TextureVkImpl&                 TextureVk,
                             Uint32                         MipLevel,
                             Uint32                         Slice,
                             const Box&                     DstBox,
                             RESOURCE_STATE_TRANSITION_MODE TextureTransitionMode);

    virtual void DILIGENT_CALL_TYPE GenerateMips(ITextureView* pTexView) override final;

    Uint32 GetContextId() const { return m_ContextId; }

    size_t GetNumCommandsInCtx() const { return m_State.NumCommands; }

    __forceinline VulkanUtilities::VulkanCommandBuffer& GetCommandBuffer()
    {
        EnsureVkCmdBuffer();
        m_CommandBuffer.FlushBarriers();
        return m_CommandBuffer;
    }

    virtual void DILIGENT_CALL_TYPE FinishFrame() override final;

    virtual void DILIGENT_CALL_TYPE TransitionResourceStates(Uint32 BarrierCount, StateTransitionDesc* pResourceBarriers) override final;

    virtual void DILIGENT_CALL_TYPE ResolveTextureSubresource(ITexture*                               pSrcTexture,
                                                              ITexture*                               pDstTexture,
                                                              const ResolveTextureSubresourceAttribs& ResolveAttribs) override final;

    VkDescriptorSet AllocateDynamicDescriptorSet(VkDescriptorSetLayout SetLayout, const char* DebugName = "")
    {
        // Descriptor pools are externally synchronized, meaning that the application must not allocate
        // and/or free descriptor sets from the same pool in multiple threads simultaneously (13.2.3)
        return m_DynamicDescrSetAllocator.Allocate(SetLayout, DebugName);
    }

    VulkanDynamicAllocation AllocateDynamicSpace(Uint32 SizeInBytes, Uint32 Alignment);

    virtual void ResetRenderTargets() override final;

    GenerateMipsVkHelper& GetGenerateMipsHelper() { return *m_GenerateMipsHelper; }
    QueryManagerVk*       GetQueryManager() { return m_QueryMgr.get(); }

private:
    void               TransitionRenderTargets(RESOURCE_STATE_TRANSITION_MODE StateTransitionMode);
    __forceinline void CommitRenderPassAndFramebuffer(bool VerifyStates);
    void               CommitVkVertexBuffers();
    void               CommitViewports();
    void               CommitScissorRects();

    void Flush(Uint32               NumCommandLists,
               ICommandList* const* ppCommandLists);

    __forceinline void TransitionOrVerifyBufferState(BufferVkImpl&                  Buffer,
                                                     RESOURCE_STATE_TRANSITION_MODE TransitionMode,
                                                     RESOURCE_STATE                 RequiredState,
                                                     VkAccessFlagBits               ExpectedAccessFlags,
                                                     const char*                    OperationName);

    __forceinline void TransitionOrVerifyTextureState(TextureVkImpl&                 Texture,
                                                      RESOURCE_STATE_TRANSITION_MODE TransitionMode,
                                                      RESOURCE_STATE                 RequiredState,
                                                      VkImageLayout                  ExpectedLayout,
                                                      const char*                    OperationName);

    __forceinline void TransitionOrVerifyBLASState(BottomLevelASVkImpl&           BLAS,
                                                   RESOURCE_STATE_TRANSITION_MODE TransitionMode,
                                                   RESOURCE_STATE                 RequiredState,
                                                   const char*                    OperationName);

    __forceinline void TransitionOrVerifyTLASState(TopLevelASVkImpl&              TLAS,
                                                   RESOURCE_STATE_TRANSITION_MODE TransitionMode,
                                                   RESOURCE_STATE                 RequiredState,
                                                   const char*                    OperationName);

    __forceinline void EnsureVkCmdBuffer()
    {
        // Make sure that the number of commands in the context is at least one,
        // so that the context cannot be disposed by Flush()
        m_State.NumCommands = m_State.NumCommands != 0 ? m_State.NumCommands : 1;
        if (m_CommandBuffer.GetVkCmdBuffer() == VK_NULL_HANDLE)
        {
            auto vkCmdBuff = m_CmdPool.GetCommandBuffer();
            m_CommandBuffer.SetVkCmdBuffer(vkCmdBuff);
        }
    }

    inline void DisposeVkCmdBuffer(Uint32 CmdQueue, VkCommandBuffer vkCmdBuff, Uint64 FenceValue);
    inline void DisposeCurrentCmdBuffer(Uint32 CmdQueue, Uint64 FenceValue);

    void CopyBufferToTexture(VkBuffer                       vkSrcBuffer,
                             Uint32                         SrcBufferOffset,
                             Uint32                         SrcBufferRowStrideInTexels,
                             TextureVkImpl&                 DstTextureVk,
                             const Box&                     DstRegion,
                             Uint32                         DstMipLevel,
                             Uint32                         DstArraySlice,
                             RESOURCE_STATE_TRANSITION_MODE DstTextureTransitionMode);

    void CopyTextureToBuffer(TextureVkImpl&                 SrcTextureVk,
                             const Box&                     SrcRegion,
                             Uint32                         SrcMipLevel,
                             Uint32                         SrcArraySlice,
                             RESOURCE_STATE_TRANSITION_MODE SrcTextureTransitionMode,
                             VkBuffer                       vkDstBuffer,
                             Uint32                         DstBufferOffset,
                             Uint32                         DstBufferRowStrideInTexels);

    __forceinline void          PrepareForDraw(DRAW_FLAGS Flags);
    __forceinline void          PrepareForIndexedDraw(DRAW_FLAGS Flags, VALUE_TYPE IndexType);
    __forceinline BufferVkImpl* PrepareIndirectDrawAttribsBuffer(IBuffer* pAttribsBuffer, RESOURCE_STATE_TRANSITION_MODE TransitonMode);
    __forceinline void          PrepareForDispatchCompute();
    __forceinline void          PrepareForRayTracing();

    void DvpLogRenderPass_PSOMismatch();

    void CreateASCompactedSizeQueryPool();

    VulkanUtilities::VulkanCommandBuffer m_CommandBuffer;

    struct ContextState
    {
        /// Flag indicating if currently committed vertex buffers are up to date
        bool CommittedVBsUpToDate = false;

        /// Flag indicating if currently committed index buffer is up to date
        bool CommittedIBUpToDate = false;

        bool CommittedResourcesValidated = false;

        Uint32 NumCommands = 0;

        VkPipelineBindPoint vkPipelineBindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;
    } m_State;

    // Graphics/mesh, compute, ray tracing
    static constexpr Uint32 NUM_PIPELINE_BIND_POINTS = 3;

    static constexpr Uint32 MAX_DESCR_SET_PER_SIGNATURE = PipelineResourceSignatureVkImpl::MAX_DESCRIPTOR_SETS;

    struct DescriptorSetBindInfo
    {
        struct ResourceInfo
        {
            // The SRB's shader resource cache
            ShaderResourceCacheVk* pResourceCache = nullptr;

            // Static/mutable and dynamic descriptor sets
            std::array<VkDescriptorSet, MAX_DESCR_SET_PER_SIGNATURE> vkSets = {};

            // Descriptor set base index given by Layout.GetFirstDescrSetIndex
            Uint32 DescriptorSetBaseInd = 0;

            // The total number of descriptors with dynamic offset, given by pSignature->GetDynamicOffsetCount().
            // Note that this is not the actual number of dynamic buffers in the resource cache.
            Uint32 DynamicOffsetCount = 0;

#ifdef DILIGENT_DEVELOPMENT
            // The DescriptorSetBaseInd that was used in the last BindDescriptorSets() call
            Uint32 LastBoundDSBaseInd = ~0u;
#endif
        };
        std::array<ResourceInfo, MAX_RESOURCE_SIGNATURES> Resources;

#ifdef DILIGENT_DEVELOPMENT
        // Do not use strong references!
        std::array<ShaderResourceBindingVkImpl*, MAX_RESOURCE_SIGNATURES> SRBs = {};
#endif
        using Bitfield = Uint8;
        static_assert(sizeof(Bitfield) * 8 >= MAX_RESOURCE_SIGNATURES, "not enought space to store MAX_RESOURCE_SIGNATURES bits");

        Bitfield ActiveSRBMask      = 0; // Indicates which SRBs are active in current PSO
        Bitfield StaleSRBMask       = 0; // Indicates stale SRBs that have descriptor sets that need to be bound
        Bitfield DynamicBuffersMask = 0; // Indicates which SRBs have dynamic buffers

        // Pipeline layout of the currently bound pipeline
        VkPipelineLayout vkPipelineLayout = VK_NULL_HANDLE;

        DescriptorSetBindInfo()
        {}

        __forceinline bool RequireUpdate(bool DynamicBuffersIntact = false) const
        {
            return (StaleSRBMask & ActiveSRBMask) != 0 || ((DynamicBuffersMask & ActiveSRBMask) != 0 && !DynamicBuffersIntact);
        }

        void SetStaleSRBBit(Uint32 Index) { StaleSRBMask |= static_cast<Bitfield>(1u << Index); }
        void ClearStaleSRBBit(Uint32 Index) { StaleSRBMask &= static_cast<Bitfield>(~(1u << Index)); }

        void SetDynamicBufferBit(Uint32 Index) { DynamicBuffersMask |= static_cast<Bitfield>(1u << Index); }
        void ClearDynamicBufferBit(Uint32 Index) { DynamicBuffersMask &= static_cast<Bitfield>(~(1u << Index)); }
    };

    __forceinline DescriptorSetBindInfo& GetDescriptorSetBindInfo(PIPELINE_TYPE Type);

    __forceinline void CommitDescriptorSets(DescriptorSetBindInfo& DescrSetBindInfo);
#ifdef DILIGENT_DEVELOPMENT
    void DvpValidateCommittedShaderResources();
#endif

    /// Descriptor set binding information for each pipeline type (graphics/mesh, compute, ray tracing)
    std::array<DescriptorSetBindInfo, NUM_PIPELINE_BIND_POINTS> m_DescrSetBindInfo;

    /// Memory to store dynamic buffer offsets for descriptor sets.
    std::vector<Uint32> m_DynamicBufferOffsets;

    /// Render pass that matches currently bound render targets.
    /// This render pass may or may not be currently set in the command buffer
    VkRenderPass m_vkRenderPass = VK_NULL_HANDLE;

    /// Framebuffer that matches currently bound render targets.
    /// This framebuffer may or may not be currently set in the command buffer
    VkFramebuffer m_vkFramebuffer = VK_NULL_HANDLE;

    FixedBlockMemoryAllocator m_CmdListAllocator;

    // Semaphores are not owned by the command context
    std::vector<RefCntAutoPtr<ManagedSemaphore>> m_WaitSemaphores;
    std::vector<VkPipelineStageFlags>            m_WaitDstStageMasks;
    std::vector<RefCntAutoPtr<ManagedSemaphore>> m_SignalSemaphores;

    std::vector<VkSemaphore> m_VkWaitSemaphores;
    std::vector<VkSemaphore> m_VkSignalSemaphores;

    // List of fences to signal next time the command context is flushed
    std::vector<std::pair<Uint64, RefCntAutoPtr<IFence>>> m_PendingFences;

    std::unordered_map<BufferVkImpl*, VulkanUploadAllocation> m_UploadAllocations;

    struct MappedTextureKey
    {
        TextureVkImpl* const Texture;
        Uint32 const         MipLevel;
        Uint32 const         ArraySlice;

        bool operator==(const MappedTextureKey& rhs) const
        {
            return Texture == rhs.Texture &&
                MipLevel == rhs.MipLevel &&
                ArraySlice == rhs.ArraySlice;
        }
        struct Hasher
        {
            size_t operator()(const MappedTextureKey& Key) const
            {
                return ComputeHash(Key.Texture, Key.MipLevel, Key.ArraySlice);
            }
        };
    };
    struct MappedTexture
    {
        BufferToTextureCopyInfo CopyInfo;
        VulkanDynamicAllocation Allocation;
    };
    std::unordered_map<MappedTextureKey, MappedTexture, MappedTextureKey::Hasher> m_MappedTextures;

    VulkanUtilities::VulkanCommandBufferPool m_CmdPool;
    VulkanUploadHeap                         m_UploadHeap;
    VulkanDynamicHeap                        m_DynamicHeap;
    DynamicDescriptorSetAllocator            m_DynamicDescrSetAllocator;

    std::shared_ptr<GenerateMipsVkHelper> m_GenerateMipsHelper;
    RefCntAutoPtr<IShaderResourceBinding> m_GenerateMipsSRB;

    // In Vulkan we can't bind null vertex buffer, so we have to create a dummy VB
    RefCntAutoPtr<BufferVkImpl> m_DummyVB;

    std::unique_ptr<QueryManagerVk> m_QueryMgr;
    Int32                           m_ActiveQueriesCounter = 0;

    std::vector<VkClearValue> m_vkClearValues;

    VulkanUtilities::QueryPoolWrapper m_ASQueryPool;
};

} // namespace Diligent

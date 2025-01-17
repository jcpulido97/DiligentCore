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

#include "pch.h"

#include "ShaderVariableManagerVk.hpp"
#include "RenderDeviceVkImpl.hpp"
#include "PipelineResourceSignatureVkImpl.hpp"
#include "SamplerVkImpl.hpp"
#include "TextureViewVkImpl.hpp"
#include "TopLevelASVkImpl.hpp"

namespace Diligent
{

template <typename HandlerType>
void ProcessSignatureResources(const PipelineResourceSignatureVkImpl& Signature,
                               const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                               Uint32                                 NumAllowedTypes,
                               SHADER_TYPE                            ShaderStages,
                               HandlerType                            Handler)
{
    const bool UsingSeparateSamplers = Signature.IsUsingSeparateSamplers();
    Signature.ProcessResources(AllowedVarTypes, NumAllowedTypes, ShaderStages,
                               [&](const PipelineResourceDesc& ResDesc, Uint32 Index) //
                               {
                                   const auto& ResAttr = Signature.GetResourceAttribs(Index);

                                   // When using HLSL-style combined image samplers, we need to skip separate samplers.
                                   // Also always skip immutable separate samplers.
                                   if (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER &&
                                       (!UsingSeparateSamplers || ResAttr.IsImmutableSamplerAssigned()))
                                       return;

                                   Handler(Index);
                               });
}

size_t ShaderVariableManagerVk::GetRequiredMemorySize(const PipelineResourceSignatureVkImpl& Signature,
                                                      const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                                                      Uint32                                 NumAllowedTypes,
                                                      SHADER_TYPE                            ShaderStages,
                                                      Uint32&                                NumVariables)
{
    NumVariables = 0;
    ProcessSignatureResources(Signature, AllowedVarTypes, NumAllowedTypes, ShaderStages,
                              [&NumVariables](Uint32) //
                              {
                                  ++NumVariables;
                              });

    return NumVariables * sizeof(ShaderVariableVkImpl);
}

// Creates shader variable for every resource from SrcLayout whose type is one AllowedVarTypes
void ShaderVariableManagerVk::Initialize(const PipelineResourceSignatureVkImpl& Signature,
                                         IMemoryAllocator&                      Allocator,
                                         const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                                         Uint32                                 NumAllowedTypes,
                                         SHADER_TYPE                            ShaderType)
{
#ifdef DILIGENT_DEBUG
    m_pDbgAllocator = &Allocator;
#endif

    VERIFY_EXPR(m_pSignature == nullptr);

    VERIFY_EXPR(m_NumVariables == 0);
    const auto MemSize = GetRequiredMemorySize(Signature, AllowedVarTypes, NumAllowedTypes, ShaderType, m_NumVariables);

    if (m_NumVariables == 0)
        return;

    auto* pRawMem = ALLOCATE_RAW(Allocator, "Raw memory buffer for shader variables", MemSize);
    m_pVariables  = reinterpret_cast<ShaderVariableVkImpl*>(pRawMem);

    Uint32 VarInd = 0;
    ProcessSignatureResources(Signature, AllowedVarTypes, NumAllowedTypes, ShaderType,
                              [this, &VarInd](Uint32 ResIndex) //
                              {
                                  ::new (m_pVariables + VarInd) ShaderVariableVkImpl{*this, ResIndex};
                                  ++VarInd;
                              });
    VERIFY_EXPR(VarInd == m_NumVariables);

    m_pSignature = &Signature;
}

ShaderVariableManagerVk::~ShaderVariableManagerVk()
{
    VERIFY(m_pVariables == nullptr, "Destroy() has not been called");
}

void ShaderVariableManagerVk::Destroy(IMemoryAllocator& Allocator)
{
    if (m_pVariables != nullptr)
    {
        VERIFY(m_pDbgAllocator == &Allocator, "Incosistent alloctor");

        for (Uint32 v = 0; v < m_NumVariables; ++v)
            m_pVariables[v].~ShaderVariableVkImpl();
        Allocator.Free(m_pVariables);
        m_pVariables = nullptr;
    }
}

ShaderVariableVkImpl* ShaderVariableManagerVk::GetVariable(const Char* Name) const
{
    for (Uint32 v = 0; v < m_NumVariables; ++v)
    {
        auto& Var = m_pVariables[v];
        if (strcmp(Var.GetDesc().Name, Name) == 0)
            return &Var;
    }
    return nullptr;
}


ShaderVariableVkImpl* ShaderVariableManagerVk::GetVariable(Uint32 Index) const
{
    if (Index >= m_NumVariables)
    {
        LOG_ERROR("Index ", Index, " is out of range");
        return nullptr;
    }

    return m_pVariables + Index;
}

Uint32 ShaderVariableManagerVk::GetVariableIndex(const ShaderVariableVkImpl& Variable)
{
    if (m_pVariables == nullptr)
    {
        LOG_ERROR("This shader variable manager has no variables");
        return ~0u;
    }

    const auto Offset = reinterpret_cast<const Uint8*>(&Variable) - reinterpret_cast<Uint8*>(m_pVariables);
    DEV_CHECK_ERR(Offset % sizeof(ShaderVariableVkImpl) == 0, "Offset is not multiple of ShaderVariableVkImpl class size");
    const auto Index = static_cast<Uint32>(Offset / sizeof(ShaderVariableVkImpl));
    if (Index < m_NumVariables)
        return Index;
    else
    {
        LOG_ERROR("Failed to get variable index. The variable ", &Variable, " does not belong to this shader variable manager");
        return ~0u;
    }
}

const PipelineResourceDesc& ShaderVariableManagerVk::GetResourceDesc(Uint32 Index) const
{
    VERIFY_EXPR(m_pSignature);
    return m_pSignature->GetResourceDesc(Index);
}

const ShaderVariableManagerVk::ResourceAttribs& ShaderVariableManagerVk::GetAttribs(Uint32 Index) const
{
    VERIFY_EXPR(m_pSignature);
    return m_pSignature->GetResourceAttribs(Index);
}


void ShaderVariableManagerVk::BindResources(IResourceMapping* pResourceMapping, Uint32 Flags) const
{
    if (!pResourceMapping)
    {
        LOG_ERROR_MESSAGE("Failed to bind resources: resource mapping is null");
        return;
    }

    if ((Flags & BIND_SHADER_RESOURCES_UPDATE_ALL) == 0)
        Flags |= BIND_SHADER_RESOURCES_UPDATE_ALL;

    for (Uint32 v = 0; v < m_NumVariables; ++v)
    {
        m_pVariables[v].BindResources(pResourceMapping, Flags);
    }
}


namespace
{

inline BUFFER_VIEW_TYPE DescriptorTypeToBufferView(DescriptorType Type)
{
    static_assert(static_cast<Uint32>(DescriptorType::Count) == 15, "Please update the switch below to handle the new descriptor type");
    switch (Type)
    {
        case DescriptorType::UniformTexelBuffer:
        case DescriptorType::StorageTexelBuffer_ReadOnly:
        case DescriptorType::StorageBuffer_ReadOnly:
        case DescriptorType::StorageBufferDynamic_ReadOnly:
            return BUFFER_VIEW_SHADER_RESOURCE;

        case DescriptorType::StorageTexelBuffer:
        case DescriptorType::StorageBuffer:
        case DescriptorType::StorageBufferDynamic:
            return BUFFER_VIEW_UNORDERED_ACCESS;

        default:
            UNEXPECTED("Unsupported descriptor type for buffer view");
            return BUFFER_VIEW_UNDEFINED;
    }
}

inline TEXTURE_VIEW_TYPE DescriptorTypeToTextureView(DescriptorType Type)
{
    static_assert(static_cast<Uint32>(DescriptorType::Count) == 15, "Please update the switch below to handle the new descriptor type");
    switch (Type)
    {
        case DescriptorType::StorageImage:
            return TEXTURE_VIEW_UNORDERED_ACCESS;

        case DescriptorType::CombinedImageSampler:
        case DescriptorType::SeparateImage:
        case DescriptorType::InputAttachment:
            return TEXTURE_VIEW_SHADER_RESOURCE;

        default:
            UNEXPECTED("Unsupported descriptor type for texture view");
            return TEXTURE_VIEW_UNDEFINED;
    }
}


struct BindResourceHelper
{
    BindResourceHelper(const PipelineResourceSignatureVkImpl& Signature,
                       ShaderResourceCacheVk&                 ResourceCache,
                       Uint32                                 ResIndex,
                       Uint32                                 ArrayIndex);

    void operator()(IDeviceObject* pObj) const;

private:
    void CacheUniformBuffer(IDeviceObject* pBuffer) const;

    void CacheStorageBuffer(IDeviceObject* pBufferView) const;

    void CacheTexelBuffer(IDeviceObject* pBufferView) const;

    void CacheImage(IDeviceObject* pTexView) const;

    void CacheSeparateSampler(IDeviceObject* pSampler) const;

    void CacheInputAttachment(IDeviceObject* pTexView) const;

    void CacheAccelerationStructure(IDeviceObject* pTLAS) const;

    template <typename ObjectType>
    bool UpdateCachedResource(RefCntAutoPtr<ObjectType>&& pObject) const;

    // Updates resource descriptor in the descriptor set
    inline void UpdateDescriptorHandle(const VkDescriptorImageInfo*                        pImageInfo,
                                       const VkDescriptorBufferInfo*                       pBufferInfo,
                                       const VkBufferView*                                 pTexelBufferView,
                                       const VkWriteDescriptorSetAccelerationStructureKHR* pAccelStructInfo = nullptr) const;

private:
    using ResourceAttribs = PipelineResourceSignatureVkImpl::ResourceAttribs;
    using CachedSet       = ShaderResourceCacheVk::DescriptorSet;

    const PipelineResourceSignatureVkImpl& m_Signature;
    ShaderResourceCacheVk&                 m_ResourceCache;
    const Uint32                           m_ArrayIndex;
    const ResourceCacheContentType         m_CacheType;
    const PipelineResourceDesc&            m_ResDesc;
    const ResourceAttribs&                 m_Attribs;
    const Uint32                           m_DstResCacheOffset;
    const CachedSet&                       m_CachedSet;
    const ShaderResourceCacheVk::Resource& m_DstRes;
};

BindResourceHelper::BindResourceHelper(const PipelineResourceSignatureVkImpl& Signature,
                                       ShaderResourceCacheVk&                 ResourceCache,
                                       Uint32                                 ResIndex,
                                       Uint32                                 ArrayIndex) :
    // clang-format off
    m_Signature         {Signature},
    m_ResourceCache     {ResourceCache},
    m_ArrayIndex        {ArrayIndex},
    m_CacheType         {ResourceCache.GetContentType()},
    m_ResDesc           {Signature.GetResourceDesc(ResIndex)},
    m_Attribs           {Signature.GetResourceAttribs(ResIndex)},
    m_DstResCacheOffset {m_Attribs.CacheOffset(m_CacheType) + ArrayIndex},
    m_CachedSet         {const_cast<const ShaderResourceCacheVk&>(ResourceCache).GetDescriptorSet(m_Attribs.DescrSet)},
    m_DstRes            {m_CachedSet.GetResource(m_DstResCacheOffset)}
// clang-format on
{
    VERIFY(ArrayIndex < m_ResDesc.ArraySize, "Array index is out of range, but it should've been corrected by VerifyAndCorrectSetArrayArguments()");
    VERIFY(m_DstRes.Type == m_Attribs.GetDescriptorType(), "Inconsistent types");

#ifdef DILIGENT_DEBUG
    {
        auto vkDescrSet = m_CachedSet.GetVkDescriptorSet();
        if (m_CacheType == ResourceCacheContentType::SRB)
        {
            if (m_ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC ||
                m_ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
            {
                VERIFY(vkDescrSet != VK_NULL_HANDLE, "Static and mutable variables must have a valid Vulkan descriptor set assigned");
            }
            else
            {
                VERIFY(vkDescrSet == VK_NULL_HANDLE, "Dynamic variables must never have valid Vulkan descriptor set assigned");
            }
        }
        else if (m_CacheType == ResourceCacheContentType::Signature)
        {
            VERIFY(vkDescrSet == VK_NULL_HANDLE, "Static shader resource cache should not have Vulkan descriptor set allocation");
        }
        else
        {
            UNEXPECTED("Unexpected shader resource cache content type");
        }
    }
#endif
}

void BindResourceHelper::operator()(IDeviceObject* pObj) const
{
    if (pObj)
    {
        static_assert(static_cast<Uint32>(DescriptorType::Count) == 15, "Please update the switch below to handle the new descriptor type");
        switch (m_DstRes.Type)
        {
            case DescriptorType::UniformBuffer:
            case DescriptorType::UniformBufferDynamic:
                CacheUniformBuffer(pObj);
                break;

            case DescriptorType::StorageBuffer:
            case DescriptorType::StorageBuffer_ReadOnly:
            case DescriptorType::StorageBufferDynamic:
            case DescriptorType::StorageBufferDynamic_ReadOnly:
                CacheStorageBuffer(pObj);
                break;

            case DescriptorType::UniformTexelBuffer:
            case DescriptorType::StorageTexelBuffer:
            case DescriptorType::StorageTexelBuffer_ReadOnly:
                CacheTexelBuffer(pObj);
                break;

            case DescriptorType::StorageImage:
            case DescriptorType::SeparateImage:
            case DescriptorType::CombinedImageSampler:
                CacheImage(pObj);
                break;

            case DescriptorType::Sampler:
                if (!m_Attribs.IsImmutableSamplerAssigned())
                {
                    CacheSeparateSampler(pObj);
                }
                else
                {
                    // Immutable samplers are permanently bound into the set layout; later binding a sampler
                    // into an immutable sampler slot in a descriptor set is not allowed (13.2.1)
                    UNEXPECTED("Attempting to assign a sampler to an immutable sampler '", m_ResDesc.Name, '\'');
                }
                break;

            case DescriptorType::InputAttachment:
                CacheInputAttachment(pObj);
                break;

            case DescriptorType::AccelerationStructure:
                CacheAccelerationStructure(pObj);
                break;

            default: UNEXPECTED("Unknown resource type ", static_cast<Uint32>(m_DstRes.Type));
        }
    }
    else
    {
        if (m_DstRes.pObject && m_ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            LOG_ERROR_MESSAGE("Shader variable '", m_ResDesc.Name, "' is not dynamic, but is being reset to null. This is an error and may cause unpredicted behavior. ",
                              "Use another shader resource binding instance or label the variable as dynamic if you need to bind another resource.");
        }

        m_ResourceCache.ResetResource(m_Attribs.DescrSet, m_DstResCacheOffset);
    }
}

template <typename ObjectType>
bool BindResourceHelper::UpdateCachedResource(RefCntAutoPtr<ObjectType>&& pObject) const
{
    if (pObject)
    {
        if (m_ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && m_DstRes.pObject != nullptr)
        {
            // Do not update resource if one is already bound unless it is dynamic. This may be
            // dangerous as writing descriptors while they are used by the GPU is an undefined behavior
            return false;
        }

        m_ResourceCache.SetResource(&m_Signature.GetDevice()->GetLogicalDevice(),
                                    m_Attribs.DescrSet, m_DstResCacheOffset,
                                    m_Attribs.BindingIndex, m_ArrayIndex,
                                    std::move(pObject));
        return true;
    }
    else
    {
        return false;
    }
}

void BindResourceHelper::CacheUniformBuffer(IDeviceObject* pBuffer) const
{
    VERIFY((m_DstRes.Type == DescriptorType::UniformBuffer ||
            m_DstRes.Type == DescriptorType::UniformBufferDynamic),
           "Uniform buffer resource is expected");

    // We cannot use ValidatedCast<> here as the resource can have wrong type
    RefCntAutoPtr<BufferVkImpl> pBufferVk{pBuffer, IID_BufferVk};
#ifdef DILIGENT_DEVELOPMENT
    VerifyConstantBufferBinding(m_ResDesc, m_ArrayIndex, pBuffer, pBufferVk.RawPtr(), m_DstRes.pObject.RawPtr(),
                                m_Signature.GetDesc().Name);
#endif

    UpdateCachedResource(std::move(pBufferVk));
}

void BindResourceHelper::CacheStorageBuffer(IDeviceObject* pBufferView) const
{
    VERIFY((m_DstRes.Type == DescriptorType::StorageBuffer ||
            m_DstRes.Type == DescriptorType::StorageBuffer_ReadOnly ||
            m_DstRes.Type == DescriptorType::StorageBufferDynamic ||
            m_DstRes.Type == DescriptorType::StorageBufferDynamic_ReadOnly),
           "Storage buffer resource is expected");

    RefCntAutoPtr<BufferViewVkImpl> pBufferViewVk{pBufferView, IID_BufferViewVk};
#ifdef DILIGENT_DEVELOPMENT
    {
        // HLSL buffer SRVs are mapped to storge buffers in GLSL
        const auto RequiredViewType = DescriptorTypeToBufferView(m_DstRes.Type);
        VerifyResourceViewBinding(m_ResDesc, m_ArrayIndex,
                                  pBufferView, pBufferViewVk.RawPtr(),
                                  {RequiredViewType},
                                  RESOURCE_DIM_BUFFER, // Expected resource dim
                                  false,               // IsMultisample (ignored when resource dim is buffer)
                                  m_DstRes.pObject.RawPtr(),
                                  m_Signature.GetDesc().Name);

        VERIFY((m_ResDesc.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) == 0,
               "FORMATTED_BUFFER resource flag is set for a storage buffer - this should've not happened.");
        ValidateBufferMode(m_ResDesc, m_ArrayIndex, pBufferViewVk.RawPtr());
    }
#endif

    UpdateCachedResource(std::move(pBufferViewVk));
}

void BindResourceHelper::CacheTexelBuffer(IDeviceObject* pBufferView) const
{
    VERIFY((m_DstRes.Type == DescriptorType::UniformTexelBuffer ||
            m_DstRes.Type == DescriptorType::StorageTexelBuffer ||
            m_DstRes.Type == DescriptorType::StorageTexelBuffer_ReadOnly),
           "Uniform or storage buffer resource is expected");

    RefCntAutoPtr<BufferViewVkImpl> pBufferViewVk{pBufferView, IID_BufferViewVk};
#ifdef DILIGENT_DEVELOPMENT
    {
        // HLSL buffer SRVs are mapped to storge buffers in GLSL
        const auto RequiredViewType = DescriptorTypeToBufferView(m_DstRes.Type);
        VerifyResourceViewBinding(m_ResDesc, m_ArrayIndex,
                                  pBufferView, pBufferViewVk.RawPtr(),
                                  {RequiredViewType},
                                  RESOURCE_DIM_BUFFER, // Expected resource dim
                                  false,               // IsMultisample (ignored when resource dim is buffer)
                                  m_DstRes.pObject.RawPtr(),
                                  m_Signature.GetDesc().Name);

        VERIFY((m_ResDesc.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) != 0,
               "FORMATTED_BUFFER resource flag is not set for a texel buffer - this should've not happened.");
        ValidateBufferMode(m_ResDesc, m_ArrayIndex, pBufferViewVk.RawPtr());
    }
#endif

    UpdateCachedResource(std::move(pBufferViewVk));
}

void BindResourceHelper::CacheImage(IDeviceObject* pTexView) const
{
    VERIFY((m_DstRes.Type == DescriptorType::StorageImage ||
            m_DstRes.Type == DescriptorType::SeparateImage ||
            m_DstRes.Type == DescriptorType::CombinedImageSampler),
           "Storage image, separate image or sampled image resource is expected");

    RefCntAutoPtr<TextureViewVkImpl> pTexViewVk0{pTexView, IID_TextureViewVk};
#ifdef DILIGENT_DEVELOPMENT
    {
        // HLSL buffer SRVs are mapped to storge buffers in GLSL
        auto RequiredViewType = DescriptorTypeToTextureView(m_DstRes.Type);
        VerifyResourceViewBinding(m_ResDesc, m_ArrayIndex,
                                  pTexView, pTexViewVk0.RawPtr(),
                                  {RequiredViewType},
                                  RESOURCE_DIM_UNDEFINED, // Required resource dimension is not known
                                  false,                  // IsMultisample (ignored when resource dim is unknown)
                                  m_DstRes.pObject.RawPtr(),
                                  m_Signature.GetDesc().Name);
    }
#endif

    TextureViewVkImpl* pTexViewVk = pTexViewVk0;
    if (UpdateCachedResource(std::move(pTexViewVk0)))
    {
#ifdef DILIGENT_DEVELOPMENT
        if (m_DstRes.Type == DescriptorType::CombinedImageSampler && !m_Attribs.IsImmutableSamplerAssigned())
        {
            if (pTexViewVk->GetSampler() == nullptr)
            {
                LOG_ERROR_MESSAGE("Error binding texture view '", pTexViewVk->GetDesc().Name, "' to variable '",
                                  GetShaderResourcePrintName(m_ResDesc, m_ArrayIndex), "'. No sampler is assigned to the view");
            }
        }
#endif

        if (m_Attribs.IsCombinedWithSampler())
        {
            VERIFY(m_DstRes.Type == DescriptorType::SeparateImage,
                   "Only separate images can be assigned separate samplers when using HLSL-style combined samplers.");
            VERIFY(!m_Attribs.IsImmutableSamplerAssigned(), "Separate image can't be assigned an immutable sampler.");

            const auto& SamplerResDesc = m_Signature.GetResourceDesc(m_Attribs.SamplerInd);
            const auto& SamplerAttribs = m_Signature.GetResourceAttribs(m_Attribs.SamplerInd);
            VERIFY_EXPR(SamplerResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER);

            if (!SamplerAttribs.IsImmutableSamplerAssigned())
            {
                auto* pSampler = pTexViewVk->GetSampler();
                if (pSampler != nullptr)
                {
                    DEV_CHECK_ERR(SamplerResDesc.ArraySize == 1 || SamplerResDesc.ArraySize == m_ResDesc.ArraySize,
                                  "Array size (", SamplerResDesc.ArraySize,
                                  ") of separate sampler variable '",
                                  SamplerResDesc.Name,
                                  "' must be one or the same as the array size (", m_ResDesc.ArraySize,
                                  ") of separate image variable '", m_ResDesc.Name, "' it is assigned to");

                    BindResourceHelper BindSeparateSamler{
                        m_Signature,
                        m_ResourceCache,
                        m_Attribs.SamplerInd,
                        SamplerResDesc.ArraySize == 1 ? 0 : m_ArrayIndex};
                    BindSeparateSamler(pSampler);
                }
                else
                {
                    LOG_ERROR_MESSAGE("Failed to bind sampler to sampler variable '", SamplerResDesc.Name,
                                      "' assigned to separate image '", GetShaderResourcePrintName(m_ResDesc, m_ArrayIndex),
                                      "': no sampler is set in texture view '", pTexViewVk->GetDesc().Name, '\'');
                }
            }
        }
    }
}

void BindResourceHelper::CacheSeparateSampler(IDeviceObject* pSampler) const
{
    VERIFY(m_DstRes.Type == DescriptorType::Sampler, "Separate sampler resource is expected");
    VERIFY(!m_Attribs.IsImmutableSamplerAssigned(), "This separate sampler is assigned an immutable sampler");

    RefCntAutoPtr<SamplerVkImpl> pSamplerVk{pSampler, IID_Sampler};
#ifdef DILIGENT_DEVELOPMENT
    if (pSampler != nullptr && pSamplerVk == nullptr)
    {
        LOG_ERROR_MESSAGE("Failed to bind object '", pSampler->GetDesc().Name, "' to variable '",
                          GetShaderResourcePrintName(m_ResDesc, m_ArrayIndex), "'. Unexpected object type: sampler is expected");
    }
    if (m_ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && m_DstRes.pObject != nullptr && m_DstRes.pObject != pSamplerVk)
    {
        auto VarTypeStr = GetShaderVariableTypeLiteralName(m_ResDesc.VarType);
        LOG_ERROR_MESSAGE("Non-null sampler is already bound to ", VarTypeStr, " shader variable '", GetShaderResourcePrintName(m_ResDesc, m_ArrayIndex),
                          "'. Attempting to bind another sampler or null is an error and may "
                          "cause unpredicted behavior. Use another shader resource binding instance or label the variable as dynamic.");
    }
#endif

    UpdateCachedResource(std::move(pSamplerVk));
}

void BindResourceHelper::CacheInputAttachment(IDeviceObject* pTexView) const
{
    VERIFY(m_DstRes.Type == DescriptorType::InputAttachment, "Input attachment resource is expected");
    RefCntAutoPtr<TextureViewVkImpl> pTexViewVk{pTexView, IID_TextureViewVk};
#ifdef DILIGENT_DEVELOPMENT
    VerifyResourceViewBinding(m_ResDesc, m_ArrayIndex,
                              pTexView, pTexViewVk.RawPtr(),
                              {TEXTURE_VIEW_SHADER_RESOURCE},
                              RESOURCE_DIM_UNDEFINED,
                              false, // IsMultisample
                              m_DstRes.pObject.RawPtr(),
                              m_Signature.GetDesc().Name);
#endif

    UpdateCachedResource(std::move(pTexViewVk));
}

void BindResourceHelper::CacheAccelerationStructure(IDeviceObject* pTLAS) const
{
    VERIFY(m_DstRes.Type == DescriptorType::AccelerationStructure, "Acceleration Structure resource is expected");
    RefCntAutoPtr<TopLevelASVkImpl> pTLASVk{pTLAS, IID_TopLevelASVk};
#ifdef DILIGENT_DEVELOPMENT
    VerifyTLASResourceBinding(m_ResDesc, m_ArrayIndex, pTLAS, pTLASVk.RawPtr(), m_DstRes.pObject.RawPtr(),
                              m_Signature.GetDesc().Name);
#endif

    UpdateCachedResource(std::move(pTLASVk));
}

} // namespace


void ShaderVariableManagerVk::BindResource(IDeviceObject* pObj, Uint32 ArrayIndex, Uint32 ResIndex)
{
    BindResourceHelper BindHelper{
        *m_pSignature,
        m_ResourceCache,
        ResIndex,
        ArrayIndex};

    BindHelper(pObj);
}

bool ShaderVariableManagerVk::IsBound(Uint32 ArrayIndex, Uint32 ResIndex) const
{
    const auto&  ResDesc     = GetResourceDesc(ResIndex);
    const auto&  Attribs     = GetAttribs(ResIndex);
    const Uint32 CacheOffset = Attribs.CacheOffset(m_ResourceCache.GetContentType());

    VERIFY_EXPR(ArrayIndex < ResDesc.ArraySize);

    if (Attribs.DescrSet < m_ResourceCache.GetNumDescriptorSets())
    {
        const auto& Set = const_cast<const ShaderResourceCacheVk&>(m_ResourceCache).GetDescriptorSet(Attribs.DescrSet);
        if (CacheOffset + ArrayIndex < Set.GetSize())
        {
            const auto& CachedRes = Set.GetResource(CacheOffset + ArrayIndex);
            return !CachedRes.IsNull();
        }
    }

    return false;
}

} // namespace Diligent

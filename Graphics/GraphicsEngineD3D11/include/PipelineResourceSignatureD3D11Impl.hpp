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
/// Declaration of Diligent::PipelineResourceSignatureD3D11Impl class

#include <array>

#include "EngineD3D11ImplTraits.hpp"
#include "PipelineResourceSignatureBase.hpp"
#include "PipelineResourceAttribsD3D11.hpp"

// ShaderVariableManagerD3D11, ShaderResourceCacheD3D11, and ShaderResourceBindingD3D11Impl
// are required by PipelineResourceSignatureBase
#include "ShaderResourceCacheD3D11.hpp"
#include "ShaderVariableManagerD3D11.hpp"
#include "ShaderResourceBindingD3D11Impl.hpp"
#include "SamplerD3D11Impl.hpp"

#include "ResourceBindingMap.hpp"

namespace Diligent
{

/// Implementation of the Diligent::PipelineResourceSignatureD3D11Impl class
class PipelineResourceSignatureD3D11Impl final : public PipelineResourceSignatureBase<EngineD3D11ImplTraits>
{
public:
    using TPipelineResourceSignatureBase = PipelineResourceSignatureBase<EngineD3D11ImplTraits>;

    PipelineResourceSignatureD3D11Impl(IReferenceCounters*                  pRefCounters,
                                       RenderDeviceD3D11Impl*               pDevice,
                                       const PipelineResourceSignatureDesc& Desc,
                                       bool                                 bIsDeviceInternal = false);
    ~PipelineResourceSignatureD3D11Impl();

    using ResourceAttribs = PipelineResourceAttribsD3D11;

    const ResourceAttribs& GetResourceAttribs(Uint32 ResIndex) const
    {
        VERIFY_EXPR(ResIndex < m_Desc.NumResources);
        return m_pResourceAttribs[ResIndex];
    }

    // sizeof(ImmutableSamplerAttribs) == 24, x64
    struct ImmutableSamplerAttribs
    {
    public:
        RefCntAutoPtr<SamplerD3D11Impl> pSampler;
        Uint32                          ArraySize = 1;
        D3D11ResourceBindPoints         BindPoints;

        ImmutableSamplerAttribs() noexcept {}

        bool IsAllocated() const { return !BindPoints.IsEmpty(); }
    };

    const ImmutableSamplerAttribs& GetImmutableSamplerAttribs(Uint32 SampIndex) const
    {
        VERIFY_EXPR(SampIndex < m_Desc.NumImmutableSamplers);
        return m_ImmutableSamplers[SampIndex];
    }

    // Shifts resource bindings by the number of resources in each shader stage and resource range.
    __forceinline void ShiftBindings(D3D11ShaderResourceCounters& Bindings) const
    {
        for (Uint32 r = 0; r < D3D11_RESOURCE_RANGE_COUNT; ++r)
            Bindings[r] += m_ResourceCounters[r];
    }

    void InitSRBResourceCache(ShaderResourceCacheD3D11& ResourceCache);

    void UpdateShaderResourceBindingMap(ResourceBinding::TMap& ResourceMap, SHADER_TYPE ShaderStage, const D3D11ShaderResourceCounters& BaseBindings) const;

    // Copies static resources from the static resource cache to the destination cache
    void CopyStaticResources(ShaderResourceCacheD3D11& ResourceCache) const;

#ifdef DILIGENT_DEVELOPMENT
    /// Verifies committed resource using the D3D resource attributes from the PSO.
    bool DvpValidateCommittedResource(const D3DShaderResourceAttribs& D3DAttribs,
                                      Uint32                          ResIndex,
                                      const ShaderResourceCacheD3D11& ResourceCache,
                                      const char*                     ShaderName,
                                      const char*                     PSOName) const;
#endif

private:
    void CreateLayout();

    void Destruct();

private:
    D3D11ShaderResourceCounters m_ResourceCounters  = {};
    ResourceAttribs*            m_pResourceAttribs  = nullptr; // [m_Desc.NumResources]
    ImmutableSamplerAttribs*    m_ImmutableSamplers = nullptr; // [m_Desc.NumImmutableSamplers]
};

} // namespace Diligent

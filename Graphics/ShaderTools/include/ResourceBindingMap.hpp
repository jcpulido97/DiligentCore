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

#include <unordered_map>

#include "Constants.h"
#include "HashUtils.hpp"

namespace Diligent
{

struct ResourceBinding
{
    struct BindInfo
    {
        // new bind point & space
        Uint32 BindPoint = ~0u;
        Uint32 Space     = ~0u;

        // current (previous) bind point & space
        mutable Uint32 SrcBindPoint = ~0u;
        mutable Uint32 SrcSpace     = ~0u;

        BindInfo() {}

        BindInfo(Uint32 _BindPoint, Uint32 _Space) :
            BindPoint{_BindPoint}, Space{_Space}
        {}
    };

    /// A mapping from the resource name to the binding (shader register).
    using TMap = std::unordered_map<HashMapStringKey, BindInfo, HashMapStringKey::Hasher>;
};

} // namespace Diligent
